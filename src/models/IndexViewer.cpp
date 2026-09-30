// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "models/IndexViewer.h"
#include <algorithm>
#include <cmath>
#include <vector>
#include <QBuffer>
#include <QColor>
#include <QFile>
#include <QFileDialog>
#include <QImage>
#include <QPainter>
#include <QObject>
#include <QPalette>
#include <QStandardPaths>
#include <QStringList>
#include "misc/ImageViewer.h"
#include "models/UtilitySevereIndices.h"
#include "objects/FutureVoid.h"
#include "objects/SampleGrid.h"
#include "objects/UtilityApng.h"
#include "objects/UtilityAnimationExport.h"
#include "util/To.h"

namespace {
    constexpr int frameDelayMs = 400;      // per-frame dwell for the exported APNG
    constexpr size_t maxAnimFrames = 24;   // caps a render sweep to at most this many GDAL renders
}

IndexViewer::IndexViewer(Window * parent)
    : Window{parent}
    , image{this}
    , legend{this, Normal}
    , comboRun{this, {"Latest"}}
    , comboIndex{this, UtilitySevereIndices::indexLabels()}
    , comboRegion{this, UtilitySevereIndices::regionLabels()}
    , comboForecastHour{this, UtilitySevereIndices::forecastHours()}
    , backForward{this, [this] { moveBack(); }, [this] { moveForward(); }}
    , buttonMax{this, Icon::None, "Max of range"}
    , buttonDay1{this, Icon::None, "Day 1 max (12z-12z)"}
    , animBar{this,
        [this] (int start, int end) { onRangeRequested(start, end); },
        [this] (int local) { onFrameShown(local); },
        [this] (int global) { onScrub(global); },
        [this] { onSave(); }}
{
    setTitle("Parametric Index Viewer");
    image.setCrosshairMode(true);
    QObject::connect(&image, &ZoomImage::doubleClicked, this, [this] { openFullImage(); });
    QObject::connect(&image, &ZoomImage::hovered, this, [this] (double fx, double fy) { onHover(fx, fy); });
    QObject::connect(&image, &ZoomImage::hoverEnded, this, [this] { onHoverEnded(); });
    comboRun.connect([this] { updateForecastHours(); reload(); });
    comboIndex.connect([this] { invalidateAnimation(); reload(); });
    comboRegion.connect([this] { invalidateAnimation(); reload(); });
    comboForecastHour.connect([this] {
        animBar.stopIfAnimating();
        reload();
    });

    boxTop.addLayout(backForward);
    boxTop.addWidget(comboRun);
    boxTop.addWidget(comboIndex);
    boxTop.addWidget(comboRegion);
    boxTop.addWidget(comboForecastHour);
    buttonMax.getView()->setToolTip("Pixel-wise maximum of the selected index over the Range hours below "
                                    "(e.g. set 01 to 24 for a 24-hour max)");
    buttonMax.connect([this] { showMaxOfRange(); });
    buttonDay1.getView()->setToolTip("Maximum over the SPC Day-1 period, 12z to 12z (the 24 forecast hours "
                                     "ending at the next 12z after this run)");
    buttonDay1.connect([this] { showDay1Max(); });
    boxTop.addWidget(buttonMax);
    boxTop.addWidget(buttonDay1);
    boxTop.addStretch();
    boxImage.addWidgetReal(&image, 1, Qt::Alignment{});
    boxImage.addWidget(legend);
    box.addLayout(boxTop);
    animBar.addTo(box);
    box.addLayout(boxImage);
    box.getAndShow(this);

    hoverLabel = new QLabel{&image};
    hoverLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    hoverLabel->setStyleSheet(
        "QLabel { background-color: rgba(15, 15, 15, 205); color: #f2f2f2;"
        " padding: 4px 8px; border-radius: 3px; }");
    hoverLabel->hide();

    animBar.setAvailableLabels(comboForecastHour.getItems());
    reload();

    // SHIP only exists at synoptic cycles - runOptions() is already filtered
    // to those, so (unlike GribViewer) there's no separate "does a hourly
    // run even have data" probe needed, just the same background directory
    // scrape UtilityGrib::runOptions() already does under the hood.
    new FutureVoid{this,
        [this] { runOptions = UtilitySevereIndices::runOptions(); },
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

void IndexViewer::updateForecastHours() {
    // the run behind the current selection ("Latest" -> the newest concrete run)
    string runId;
    const auto index = comboRun.getIndex();
    if (index >= 0 && index < static_cast<int>(runOptions.size())) {
        runId = runOptions[index].second;
    }
    if (runId.empty() && runOptions.size() > 1) {
        runId = runOptions[1].second;
    }
    const int cycle = runId.size() == 10 ? To::Int(runId.substr(8, 2)) : -1;

    const auto previous = comboForecastHour.getValue();
    const auto hours = UtilitySevereIndices::forecastHours(cycle);
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

    // the fhr list just changed (new run / cycle) - any loaded animation is stale
    animGeneration += 1;
    animBar.stopIfAnimating();
    animBar.setAvailableLabels(hours);
    animBar.setSliderPosition(keep);
}

void IndexViewer::invalidateAnimation() {
    animGeneration += 1;
    animBar.stopIfAnimating();
    animBar.clearFrames();
    gridCache.clear();
}

// "24hr max": the worst case of the selected index over the Range hours
// (the same From/To pickers Play uses). Shown in the main image; changing the
// hour, index, region or run goes back to a normal single-hour view.
void IndexViewer::showMaxOfRange() {
    const auto allHours = comboForecastHour.getItems();
    vector<string> hours;
    for (int i = animBar.rangeStartIndex(); i <= animBar.rangeEndIndex() && i < static_cast<int>(allHours.size()); i += 1) {
        hours.push_back(allHours[i]);
    }
    showMaxOfHours(hours);
}

void IndexViewer::showMaxOfHours(const std::vector<std::string>& hoursIn) {
    constexpr size_t maxHours = 48;
    auto hours = hoursIn;
    if (hours.size() < 2) {
        setTitle("Parametric Index Viewer - pick a Range of at least two hours for a max composite");
        return;
    }
    if (hours.size() > maxHours) {
        hours.resize(maxHours);
    }
    animBar.stopIfAnimating();
    const auto indexIndex = comboIndex.getIndex();
    const auto regionIndex = comboRegion.getIndex();
    const auto runIndex = comboRun.getIndex();
    const string runId = (runIndex >= 0 && runIndex < static_cast<int>(runOptions.size()))
        ? runOptions[runIndex].second : string{};
    legend.setBytes(buildLegend(indexIndex, 0.0, 0.0));
    setTitle("Parametric Index Viewer - computing " + To::string(static_cast<int>(hours.size())) + "-hour max...");
    sampleGridPath.clear();
    refreshHover();
    new FutureVoid{this,
        [this, indexIndex, regionIndex, hours, runId] {
            pngPath = UtilitySevereIndices::renderMax(indexIndex, regionIndex, hours, runId,
                                                       status, dataMin, dataMax, sampleGridPath);
        },
        [this, indexIndex] {
            setTitle("Parametric Index Viewer - " + status);
            legend.setBytes(buildLegend(indexIndex, dataMin, dataMax));
            if (pngPath.empty()) {
                renderedBytes.clear();
                sampleGridPath.clear();
                return;
            }
            QFile file{QString::fromStdString(pngPath)};
            if (file.open(QIODevice::ReadOnly)) {
                renderedBytes = file.readAll();
                image.setBytesKeepView(renderedBytes);
                file.close();
            }
            refreshHover();
        }};
}

void IndexViewer::showDay1Max() {
    const auto runIndex = comboRun.getIndex();
    const string runId = (runIndex >= 0 && runIndex < static_cast<int>(runOptions.size()))
        ? runOptions[runIndex].second : string{};
    new FutureVoid{this,
        [this, runId] { day1Pending = UtilityGrib::day1Hours(runId, true); },
        [this] {
            if (day1Pending.size() < 2) {
                setTitle("Parametric Index Viewer - the Day 1 (12z-12z) window is not available for this run");
                return;
            }
            showMaxOfHours(day1Pending);
        }};
}

void IndexViewer::reload() {
    const auto indexIndex = comboIndex.getIndex();
    const auto regionIndex = comboRegion.getIndex();
    const auto forecastHour = comboForecastHour.getValue();
    const auto runIndex = comboRun.getIndex();
    const string runId = (runIndex >= 0 && runIndex < static_cast<int>(runOptions.size()))
        ? runOptions[runIndex].second : string{};
    legend.setBytes(buildLegend(indexIndex, 0.0, 0.0));
    setTitle("Parametric Index Viewer - loading...");
    sampleGridPath.clear();
    refreshHover();
    new FutureVoid{this,
        [this, indexIndex, regionIndex, forecastHour, runId] {
            pngPath = UtilitySevereIndices::render(indexIndex, regionIndex, forecastHour, runId,
                                                    status, dataMin, dataMax, sampleGridPath);
        },
        [this, indexIndex] {
            setTitle("Parametric Index Viewer - " + status);
            legend.setBytes(buildLegend(indexIndex, dataMin, dataMax));
            if (pngPath.empty()) {
                renderedBytes.clear();
                sampleGridPath.clear();
                return;
            }
            QFile file{QString::fromStdString(pngPath)};
            if (file.open(QIODevice::ReadOnly)) {
                renderedBytes = file.readAll();
                image.setBytesKeepView(renderedBytes);
                file.close();
            }
            animBar.setSliderPosition(comboForecastHour.getIndex());
            refreshHover();
        }};
}

void IndexViewer::openFullImage() {
    if (!renderedBytes.isEmpty()) {
        new ImageViewer{this, renderedBytes, status.empty() ? string{"Parametric Index"} : status};
    }
}

void IndexViewer::onScrub(int globalIndex) {
    comboForecastHour.block();
    comboForecastHour.setIndex(globalIndex);
    comboForecastHour.unblock();
    reload();
}

void IndexViewer::onHoverEnded() {
    lastHoverFx = -1.0;
    lastHoverFy = -1.0;
    image.clearMarker();
    if (hoverLabel) {
        hoverLabel->hide();
    }
}

void IndexViewer::refreshHover() {
    // lastHoverFx >= 0 means the cursor is currently over the image
    if (lastHoverFx >= 0.0) {
        onHover(lastHoverFx, lastHoverFy);
    }
}

void IndexViewer::onHover(double fx, double fy) {
    lastHoverFx = fx;
    lastHoverFy = fy;
    if (!hoverLabel) {
        return;
    }
    if (sampleGridPath.empty()) {
        image.clearMarker();
        hoverLabel->hide();
        return;
    }
    auto cached = gridCache.find(sampleGridPath);
    if (cached == gridCache.end()) {
        if (gridCache.size() > 40) {
            gridCache.clear();
        }
        cached = gridCache.emplace(sampleGridPath, SampleGrid::load(QString::fromStdString(sampleGridPath))).first;
    }
    const auto& grid = cached->second;

    // snap to the sampled cell so the crosshair, coords and value all agree
    double lon = 0.0;
    double lat = 0.0;
    double markerFx = 0.0;
    double markerFy = 0.0;
    if (!grid.snap(fx, fy, lon, lat, markerFx, markerFy)) {
        image.clearMarker();
        hoverLabel->hide();
        return;
    }
    image.setMarker(markerFx, markerFy);

    const QChar degree{0x00B0};
    double value = 0.0;
    QString valueText = "no data";
    if (grid.valueAt(lon, lat, value)) {
        const auto indexIndex = comboIndex.getIndex();
        const auto keyUpper = (indexIndex >= 0 && indexIndex < static_cast<int>(UtilitySevereIndices::indices.size()))
            ? QString::fromStdString(UtilitySevereIndices::indices[indexIndex].key).toUpper()
            : QString{"VALUE"};
        valueText = keyUpper + " " + QString::number(value, 'f', 2);
    }
    const auto coordText =
        QString::number(std::abs(lat), 'f', 2) + degree + (lat >= 0.0 ? " N" : " S") + "   " +
        QString::number(std::abs(lon), 'f', 2) + degree + (lon >= 0.0 ? " E" : " W");

    hoverLabel->setText(coordText + "\n" + valueText);
    hoverLabel->adjustSize();
    hoverLabel->move(12, 12);
    hoverLabel->show();
    hoverLabel->raise();
}

void IndexViewer::onFrameShown(int localIndex) {
    const auto bytes = animBar.frameAt(localIndex);
    if (bytes.isEmpty()) {
        return;
    }
    renderedBytes = bytes;
    image.setBytesKeepView(bytes);
    if (localIndex >= 0 && localIndex < static_cast<int>(frameStatuses.size())) {
        status = frameStatuses[localIndex];
        setTitle("Parametric Index Viewer - " + status);
        legend.setBytes(buildLegend(comboIndex.getIndex(), frameRanges[localIndex].first, frameRanges[localIndex].second));
    }
    sampleGridPath = (localIndex >= 0 && localIndex < static_cast<int>(frameGridPaths.size()))
        ? frameGridPaths[localIndex] : string{};
    refreshHover();
    const auto globalIndex = animBar.globalIndexAt(localIndex);
    if (globalIndex >= 0) {
        comboForecastHour.block();
        comboForecastHour.setIndex(globalIndex);
        comboForecastHour.unblock();
    }
}

void IndexViewer::onRangeRequested(int rangeStart, int rangeEnd) {
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
    sweepFrames.assign(indices.size(), QByteArray{});
    sweepStatuses.assign(indices.size(), string{});
    sweepRanges.assign(indices.size(), std::pair<double, double>{0.0, 0.0});
    sweepGridPaths.assign(indices.size(), string{});
    renderNextAnimFrame(0, animGeneration);
}

void IndexViewer::renderNextAnimFrame(size_t sweepIndex, int generation) {
    if (generation != animGeneration) {
        return;   // index / region / run / cycle changed mid-sweep - abandon it
    }
    if (sweepIndex >= sweepIndices.size()) {
        vector<QByteArray> validFrames;
        vector<int> validIndices;
        vector<string> validStatuses;
        vector<std::pair<double, double>> validRanges;
        vector<string> validGridPaths;
        for (size_t k = 0; k < sweepFrames.size(); k += 1) {
            if (!sweepFrames[k].isEmpty()) {
                validFrames.push_back(sweepFrames[k]);
                validIndices.push_back(sweepIndices[k]);
                validStatuses.push_back(sweepStatuses[k]);
                validRanges.push_back(sweepRanges[k]);
                validGridPaths.push_back(sweepGridPaths[k]);
            }
        }
        if (validFrames.size() < 2) {
            animBar.cancelPending();
            setTitle("Parametric Index Viewer - animation render failed");
            return;
        }
        frameStatuses = validStatuses;
        frameRanges = validRanges;
        frameGridPaths = validGridPaths;
        animBar.setFrames(validFrames, validIndices);
        return;
    }

    const auto indexIndex = comboIndex.getIndex();
    const auto regionIndex = comboRegion.getIndex();
    const auto runIndex = comboRun.getIndex();
    const string runId = (runIndex >= 0 && runIndex < static_cast<int>(runOptions.size()))
        ? runOptions[runIndex].second : string{};
    const auto allHours = comboForecastHour.getItems();
    const auto globalIndex = sweepIndices[sweepIndex];
    const auto hour = (globalIndex >= 0 && globalIndex < static_cast<int>(allHours.size()))
        ? allHours[globalIndex] : string{};

    new FutureVoid{this,
        [this, indexIndex, regionIndex, hour, runId] {
            string localStatus;
            string localGridPath;
            double lo = 0.0;
            double hi = 0.0;
            const auto path = UtilitySevereIndices::render(indexIndex, regionIndex, hour, runId,
                                                            localStatus, lo, hi, localGridPath);
            pendingFrame.clear();
            if (!path.empty()) {
                QFile file{QString::fromStdString(path)};
                if (file.open(QIODevice::ReadOnly)) {
                    pendingFrame = file.readAll();
                    file.close();
                }
            }
            pendingStatus = localStatus;
            pendingRange = {lo, hi};
            pendingGridPath = localGridPath;
        },
        [this, sweepIndex, generation] {
            if (generation != animGeneration) {
                return;
            }
            sweepFrames[sweepIndex] = pendingFrame;
            sweepStatuses[sweepIndex] = pendingStatus;
            sweepRanges[sweepIndex] = pendingRange;
            sweepGridPaths[sweepIndex] = pendingGridPath;
            pendingFrame.clear();
            renderNextAnimFrame(sweepIndex + 1, generation);
        }};
}

void IndexViewer::onSave() {
    const auto valid = animBar.frameCount();
    if (renderedBytes.isEmpty() && valid == 0) {
        return;
    }
    const auto indexIndex = comboIndex.getIndex();
    const auto indexKey = (indexIndex >= 0 && indexIndex < static_cast<int>(UtilitySevereIndices::indices.size()))
        ? UtilitySevereIndices::indices[indexIndex].key : string{"index"};
    // run date / cycle / f-hour / valid time from the screen's own status lines
    // (first and last loaded frame for an animation) + product + region
    const auto looping = valid >= 2 && !frameStatuses.empty();
    const auto firstStatus = QString::fromStdString(looping ? frameStatuses.front() : status);
    const auto lastStatus = QString::fromStdString(looping ? frameStatuses.back() : status);
    const auto suggested = UtilityAnimationExport::modelName(firstStatus, lastStatus,
        QString::fromStdString(indexKey) + "_" + UtilityAnimationExport::slug(QString::fromStdString(comboRegion.getValue()), 30));

    // the loaded frames when a loop has been rendered, else the single image on screen;
    // the Save dialog offers every available format and reports any failure
    // exports carry the SPC-style information bars (model, product, region, run, hour,
    // valid time); each frame of a loop gets its own, read from that frame's status line
    const auto exportFrames = valid < 2 ? vector<QByteArray>{}
        : (looping ? UtilityAnimationExport::withHeaders(animBar.loadedFrames(), frameStatuses, QString{})
                   : animBar.loadedFrames());
    const auto exportStill = UtilityAnimationExport::withHeader(renderedBytes, QString::fromStdString(status), QString{});
    UtilityAnimationExport::saveWithDialog(this, exportFrames, frameDelayMs, exportStill, suggested, QByteArray{}, false);
}

QByteArray IndexViewer::buildLegend(int indexIndex, double clipLo, double clipHi) const {
    if (indexIndex < 0 || indexIndex >= static_cast<int>(UtilitySevereIndices::indices.size())) {
        return {};
    }
    const auto& entry = UtilitySevereIndices::indices[indexIndex];

    struct Stop {
        double value;
        QColor color;
    };
    std::vector<Stop> stops;
    const auto lines = QString::fromStdString(entry.colorMap).split('\n', Qt::SkipEmptyParts);
    for (const auto& line : lines) {
        const auto parts = line.split(' ', Qt::SkipEmptyParts);
        if (parts.size() >= 4) {
            stops.push_back({parts[0].toDouble(), QColor{parts[1].toInt(), parts[2].toInt(), parts[3].toInt()}});
        }
    }
    if (stops.size() < 2) {
        return {};
    }

    auto colorAt = [&stops] (double value) {
        if (value <= stops.front().value) {
            return stops.front().color;
        }
        for (size_t i = 1; i < stops.size(); i += 1) {
            if (value <= stops[i].value) {
                const auto& low = stops[i - 1];
                const auto& high = stops[i];
                const auto t = (high.value == low.value) ? 0.0 : (value - low.value) / (high.value - low.value);
                return QColor{
                    static_cast<int>(low.color.red() + t * (high.color.red() - low.color.red())),
                    static_cast<int>(low.color.green() + t * (high.color.green() - low.color.green())),
                    static_cast<int>(low.color.blue() + t * (high.color.blue() - low.color.blue()))};
            }
        }
        return stops.back().color;
    };

    // clip the legend to the value range actually present, keeping a small margin
    auto vmin = stops.front().value;
    auto vmax = stops.back().value;
    if (clipHi - clipLo > 0.05) {
        const auto margin = (clipHi - clipLo) * 0.03;
        vmin = std::max(vmin, clipLo - margin);
        vmax = std::min(vmax, clipHi + margin);
    }
    const auto span = vmax - vmin;
    if (span <= 0.0) {
        return {};
    }

    // rebuild the stop list against the (possibly clipped) range
    std::vector<Stop> shown;
    shown.push_back({vmin, colorAt(vmin)});
    for (const auto& stop : stops) {
        if (stop.value > vmin && stop.value < vmax) {
            shown.push_back(stop);
        }
    }
    shown.push_back({vmax, colorAt(vmax)});
    stops = shown;

    const int barWidth = 24;
    const int barX = 6;
    const int width = 108;
    const int height = 400;
    const int top = 12;
    const int barHeight = height - 2 * top;

    QImage image{width, height, QImage::Format_ARGB32};
    image.fill(Qt::transparent);
    QPainter painter{&image};
    painter.setRenderHint(QPainter::Antialiasing, false);

    for (int y = 0; y < barHeight; y += 1) {
        const auto value = vmax - span * y / barHeight;
        QColor color = stops.back().color;
        for (size_t i = 1; i < stops.size(); i += 1) {
            if (value <= stops[i].value) {
                const auto& low = stops[i - 1];
                const auto& high = stops[i];
                const auto t = (high.value == low.value) ? 0.0 : (value - low.value) / (high.value - low.value);
                color = QColor{
                    static_cast<int>(low.color.red() + t * (high.color.red() - low.color.red())),
                    static_cast<int>(low.color.green() + t * (high.color.green() - low.color.green())),
                    static_cast<int>(low.color.blue() + t * (high.color.blue() - low.color.blue()))};
                break;
            }
        }
        painter.fillRect(barX, top + y, barWidth, 1, color);
    }

    painter.setPen(palette().color(QPalette::WindowText));
    auto font = painter.font();
    font.setPointSize(9);
    painter.setFont(font);
    for (const auto& stop : stops) {
        const auto y = top + static_cast<int>((vmax - stop.value) / span * barHeight);
        painter.drawLine(barX, y, barX + barWidth + 3, y);
        painter.drawText(barX + barWidth + 7, y + 4, QString::number(stop.value, 'g', 3));
    }
    painter.drawText(barX, height - 3, QString::fromStdString(entry.units));
    painter.end();

    QByteArray bytes;
    QBuffer buffer{&bytes};
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    return bytes;
}

void IndexViewer::moveBack() {
    const auto index = comboForecastHour.getIndex();
    if (index > 0) {
        comboForecastHour.setIndex(index - 1);
    }
}

void IndexViewer::moveForward() {
    const auto index = comboForecastHour.getIndex();
    if (index + 1 < comboForecastHour.getView()->count()) {
        comboForecastHour.setIndex(index + 1);
    }
}

void IndexViewer::resizeEventCustom() {
    // ZoomImage re-fits itself on resize while the user has not zoomed
}
