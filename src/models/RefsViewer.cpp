// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "models/RefsViewer.h"
#include <QBuffer>
#include <QFile>
#include <QFileDialog>
#include <QImage>
#include <QPainter>
#include <QStandardPaths>
#include "models/RefsPointGraph.h"
#include "models/UtilityGrib.h"
#include "models/UtilityRefs.h"
#include "objects/FutureVoid.h"
#include "objects/UtilityApng.h"
#include "objects/UtilityJxl.h"
#include <cmath>
#include "util/To.h"

namespace {
    constexpr int frameDelayMs = 400;      // per-frame dwell for the exported APNG, matches AnimationBar
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
    , buttonGraph{this, Icon::None, "Plume graph"}
{
    setTitle("REFS Ensemble Viewer");

    // set defaults BEFORE connecting - each combo's connect() fires
    // immediately below, and setting an index after connecting would fire
    // a redundant reload() per default (found live: 3 extra concurrent
    // resolveSynopticRun() probes on first launch before this fix).
    // A reasonable default comparison out of the box, now that Stage 1
    // rounds out mean/spread/pmmn fields: mean temp + mean-derived
    // reflectivity (pmmn) on top, their spread counterparts underneath -
    // one glance shows both "what's forecast" and "how much do members
    // disagree" for the same two quantities, rather than four panels all
    // showing the same field index.
    panel2.setFieldIndex(6);   // Probability-Matched Mean Composite Reflectivity
    panel3.setFieldIndex(3);   // Ensemble Spread 2m Temperature
    panel4.setFieldIndex(5);   // Ensemble Spread Composite Reflectivity
    panel1.onFieldChanged();
    panel2.onFieldChanged();
    panel3.onFieldChanged();
    panel4.onFieldChanged();

    comboRun.connect([this] { updateForecastHours(); reload(); });
    comboRegion.connect([this] { invalidateAnimation(); reload(); });
    comboForecastHour.connect([this] {
        animBar.stopIfAnimating();
        reload();
    });
    panel1.fieldCombo().connect([this] { panel1.onFieldChanged(); invalidateAnimation(); reload(); });
    panel1.connectThreshold([this] { invalidateAnimation(); reload(); });
    panel2.fieldCombo().connect([this] { panel2.onFieldChanged(); invalidateAnimation(); reload(); });
    panel2.connectThreshold([this] { invalidateAnimation(); reload(); });
    panel3.fieldCombo().connect([this] { panel3.onFieldChanged(); invalidateAnimation(); reload(); });
    panel3.connectThreshold([this] { invalidateAnimation(); reload(); });
    panel4.fieldCombo().connect([this] { panel4.onFieldChanged(); invalidateAnimation(); reload(); });
    panel4.connectThreshold([this] { invalidateAnimation(); reload(); });

    boxTop.addWidget(comboRun);
    boxTop.addWidget(comboRegion);
    boxTop.addWidget(comboForecastHour);
    boxTop.addWidget(buttonGraph);
    buttonGraph.getView()->setToolTip("Click a map to pick a point, then open the per-member plume chart for that panel's field");
    buttonGraph.connect([this] { onGraph(); });
    RefsPanel* clickPanels[4] = {&panel1, &panel2, &panel3, &panel4};
    for (size_t i = 0; i < 4; i += 1) {
        QObject::connect(&clickPanels[i]->imageView(), &ZoomImage::clicked, this,
                         [this, i] (double fx, double fy) { onMapClicked(i, fx, fy); });
        QObject::connect(&clickPanels[i]->imageView(), &ZoomImage::hovered, this,
                         [this, i] (double fx, double fy) { onHover(i, fx, fy); });
        QObject::connect(&clickPanels[i]->imageView(), &ZoomImage::hoverEnded, this, [this] { onHoverEnded(); });
    }
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
    const std::array<double, 4> thresholds{
        panel1.threshold(), panel2.threshold(), panel3.threshold(), panel4.threshold()};

    setTitle("REFS Ensemble Viewer - loading...");
    new FutureVoid{this,
        [this, regionIndex, forecastHour, runId, fieldIndices, thresholds] {
            for (size_t i = 0; i < 4; i += 1) {
                string localStatus;
                double lo = 0.0;
                double hi = 0.0;
                string gridPath;
                const auto path = UtilityRefs::render(fieldIndices[i], regionIndex, forecastHour, runId,
                                                        localStatus, lo, hi, gridPath, thresholds[i]);
                pendingFrame[i].clear();
                pendingGrid[i] = gridPath;
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
            gridPaths = pendingGrid;
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
    sweepGrids.assign(indices.size(), std::array<string, 4>{});
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
        vector<std::array<string, 4>> validGrids;
        for (size_t k = 0; k < sweepFrames.size(); k += 1) {
            if (!sweepFrames[k][0].isEmpty()) {
                validGrids.push_back(sweepGrids[k]);
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
        sweepGrids = validGrids;
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
    const std::array<double, 4> thresholds{
        panel1.threshold(), panel2.threshold(), panel3.threshold(), panel4.threshold()};

    new FutureVoid{this,
        [this, regionIndex, hour, runId, fieldIndices, thresholds] {
            for (size_t i = 0; i < 4; i += 1) {
                string localStatus;
                double lo = 0.0;
                double hi = 0.0;
                string gridPath;
                const auto path = UtilityRefs::render(fieldIndices[i], regionIndex, hour, runId,
                                                        localStatus, lo, hi, gridPath, thresholds[i]);
                pendingFrame[i].clear();
                pendingGrid[i] = gridPath;
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
            sweepGrids[sweepIndex] = pendingGrid;
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
    if (localIndex < static_cast<int>(sweepGrids.size())) {
        gridPaths = sweepGrids[localIndex];
    }
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

// Linked read-out: one cursor position, four independent values (each panel
// samples its own hover sidecar). Every panel renders the same region at the
// same size, so a single (fx, fy) is valid in all of them; the crosshair is
// snapped to the hovered panel's own sample cell.
void RefsViewer::onHover(size_t panelIndex, double fx, double fy) {
    RefsPanel* panels[4] = {&panel1, &panel2, &panel3, &panel4};
    double markerFx = fx;
    double markerFy = fy;
    const auto sampleGrid = [this] (const string& path) -> const SampleGrid * {
        if (path.empty()) {
            return nullptr;
        }
        auto cached = gridCache.find(path);
        if (cached == gridCache.end()) {
            if (gridCache.size() > 60) {
                gridCache.clear();
            }
            cached = gridCache.emplace(path, SampleGrid::load(QString::fromStdString(path))).first;
        }
        return &cached->second;
    };
    if (const auto * own = sampleGrid(gridPaths[panelIndex])) {
        double lon = 0.0;
        double lat = 0.0;
        own->snap(fx, fy, lon, lat, markerFx, markerFy);
    }
    const auto box = UtilityGrib::regionBbox(comboRegion.getIndex());
    const auto lon = box.west + fx * (box.east - box.west);
    const auto lat = box.north - fy * (box.north - box.south);
    const QString coords = QString{"%1%2  %3%4"}
        .arg(std::fabs(lat), 0, 'f', 2).arg(lat >= 0 ? "N" : "S")
        .arg(std::fabs(lon), 0, 'f', 2).arg(lon >= 0 ? "E" : "W");
    for (size_t k = 0; k < 4; k += 1) {
        panels[k]->imageView().setMarker(markerFx, markerFy);
        const auto * grid = sampleGrid(gridPaths[k]);
        double value = 0.0;
        if (grid == nullptr || !grid->valueAt(lon, lat, value)) {
            panels[k]->setHoverText(grid == nullptr ? QString{} : coords + "\nno data");
            continue;
        }
        const auto fieldIndex = panels[k]->fieldIndex();
        const auto units = QString::fromStdString(
            fieldIndex >= 0 && fieldIndex < static_cast<int>(UtilityRefs::fields.size())
                ? UtilityRefs::fields[fieldIndex].units : string{});
        panels[k]->setHoverText(coords + "\n" + QString::number(value, 'f', 1) + " " + units);
    }
}

void RefsViewer::onHoverEnded() {
    RefsPanel* panels[4] = {&panel1, &panel2, &panel3, &panel4};
    for (auto* panel : panels) {
        panel->imageView().clearMarker();
        panel->setHoverText(QString{});
    }
}

void RefsViewer::onMapClicked(size_t panelIndex, double fx, double fy) {
    selectedFx = fx;
    selectedFy = fy;
    selectedPanel = static_cast<int>(panelIndex);
    // linked crosshair: every panel shows the same region at the same size,
    // so one (fx, fy) marks the same spot in all four
    RefsPanel* panels[4] = {&panel1, &panel2, &panel3, &panel4};
    for (auto* panel : panels) {
        panel->imageView().setMarker(fx, fy);
    }
    const auto box = UtilityGrib::regionBbox(comboRegion.getIndex());
    const auto lon = box.west + fx * (box.east - box.west);
    const auto lat = box.north - fy * (box.north - box.south);
    setTitle("REFS Ensemble Viewer - point " + To::string(lat) + " N, " + To::string(lon) +
             " E selected (use 'Plume graph' for panel " + To::string(selectedPanel + 1) + ")");
}

void RefsViewer::onGraph() {
    if (selectedFx < 0.0) {
        setTitle("REFS Ensemble Viewer - click a map first to pick a point");
        return;
    }
    RefsPanel* panels[4] = {&panel1, &panel2, &panel3, &panel4};
    auto& panel = *panels[selectedPanel];
    UtilityRefs::MemberBasis basis;
    if (!UtilityRefs::memberBasis(panel.fieldIndex(), panel.threshold(), basis)) {
        setTitle("REFS Ensemble Viewer - panel " + To::string(selectedPanel + 1) +
                 " has no per-member data; pick a Member, Paintball or Member Probability field");
        return;
    }
    const auto box = UtilityGrib::regionBbox(comboRegion.getIndex());
    const auto lon = box.west + selectedFx * (box.east - box.west);
    const auto lat = box.north - selectedFy * (box.north - box.south);
    const auto runIndex = comboRun.getIndex();
    const string runId = (runIndex >= 0 && runIndex < static_cast<int>(runOptions.size()))
        ? runOptions[runIndex].second : string{};
    new RefsPointGraph{this, basis, lon, lat, runId};
}

// One image for the whole comparison: the four panel renders in a 2x2 grid
// (each with a caption strip naming its field) under a header line. Panels
// with no image yet are left blank. PNG bytes, ready for UtilityApng /
// UtilityJxl like every other viewer's export.
QByteArray RefsViewer::buildMosaic(const std::array<QByteArray, 4>& panelBytes, const string& header) const {
    std::array<QImage, 4> images;
    QSize cell;
    for (size_t i = 0; i < 4; i += 1) {
        images[i] = QImage::fromData(panelBytes[i]);
        if (!images[i].isNull() && cell.isEmpty()) {
            cell = images[i].size();
        }
    }
    if (cell.isEmpty()) {
        return {};
    }
    constexpr int gap = 4;
    constexpr int headerHeight = 26;
    constexpr int captionHeight = 22;
    const int width = cell.width() * 2 + gap;
    const int height = headerHeight + (cell.height() + captionHeight) * 2 + gap;
    QImage canvas{width, height, QImage::Format_ARGB32};
    canvas.fill(QColor{30, 30, 30});
    QPainter painter{&canvas};
    painter.setRenderHint(QPainter::Antialiasing);
    QFont font = painter.font();
    font.setPixelSize(14);
    font.setBold(true);
    painter.setFont(font);
    painter.setPen(QColor{235, 235, 235});
    painter.drawText(QRect{8, 0, width - 16, headerHeight}, Qt::AlignVCenter | Qt::AlignLeft,
                     QString::fromStdString(header));
    const RefsPanel * panels[4] = {&panel1, &panel2, &panel3, &panel4};
    for (int i = 0; i < 4; i += 1) {
        const int column = i % 2;
        const int row = i / 2;
        const int x = column * (cell.width() + gap);
        const int y = headerHeight + row * (cell.height() + captionHeight + gap);
        painter.fillRect(QRect{x, y, cell.width(), captionHeight}, QColor{50, 50, 50});
        painter.setPen(QColor{235, 235, 235});
        const auto fieldIndex = panels[i]->fieldIndex();
        auto caption = QString::fromStdString(fieldIndex >= 0 && fieldIndex < static_cast<int>(UtilityRefs::fields.size())
            ? UtilityRefs::fields[fieldIndex].label : string{});
        if (UtilityRefs::usesThreshold(fieldIndex)) {
            const auto threshold = panels[i]->threshold();
            caption += QString{"  >= %1 %2"}
                .arg(std::isnan(threshold) ? UtilityRefs::defaultThreshold(fieldIndex) : threshold, 0, 'g', 6)
                .arg(QString::fromStdString(UtilityRefs::thresholdUnits(fieldIndex)));
        }
        painter.drawText(QRect{x + 6, y, cell.width() - 12, captionHeight}, Qt::AlignVCenter | Qt::AlignLeft, caption);
        painter.fillRect(QRect{x, y + captionHeight, cell.width(), cell.height()}, QColor{200, 210, 215});
        if (!images[i].isNull()) {
            painter.drawImage(QPoint{x, y + captionHeight}, images[i].scaled(cell, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
        }
    }
    painter.end();
    QByteArray out;
    QBuffer buffer{&out};
    buffer.open(QIODevice::WriteOnly);
    canvas.save(&buffer, "PNG");
    return out;
}

void RefsViewer::onSave() {
    const auto frameCount = animBar.frameCount();
    QByteArray outBytes;
    if (frameCount >= 2 && sweepFrames.size() == static_cast<size_t>(frameCount)) {
        vector<QByteArray> mosaics;
        for (size_t k = 0; k < sweepFrames.size(); k += 1) {
            const auto mosaic = buildMosaic(sweepFrames[k], k < frameStatuses.size() ? frameStatuses[k] : string{});
            if (!mosaic.isEmpty()) {
                mosaics.push_back(mosaic);
            }
        }
        outBytes = UtilityApng::fromFrames(mosaics, frameDelayMs);
    }
    if (outBytes.isEmpty()) {
        outBytes = buildMosaic(renderedBytes, status);
    }
    if (outBytes.isEmpty()) {
        setTitle("REFS Ensemble Viewer - nothing to save yet");
        return;
    }

    auto suggested = frameCount >= 2
        ? string{"refs_comparison_anim"}
        : ("refs_comparison_f" + comboForecastHour.getValue());
    suggested += UtilityJxl::preferredExtension(outBytes);
    const auto picturesDir = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    const auto defaultPath = picturesDir.isEmpty()
        ? QString::fromStdString(suggested)
        : picturesDir + "/" + QString::fromStdString(suggested);
    const auto filter = UtilityJxl::available()
        ? QString{"JPEG XL Image (*.jxl);;All Files (*)"}
        : QString{"Images (*.png);;All Files (*)"};
    const auto fileName = QFileDialog::getSaveFileName(this, "Save Image", defaultPath, filter);
    if (fileName.isEmpty()) {
        return;
    }
    UtilityJxl::save(outBytes, fileName);
}

void RefsViewer::resizeEventCustom() {
    // ZoomImage re-fits itself on resize while the user has not zoomed
}
