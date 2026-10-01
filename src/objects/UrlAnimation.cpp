// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "objects/UrlAnimation.h"
#include <QDate>
#include <QRegularExpression>
#include <QtConcurrent/QtConcurrent>
#include "objects/DownloadParallelBytes.h"
#include "objects/FutureVoid.h"
#include "objects/UtilityAnimationExport.h"
#include "util/To.h"

namespace {
    constexpr int frameDelayMs = 400;   // per-frame dwell for the exported APNG, matches AnimationBar

    // A readable label per frame from the digits in the URL's file name
    // (GOES: 11 digits yyyydddhhmm; SPC meso: 8 digits yymmddhh). Falls back
    // to the frame number.
    string labelFor(const string& url, size_t index) {
        const auto name = QString::fromStdString(url).section('/', -1);
        const auto goes = QRegularExpression{R"((\d{4})(\d{3})(\d{2})(\d{2}))"}.match(name);
        if (goes.hasMatch() && name.indexOf(goes.captured(0)) == 0) {
            // day-of-year -> month/day
            const auto date = QDate{goes.captured(1).toInt(), 1, 1}.addDays(goes.captured(2).toInt() - 1);
            return (date.toString("MM/dd") + " " + goes.captured(3) + ":" + goes.captured(4) + "Z").toStdString();
        }
        const auto spc = QRegularExpression{R"(_(\d{2})(\d{2})(\d{2})(\d{2})\.)"}.match(name);
        if (spc.hasMatch()) {
            return (spc.captured(2) + "/" + spc.captured(3) + " " + spc.captured(4) + "Z").toStdString();
        }
        return To::string(static_cast<int>(index) + 1);
    }
}

UrlAnimation::UrlAnimation(Window * parent, ZoomImage * image,
                           const function<vector<string>(string, string, int)>& getFunction)
    : getFunction{getFunction}
    , parent{parent}
    , image{image}
    , animBar{parent,
        [this] (int start, int end) { onRangeRequested(start, end); },
        [this] (int local) {
            const auto bytes = animBar.frameAt(local);
            if (!bytes.isEmpty()) {
                this->image->setBytesKeepView(bytes);
                currentBytes = bytes;
            }
        },
        [this] (int global) { onScrub(global); },
        [this] { onSave(); }}
{}

void UrlAnimation::addTo(VBox& box) {
    animBar.addTo(box);
}

void UrlAnimation::setFrameCount(int count) {
    frameCount = count;
    refresh();
}

void UrlAnimation::setCurrentBytes(const QByteArray& bytes) {
    currentBytes = bytes;
}

void UrlAnimation::stopAnimateNoDownload() {
    animBar.stopIfAnimating();
}

void UrlAnimation::clear() {
    generation += 1;
    urls.clear();
    animBar.stopIfAnimating();
    animBar.clearFrames();
    animBar.setAvailableLabels({});
}

void UrlAnimation::refresh() {
    generation += 1;
    animBar.stopIfAnimating();
    animBar.clearFrames();
    const auto thisGeneration = generation;
    const auto wantedProduct = product;
    const auto wantedSector = sector;
    const auto count = frameCount;
    new FutureVoid{parent,
        [this, wantedProduct, wantedSector, count] { pendingUrls = getFunction(wantedProduct, wantedSector, count); },
        [this, thisGeneration] {
            if (thisGeneration != generation) {
                return;   // product / sector changed again while this was loading
            }
            urls = pendingUrls;
            vector<string> labels;
            for (size_t i = 0; i < urls.size(); i += 1) {
                labels.push_back(labeler ? labeler(urls[i], i) : labelFor(urls[i], i));
            }
            animBar.setAvailableLabels(labels);
            if (!urls.empty()) {
                animBar.setSliderPosition(static_cast<int>(urls.size()) - 1);
            }
        }};
}

void UrlAnimation::onRangeRequested(int start, int end) {
    const auto thisGeneration = generation;
    vector<string> subset;
    vector<int> indices;
    for (int i = start; i <= end && i < static_cast<int>(urls.size()); i += 1) {
        subset.push_back(urls[i]);
        indices.push_back(i);
    }
    if (subset.size() < 2) {
        animBar.cancelPending();
        return;
    }
    new FutureVoid{parent,
        [this, subset, indices] {
            pendingFrames.clear();
            pendingIndices.clear();
            QList<QByteArray> frames;
            if (frameFetcher) {
                // one task per frame on the shared thread pool (header-only QtConcurrent::run, no extra library)
                const auto fetcher = frameFetcher;
                QList<QFuture<QByteArray>> pending;
                for (const auto& url : subset) {
                    pending.append(QtConcurrent::run([fetcher, url] { return fetcher(url); }));
                }
                for (auto& future : pending) {
                    frames.append(future.result());
                }
            } else {
                const auto downloaded = DownloadParallelBytes{subset}.byteList;
                frames = QList<QByteArray>(downloaded.begin(), downloaded.end());
            }
            pendingMissing = 0;
            for (size_t i = 0; i < static_cast<size_t>(frames.size()) && i < indices.size(); i += 1) {
                if (frames[i].isEmpty()) {
                    pendingMissing += 1;
                }
                if (!frames[i].isEmpty()) {
                    pendingFrames.push_back(frames[i]);
                    pendingIndices.push_back(indices[i]);
                }
            }
        },
        [this, thisGeneration, subset] {
            if (thisGeneration != generation) {
                return;
            }
            if (pendingMissing > 0 && onFailure) {
                onFailure(To::string(pendingMissing) + " of " + To::string(static_cast<int>(subset.size())) +
                          " frames could not be fetched" + (pendingFrames.size() < 2 ? " - no loop to play" : " and were skipped"));
            }
            if (pendingFrames.size() < 2) {
                animBar.cancelPending();
                return;
            }
            animBar.setFrames(pendingFrames, pendingIndices);
        }};
}

void UrlAnimation::onScrub(int globalIndex) {
    if (globalIndex < 0 || globalIndex >= static_cast<int>(urls.size())) {
        return;
    }
    const auto thisGeneration = generation;
    const auto url = urls[globalIndex];
    new FutureVoid{parent,
        [this, url] { pendingSingle = frameFetcher ? frameFetcher(url) : DownloadParallelBytes{{url}}.byteList.front(); },
        [this, thisGeneration, globalIndex] {
            if (thisGeneration != generation) {
                return;
            }
            if (pendingSingle.isEmpty()) {
                if (onFailure) {
                    onFailure("that frame could not be fetched");
                }
                return;
            }
            image->setBytesKeepView(pendingSingle);
            currentBytes = pendingSingle;
            animBar.setSliderPosition(globalIndex);
        }};
}

void UrlAnimation::onSave() {
    const auto looping = animBar.frameCount() >= 2;
    if (!looping && currentBytes.isEmpty()) {
        return;
    }
    const auto suggested = product + "_" + (looping ? string{"anim"} : string{"latest"});
    UtilityAnimationExport::saveWithDialog(parent, looping ? animBar.loadedFrames() : vector<QByteArray>{},
                                           frameDelayMs, currentBytes, QString::fromStdString(suggested));
}
