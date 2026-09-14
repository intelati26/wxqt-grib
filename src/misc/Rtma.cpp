// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "Rtma.h"
#include <algorithm>
#include <QFile>
#include <QFileDialog>
#include <QStandardPaths>
#include "misc/ImageViewer.h"
#include "misc/UtilityRtma.h"
#include "objects/DownloadParallelBytes.h"
#include "objects/FutureBytes.h"
#include "objects/FutureVoid.h"
#include "objects/ObjectDateTime.h"
#include "objects/UtilityApng.h"
#include "objects/UtilityJxl.h"
#include "settings/Location.h"
#include "util/Utility.h"

namespace {
    constexpr int frameDelayMs = 350;   // per-frame dwell for playback and the APNG
}

Rtma::Rtma(Window * parent)
    : Window{parent}
    , image{this}
    , comboboxProduct{this, UtilityRtma::codes}
    , comboboxSector{this, UtilityRtma::sectors}
    , comboboxTimes{this}
    , backForward{this, [this] { moveBack(); }, [this] { moveForward(); }}
    , animBar{this,
        [this] (int start, int end) { onRangeRequested(start, end); },
        [this] (int local) { onFrameShown(local); },
        [this] (int global) { onScrub(global); },
        [this] { onSave(); }}
    , index{Utility::readPrefInt(prefToken, 0)}
{
    setTitle("RTMA");
    index = std::clamp(index, 0, static_cast<int>(UtilityRtma::codes.size()) - 1);

    times = UtilityRtma::getTimes();
    comboboxTimes.setList(times);
    comboboxTimes.setIndex(0);

    comboboxProduct.setIndexByValue(UtilityRtma::codes[index]);
    comboboxProduct.connect([this] { changeProduct(); });
    comboboxSector.setIndexByValue(UtilityRtma::getNearest(Location::getLatLonCurrent()));
    comboboxSector.connect([this] { changeSector(); });
    comboboxTimes.connect([this] { changeTime(); });
    QObject::connect(&image, &ZoomImage::doubleClicked, this, [this] { openFullImage(); });

    hbox.addWidget(comboboxProduct);
    hbox.addWidget(comboboxSector);
    hbox.addWidget(comboboxTimes);
    hbox.addLayout(backForward);
    hbox.addStretch();
    box.addLayout(hbox);
    animBar.addTo(box);
    box.addWidgetReal(&image, 1, Qt::Alignment{});
    box.getAndShow(this);

    // the bar works in chronological (oldest-first) order; `times` is newest-first
    vector<string> chronological{times.rbegin(), times.rend()};
    animBar.setAvailableLabels(chronological);
    if (!times.empty()) {
        animBar.setSliderPosition(chronoForComboIndex(0));
    }
    reload();
}

string Rtma::titleForTime(const string& utcTime) const {
    if (utcTime.empty()) {
        return "RTMA - " + UtilityRtma::labels[index];
    }
    auto objectDateTime = ObjectDateTime::parse(utcTime, "yyyyMMdd HH' UTC'");
    objectDateTime.utcToLocal();
    return objectDateTime.format("yyyyMMdd HH") + " - " + UtilityRtma::labels[index];
}

int Rtma::comboIndexForChrono(int chronoIndex) const {
    return std::max(0, static_cast<int>(times.size()) - 1) - chronoIndex;
}

int Rtma::chronoForComboIndex(int comboIndex) const {
    return std::max(0, static_cast<int>(times.size()) - 1) - comboIndex;
}

void Rtma::reload() {
    if (times.empty()) {
        setTitle("RTMA - no analyses available");
        return;
    }
    const auto comboIndex = std::max(0, comboboxTimes.getIndex());
    const auto utcTime = comboboxTimes.getValue();
    Utility::writePrefInt(prefToken, index);
    setTitle(titleForTime(utcTime));
    const auto url = UtilityRtma::getUrl(index, comboboxSector.getIndex(), utcTime);
    new FutureBytes{this, url, [this, comboIndex] (const auto& ba) {
        currentBytes = ba;
        image.setBytesKeepView(ba);
        animBar.setSliderPosition(chronoForComboIndex(comboIndex));
    }};
}

void Rtma::onScrub(int globalIndex) {
    const auto comboIndex = comboIndexForChrono(globalIndex);
    comboboxTimes.block();
    comboboxTimes.setIndex(comboIndex);
    comboboxTimes.unblock();
    reload();
}

void Rtma::onFrameShown(int localIndex) {
    const auto bytes = animBar.frameAt(localIndex);
    if (bytes.isEmpty()) {
        return;
    }
    currentBytes = bytes;
    image.setBytesKeepView(bytes);
    const auto globalIndex = animBar.globalIndexAt(localIndex);
    if (globalIndex < 0) {
        return;
    }
    const auto comboIndex = comboIndexForChrono(globalIndex);
    comboboxTimes.block();
    comboboxTimes.setIndex(comboIndex);
    comboboxTimes.unblock();
    if (comboIndex >= 0 && comboIndex < static_cast<int>(times.size())) {
        setTitle(titleForTime(times[comboIndex]));
    }
}

void Rtma::onRangeRequested(int rangeStart, int rangeEnd) {
    if (times.empty()) {
        animBar.cancelPending();
        return;
    }
    rangeStart = std::max(0, rangeStart);
    rangeEnd = std::min(static_cast<int>(times.size()) - 1, rangeEnd);
    if (rangeEnd - rangeStart < 1) {
        animBar.cancelPending();
        return;
    }

    vector<int> globalIndices;
    vector<string> urls;
    const auto sectorIndex = comboboxSector.getIndex();
    const auto productIndex = index;
    for (int g = rangeStart; g <= rangeEnd; g += 1) {
        const auto comboIndex = comboIndexForChrono(g);
        if (comboIndex < 0 || comboIndex >= static_cast<int>(times.size())) {
            continue;
        }
        globalIndices.push_back(g);
        urls.push_back(UtilityRtma::getUrl(productIndex, sectorIndex, times[comboIndex]));
    }
    if (urls.size() < 2) {
        animBar.cancelPending();
        return;
    }

    new FutureVoid{this,
        [this, urls] { downloaded = DownloadParallelBytes{urls}.byteList; },
        [this, globalIndices] {
            vector<QByteArray> validFrames;
            vector<int> validIndices;
            for (size_t i = 0; i < downloaded.size(); i += 1) {
                if (!downloaded[i].isEmpty()) {
                    validFrames.push_back(downloaded[i]);
                    validIndices.push_back(globalIndices[i]);
                }
            }
            downloaded.clear();
            if (validFrames.size() < 2) {
                animBar.cancelPending();
                setTitle("RTMA - animation download failed");
                return;
            }
            animBar.setFrames(validFrames, validIndices);
        }};
}

void Rtma::onSave() {
    const auto valid = animBar.frameCount();
    if (currentBytes.isEmpty() && valid == 0) {
        return;
    }
    QByteArray outBytes;
    if (valid >= 2) {
        // the bar's loaded frames are already chronological (oldest-first)
        outBytes = UtilityApng::fromFrames(animBar.loadedFrames(), frameDelayMs);
    }
    if (outBytes.isEmpty()) {
        outBytes = currentBytes;
    }

    const auto sectorIndex = std::max(0, comboboxSector.getIndex());
    const auto sectorName = sectorIndex < static_cast<int>(UtilityRtma::sectors.size())
        ? UtilityRtma::sectors[sectorIndex] : string{"sector"};
    string suggested;
    if (valid >= 2) {
        suggested = "rtma_" + UtilityRtma::codes[index] + "_" + sectorName + "_anim";
    } else {
        auto stamp = comboboxTimes.getValue();
        std::replace(stamp.begin(), stamp.end(), ' ', '_');
        suggested = "rtma_" + UtilityRtma::codes[index] + "_" + sectorName + "_" + stamp;
    }
    suggested += UtilityJxl::preferredExtension(outBytes);

    const auto picturesDir = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    const auto defaultPath = picturesDir.isEmpty()
        ? QString::fromStdString(suggested)
        : picturesDir + "/" + QString::fromStdString(suggested);
    const auto filter = UtilityJxl::available()
        ? QString{"JPEG XL Image (*.jxl);;All Files (*)"}
        : QString{"Images (*.png *.gif);;All Files (*)"};
    const auto fileName = QFileDialog::getSaveFileName(this, "Save Image", defaultPath, filter);
    if (fileName.isEmpty()) {
        return;
    }
    UtilityJxl::save(outBytes, fileName);
}

void Rtma::moveBack() {
    index -= 1;
    index = std::max(index, 0);
    comboboxProduct.setIndex(index);
}

void Rtma::moveForward() {
    index += 1;
    index = std::min(index, static_cast<int>(UtilityRtma::labels.size()) - 1);
    comboboxProduct.setIndex(index);
}

void Rtma::changeProduct() {
    index = comboboxProduct.getIndex();
    animBar.stopIfAnimating();
    animBar.clearFrames();
    reload();
}

void Rtma::changeSector() {
    animBar.stopIfAnimating();
    animBar.clearFrames();
    reload();
}

void Rtma::changeTime() {
    animBar.stopIfAnimating();
    reload();
}

void Rtma::openFullImage() {
    if (!currentBytes.isEmpty()) {
        new ImageViewer{this, currentBytes, "RTMA - " + UtilityRtma::labels[index]};
    }
}

void Rtma::resizeEventCustom() {
    // ZoomImage re-fits itself on resize while the user has not zoomed
}
