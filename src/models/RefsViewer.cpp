// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "models/RefsViewer.h"
#include <QFile>
#include "models/UtilityGrib.h"
#include "models/UtilityRefs.h"
#include "objects/FutureVoid.h"
#include "util/To.h"

namespace {
    constexpr size_t maxAnimFrames = 12;   // 4x the GDAL work per hour vs a single-panel viewer - keep the sweep shorter
}

RefsViewer::RefsViewer(Window * parent)
    : Window{parent}
    , comboRun{this, {"Latest"}}
    , comboRegion{this, UtilityRefs::regions()}
    , comboForecastHour{this, UtilityRefs::forecastHours()}
    , panel1{this}
    , panel2{this}
    , panel3{this}
    , panel4{this}
    , animBar{this,
        [this] (int start, int end) { onRangeRequested(start, end); },
        [this] (int local) { onFrameShown(local); },
        [this] (int global) { onScrub(global); },
        [this] { onSave(); }}
{
    setTitle("REFS Ensemble Viewer");

    // set defaults BEFORE connecting - each combo's connect() fires
    // immediately below, and setting an index after connecting would fire
    // a redundant reload() per default (found live: 3 extra concurrent
    // resolveSynopticRun() probes on first launch before this fix).
    // A reasonable default comparison out of the box - two panels on each
    // of the two fields Stage 0 has, not all four identical.
    panel3.setFieldIndex(1);
    panel4.setFieldIndex(1);

    comboRun.connect([this] { updateForecastHours(); reload(); });
    comboRegion.connect([this] { invalidateAnimation(); reload(); });
    comboForecastHour.connect([this] {
        animBar.stopIfAnimating();
        reload();
    });
    panel1.fieldCombo().connect([this] { invalidateAnimation(); reload(); });
    panel2.fieldCombo().connect([this] { invalidateAnimation(); reload(); });
    panel3.fieldCombo().connect([this] { invalidateAnimation(); reload(); });
    panel4.fieldCombo().connect([this] { invalidateAnimation(); reload(); });

    boxTop.addWidget(comboRun);
    boxTop.addWidget(comboRegion);
    boxTop.addWidget(comboForecastHour);
    boxTop.addStretch();
    panel1.addTo(rowTop);
    panel2.addTo(rowTop);
    panel3.addTo(rowBottom);
    panel4.addTo(rowBottom);
    boxGrid.addLayout(rowTop, 1);
    boxGrid.addLayout(rowBottom, 1);
    box.addLayout(boxTop);
    animBar.addTo(box);
    box.addLayout(boxGrid, 1);
    box.getAndShow(this);

    animBar.setAvailableLabels(comboForecastHour.getItems());
    reload();

    new FutureVoid{this,
        [this] { runOptions = UtilityRefs::runOptions(); },
        [this] {
            if (runOptions.size() < 2) {
                return;
            }
            std::vector<string> labels;
            for (const auto& option : runOptions) {
                labels.push_back(option.first);
            }
            comboRun.block();
            comboRun.setList(labels);
            comboRun.setIndex(0);
            comboRun.unblock();
            updateForecastHours();
        }};
}

void RefsViewer::updateForecastHours() {
    animGeneration += 1;
    animBar.stopIfAnimating();
    const auto hours = UtilityRefs::forecastHours();
    const auto previous = comboForecastHour.getValue();
    comboForecastHour.block();
    comboForecastHour.setList(hours);
    int keep = 0;
    for (int i = 0; i < static_cast<int>(hours.size()); i += 1) {
        if (hours[i] == previous) {
            keep = i;
            break;
        }
    }
    comboForecastHour.setIndex(keep);
    comboForecastHour.unblock();
    animBar.setAvailableLabels(hours);
    animBar.setSliderPosition(keep);
}

void RefsViewer::invalidateAnimation() {
    animGeneration += 1;
    animBar.stopIfAnimating();
    animBar.clearFrames();
}

void RefsViewer::reload() {
    const auto regionIndex = comboRegion.getIndex();
    const auto forecastHour = comboForecastHour.getValue();
    const auto runIndex = comboRun.getIndex();
    const string runId = (runIndex >= 0 && runIndex < static_cast<int>(runOptions.size()))
        ? runOptions[runIndex].second : string{};
    const std::array<int, 4> fieldIndices{
        panel1.fieldIndex(), panel2.fieldIndex(), panel3.fieldIndex(), panel4.fieldIndex()};

    setTitle("REFS Ensemble Viewer - loading...");
    new FutureVoid{this,
        [this, regionIndex, forecastHour, runId, fieldIndices] {
            for (size_t i = 0; i < 4; i += 1) {
                string localStatus;
                double lo = 0.0;
                double hi = 0.0;
                string gridPath;
                const auto path = UtilityRefs::render(fieldIndices[i], regionIndex, forecastHour, runId,
                                                        localStatus, lo, hi, gridPath);
                pendingFrame[i].clear();
                if (!path.empty()) {
                    QFile file{QString::fromStdString(path)};
                    if (file.open(QIODevice::ReadOnly)) {
                        pendingFrame[i] = file.readAll();
                        file.close();
                    }
                }
                if (i == 0) {
                    pendingStatus = localStatus;
                }
            }
        },
        [this] {
            renderedBytes = pendingFrame;
            RefsPanel* panels[4] = {&panel1, &panel2, &panel3, &panel4};
            for (size_t i = 0; i < 4; i += 1) {
                if (!renderedBytes[i].isEmpty()) {
                    panels[i]->setImageKeepView(renderedBytes[i]);
                }
            }
            status = pendingStatus;
            setTitle("REFS Ensemble Viewer - " + status);
            animBar.setSliderPosition(comboForecastHour.getIndex());
        }};
}

void RefsViewer::onScrub(int globalIndex) {
    comboForecastHour.block();
    comboForecastHour.setIndex(globalIndex);
    comboForecastHour.unblock();
    reload();
}

void RefsViewer::onRangeRequested(int rangeStart, int rangeEnd) {
    const auto allHours = comboForecastHour.getItems();
    vector<int> indices;
    for (int i = rangeStart; i <= rangeEnd && i < static_cast<int>(allHours.size()); i += 1) {
        indices.push_back(i);
    }
    if (indices.size() > maxAnimFrames) {
        vector<int> sub;
        const auto n = indices.size();
        for (size_t i = 0; i < maxAnimFrames; i += 1) {
            const auto idx = i * (n - 1) / (maxAnimFrames - 1);
            if (sub.empty() || sub.back() != indices[idx]) {
                sub.push_back(indices[idx]);
            }
        }
        indices = sub;
    }
    if (indices.size() < 2) {
        animBar.cancelPending();
        return;
    }

    animGeneration += 1;
    sweepIndices = indices;
    sweepFrames.assign(indices.size(), std::array<QByteArray, 4>{});
    frameStatuses.assign(indices.size(), string{});
    renderNextAnimFrame(0, animGeneration);
}

void RefsViewer::renderNextAnimFrame(size_t sweepIndex, int generation) {
    if (generation != animGeneration) {
        return;   // region / run / field / cycle changed mid-sweep - abandon it
    }
    if (sweepIndex >= sweepIndices.size()) {
        vector<std::array<QByteArray, 4>> validFrames;
        vector<int> validIndices;
        vector<string> validStatuses;
        for (size_t k = 0; k < sweepFrames.size(); k += 1) {
            if (!sweepFrames[k][0].isEmpty()) {
                validFrames.push_back(sweepFrames[k]);
                validIndices.push_back(sweepIndices[k]);
                validStatuses.push_back(frameStatuses[k]);
            }
        }
        if (validFrames.size() < 2) {
            animBar.cancelPending();
            setTitle("REFS Ensemble Viewer - animation render failed");
            return;
        }
        sweepFrames = validFrames;
        sweepIndices = validIndices;
        frameStatuses = validStatuses;
        // AnimationBar-as-adapter: it only needs one QByteArray per frame for
        // its own frame-count/label bookkeeping - panel 1's bytes stand in.
        vector<QByteArray> barFrames;
        for (const auto& frame : sweepFrames) {
            barFrames.push_back(frame[0]);
        }
        animBar.setFrames(barFrames, sweepIndices);
        return;
    }

    const auto regionIndex = comboRegion.getIndex();
    const auto runIndex = comboRun.getIndex();
    const string runId = (runIndex >= 0 && runIndex < static_cast<int>(runOptions.size()))
        ? runOptions[runIndex].second : string{};
    const auto allHours = comboForecastHour.getItems();
    const auto globalIndex = sweepIndices[sweepIndex];
    const auto hour = (globalIndex >= 0 && globalIndex < static_cast<int>(allHours.size()))
        ? allHours[globalIndex] : string{};
    const std::array<int, 4> fieldIndices{
        panel1.fieldIndex(), panel2.fieldIndex(), panel3.fieldIndex(), panel4.fieldIndex()};

    new FutureVoid{this,
        [this, regionIndex, hour, runId, fieldIndices] {
            for (size_t i = 0; i < 4; i += 1) {
                string localStatus;
                double lo = 0.0;
                double hi = 0.0;
                string gridPath;
                const auto path = UtilityRefs::render(fieldIndices[i], regionIndex, hour, runId,
                                                        localStatus, lo, hi, gridPath);
                pendingFrame[i].clear();
                if (!path.empty()) {
                    QFile file{QString::fromStdString(path)};
                    if (file.open(QIODevice::ReadOnly)) {
                        pendingFrame[i] = file.readAll();
                        file.close();
                    }
                }
                if (i == 0) {
                    pendingStatus = localStatus;
                }
            }
        },
        [this, sweepIndex, generation] {
            if (generation != animGeneration) {
                return;
            }
            sweepFrames[sweepIndex] = pendingFrame;
            frameStatuses[sweepIndex] = pendingStatus;
            for (auto& frame : pendingFrame) {
                frame.clear();
            }
            renderNextAnimFrame(sweepIndex + 1, generation);
        }};
}

void RefsViewer::onFrameShown(int localIndex) {
    if (localIndex < 0 || localIndex >= static_cast<int>(sweepFrames.size())) {
        return;
    }
    const auto& frame = sweepFrames[localIndex];
    renderedBytes = frame;
    RefsPanel* panels[4] = {&panel1, &panel2, &panel3, &panel4};
    for (size_t i = 0; i < 4; i += 1) {
        if (!frame[i].isEmpty()) {
            panels[i]->setImageKeepView(frame[i]);
        }
    }
    if (localIndex < static_cast<int>(frameStatuses.size())) {
        status = frameStatuses[localIndex];
        setTitle("REFS Ensemble Viewer - " + status);
    }
    const auto globalIndex = animBar.globalIndexAt(localIndex);
    if (globalIndex >= 0) {
        comboForecastHour.block();
        comboForecastHour.setIndex(globalIndex);
        comboForecastHour.unblock();
    }
}

void RefsViewer::onSave() {
    // Stage 0 doesn't wire export yet (mosaic compositing is Stage 6) -
    // nothing to do until then.
}

void RefsViewer::resizeEventCustom() {
    // ZoomImage re-fits itself on resize while the user has not zoomed
}
