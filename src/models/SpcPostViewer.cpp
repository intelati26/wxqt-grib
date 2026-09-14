// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "models/SpcPostViewer.h"
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
#include <QString>
#include <QStringList>
#include "misc/ImageViewer.h"
#include "models/UtilitySpcPost.h"
#include "objects/FutureVoid.h"
#include "objects/SampleGrid.h"
#include "objects/UtilityApng.h"
#include "objects/UtilityJxl.h"
#include "util/To.h"

namespace {
    constexpr int frameDelayMs = 400;      // per-frame dwell for the exported APNG
    constexpr size_t maxAnimFrames = 24;   // caps a render sweep to at most this many GDAL renders
}

SpcPostViewer::SpcPostViewer(Window * parent)
    : Window{parent}
    , image{this}
    , legend{this, Normal}
    , comboProduct{this, UtilitySpcPost::productLabels()}
    , comboDomain{this, UtilitySpcPost::domainLabels()}
    , comboBackground{this, UtilitySpcPost::backgroundLabels()}
    , comboRun{this, {"Latest"}}
    , comboForecastHour{this, {}}
    , backForward{this, [this] { moveBack(); }, [this] { moveForward(); }}
    , animBar{this,
        [this] (int start, int end) { onRangeRequested(start, end); },
        [this] (int local) { onFrameShown(local); },
        [this] (int global) { onScrub(global); },
        [this] { onSave(); }}
{
    setTitle("SPC Post Slideshow");
    image.setCrosshairMode(true);
    QObject::connect(&image, &ZoomImage::doubleClicked, this, [this] { openFullImage(); });
    QObject::connect(&image, &ZoomImage::hovered, this, [this] (double fx, double fy) { onHover(fx, fy); });
    QObject::connect(&image, &ZoomImage::hoverEnded, this, [this] { onHoverEnded(); });
    comboProduct.connect([this] { invalidateAnimation(); refreshRunOptions(); });
    comboDomain.connect([this] { invalidateAnimation(); reload(); });
    // "State Lines" and "NWS CWAs" default checked (the user's stated
    // typical SPC-site setup); "County Lines"/"Highways" opt-in extras
    for (const auto& [label, flag] : UtilitySpcPost::boundaryOptions()) {
        auto * checkBox = new QCheckBox{QString::fromStdString(label), this};
        checkBox->setChecked(flag == UtilitySpcPost::BoundaryState || flag == UtilitySpcPost::BoundaryCwa);
        QObject::connect(checkBox, &QCheckBox::stateChanged, this, [this] { invalidateAnimation(); reload(); });
        boundaryChecks.emplace_back(checkBox, flag);
    }
    comboBackground.connect([this] { invalidateAnimation(); reload(); });
    comboRun.connect([this] { invalidateAnimation(); refreshForecastHours(); });
    comboForecastHour.connect([this] {
        animBar.stopIfAnimating();
        reload();
    });

    legend.setBytes(buildLegend());

    boxTop.addLayout(backForward);
    boxTop.addWidget(comboProduct);
    boxTop.addWidget(comboDomain);
    for (const auto& entry : boundaryChecks) {
        boxTop.addWidgetReal(entry.first);
    }
    boxTop.addWidget(comboBackground);
    boxTop.addWidget(comboRun);
    boxTop.addWidget(comboForecastHour);
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

    refreshRunOptions();
}

int SpcPostViewer::boundaryFlags() const {
    int flags = 0;
    for (const auto& entry : boundaryChecks) {
        if (entry.first->isChecked()) {
            flags |= entry.second;
        }
    }
    return flags;
}

void SpcPostViewer::invalidateAnimation() {
    animGeneration += 1;
    animBar.stopIfAnimating();
    animBar.clearFrames();
}

// fills in the run-date/cycle choices for the currently selected product, then
// chains into refreshForecastHours() once they land - both are server
// directory-listing queries, so both run in the background.
void SpcPostViewer::refreshRunOptions() {
    const auto productIndex = comboProduct.getIndex();
    const auto generation = animGeneration;
    setTitle("SPC Post Slideshow - loading run list...");
    new FutureVoid{this,
        [this, productIndex] { runOptionsList = UtilitySpcPost::runOptions(productIndex); },
        [this, generation] {
            if (generation != animGeneration) {
                return;   // product changed again while this fetch was in flight
            }
            vector<string> labels;
            for (const auto& option : runOptionsList) {
                labels.push_back(option.first);
            }
            if (labels.empty()) {
                labels.push_back("Latest");
            }
            comboRun.block();
            comboRun.setList(labels);
            comboRun.setIndex(0);
            comboRun.unblock();
            refreshForecastHours();
        }};
}

void SpcPostViewer::refreshForecastHours() {
    const auto productIndex = comboProduct.getIndex();
    const auto runIndex = comboRun.getIndex();
    const string runId = (runIndex >= 0 && runIndex < static_cast<int>(runOptionsList.size()))
        ? runOptionsList[runIndex].second : string{};
    const auto generation = animGeneration;
    setTitle("SPC Post Slideshow - loading...");
    new FutureVoid{this,
        [this, productIndex, runId] { pendingHours = UtilitySpcPost::forecastHours(productIndex, runId); },
        [this, generation] {
            if (generation != animGeneration) {
                return;
            }
            const auto previous = comboForecastHour.getValue();
            comboForecastHour.block();
            comboForecastHour.setList(pendingHours);
            int keep = 0;
            for (int i = 0; i < static_cast<int>(pendingHours.size()); i += 1) {
                if (pendingHours[i] == previous) {
                    keep = i;
                    break;
                }
            }
            if (!pendingHours.empty()) {
                comboForecastHour.setIndex(keep);
            }
            comboForecastHour.unblock();

            animBar.setAvailableLabels(pendingHours);
            animBar.setSliderPosition(keep);

            if (pendingHours.empty()) {
                setTitle("SPC Post Slideshow - no data available for this run");
                renderedBytes.clear();
                return;
            }
            reload();
        }};
}

void SpcPostViewer::reload() {
    const auto productIndex = comboProduct.getIndex();
    const auto domainIndex = comboDomain.getIndex();
    const auto boundaryIndex = boundaryFlags();
    const auto backgroundIndex = comboBackground.getIndex();
    const auto forecastHour = comboForecastHour.getValue();
    if (forecastHour.empty()) {
        return;
    }
    const auto runIndex = comboRun.getIndex();
    const string runId = (runIndex >= 0 && runIndex < static_cast<int>(runOptionsList.size()))
        ? runOptionsList[runIndex].second : string{};
    setTitle("SPC Post Slideshow - loading...");
    sampleGridPath.clear();
    refreshHover();
    new FutureVoid{this,
        [this, productIndex, domainIndex, boundaryIndex, backgroundIndex, forecastHour, runId] {
            pngPath = UtilitySpcPost::render(productIndex, domainIndex, boundaryIndex, backgroundIndex,
                                             forecastHour, runId, status, sampleGridPath);
        },
        [this] {
            setTitle("SPC Post Slideshow - " + status);
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

void SpcPostViewer::openFullImage() {
    if (!renderedBytes.isEmpty()) {
        new ImageViewer{this, renderedBytes, status.empty() ? string{"SPC Post"} : status};
    }
}

void SpcPostViewer::onScrub(int globalIndex) {
    comboForecastHour.block();
    comboForecastHour.setIndex(globalIndex);
    comboForecastHour.unblock();
    reload();
}

void SpcPostViewer::onHoverEnded() {
    lastHoverFx = -1.0;
    lastHoverFy = -1.0;
    image.clearMarker();
    if (hoverLabel) {
        hoverLabel->hide();
    }
}

void SpcPostViewer::refreshHover() {
    // lastHoverFx >= 0 means the cursor is currently over the image
    if (lastHoverFx >= 0.0) {
        onHover(lastHoverFx, lastHoverFy);
    }
}

void SpcPostViewer::onHover(double fx, double fy) {
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
    const auto valueText = grid.valueAt(lon, lat, value)
        ? QString::number(value, 'f', 0) + "%"
        : QString{"no data"};
    const auto coordText =
        QString::number(std::abs(lat), 'f', 2) + degree + (lat >= 0.0 ? " N" : " S") + "   " +
        QString::number(std::abs(lon), 'f', 2) + degree + (lon >= 0.0 ? " E" : " W");

    hoverLabel->setText(coordText + "\n" + valueText);
    hoverLabel->adjustSize();
    hoverLabel->move(12, 12);
    hoverLabel->show();
    hoverLabel->raise();
}

void SpcPostViewer::onFrameShown(int localIndex) {
    const auto bytes = animBar.frameAt(localIndex);
    if (bytes.isEmpty()) {
        return;
    }
    renderedBytes = bytes;
    image.setBytesKeepView(bytes);
    if (localIndex >= 0 && localIndex < static_cast<int>(frameStatuses.size())) {
        status = frameStatuses[localIndex];
        setTitle("SPC Post Slideshow - " + status);
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

void SpcPostViewer::onRangeRequested(int rangeStart, int rangeEnd) {
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
    sweepGridPaths.assign(indices.size(), string{});
    renderNextAnimFrame(0, animGeneration);
}

void SpcPostViewer::renderNextAnimFrame(size_t sweepIndex, int generation) {
    if (generation != animGeneration) {
        return;   // product / domain / boundaries / run changed mid-sweep - abandon it
    }
    if (sweepIndex >= sweepIndices.size()) {
        vector<QByteArray> validFrames;
        vector<int> validIndices;
        vector<string> validStatuses;
        vector<string> validGridPaths;
        for (size_t k = 0; k < sweepFrames.size(); k += 1) {
            if (!sweepFrames[k].isEmpty()) {
                validFrames.push_back(sweepFrames[k]);
                validIndices.push_back(sweepIndices[k]);
                validStatuses.push_back(sweepStatuses[k]);
                validGridPaths.push_back(sweepGridPaths[k]);
            }
        }
        if (validFrames.size() < 2) {
            animBar.cancelPending();
            setTitle("SPC Post Slideshow - animation render failed");
            return;
        }
        frameStatuses = validStatuses;
        frameGridPaths = validGridPaths;
        animBar.setFrames(validFrames, validIndices);
        return;
    }

    const auto productIndex = comboProduct.getIndex();
    const auto domainIndex = comboDomain.getIndex();
    const auto boundaryIndex = boundaryFlags();
    const auto backgroundIndex = comboBackground.getIndex();
    const auto runIndex = comboRun.getIndex();
    const string runId = (runIndex >= 0 && runIndex < static_cast<int>(runOptionsList.size()))
        ? runOptionsList[runIndex].second : string{};
    const auto allHours = comboForecastHour.getItems();
    const auto globalIndex = sweepIndices[sweepIndex];
    const auto hour = (globalIndex >= 0 && globalIndex < static_cast<int>(allHours.size()))
        ? allHours[globalIndex] : string{};

    new FutureVoid{this,
        [this, productIndex, domainIndex, boundaryIndex, backgroundIndex, hour, runId] {
            string localStatus;
            string localGridPath;
            const auto path = UtilitySpcPost::render(productIndex, domainIndex, boundaryIndex, backgroundIndex,
                                                      hour, runId, localStatus, localGridPath);
            pendingFrame.clear();
            if (!path.empty()) {
                QFile file{QString::fromStdString(path)};
                if (file.open(QIODevice::ReadOnly)) {
                    pendingFrame = file.readAll();
                    file.close();
                }
            }
            pendingStatus = localStatus;
            pendingGridPath = localGridPath;
        },
        [this, sweepIndex, generation] {
            if (generation != animGeneration) {
                return;
            }
            sweepFrames[sweepIndex] = pendingFrame;
            sweepStatuses[sweepIndex] = pendingStatus;
            sweepGridPaths[sweepIndex] = pendingGridPath;
            pendingFrame.clear();
            renderNextAnimFrame(sweepIndex + 1, generation);
        }};
}

void SpcPostViewer::onSave() {
    const auto valid = animBar.frameCount();
    if (renderedBytes.isEmpty() && valid == 0) {
        return;
    }
    QByteArray outBytes;
    if (valid >= 2) {
        outBytes = UtilityApng::fromFrames(animBar.loadedFrames(), frameDelayMs);
    }
    if (outBytes.isEmpty()) {
        outBytes = renderedBytes;
    }

    const auto productIndex = comboProduct.getIndex();
    const auto productKey = (productIndex >= 0 && productIndex < static_cast<int>(UtilitySpcPost::products.size()))
        ? UtilitySpcPost::products[productIndex].key : string{"spcpost"};
    auto suggested = (valid >= 2)
        ? ("spcpost_" + productKey + "_anim")
        : ("spcpost_" + productKey + "_f" + comboForecastHour.getValue());
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

// static 0-100% probability legend - unlike GribViewer's per-field legend, every
// SPC Post product shares the same fixed percent scale, so this is built once.
QByteArray SpcPostViewer::buildLegend() {
    struct Stop {
        double value;
        QColor color;
    };
    std::vector<Stop> stops;
    const auto lines = QString::fromStdString(UtilitySpcPost::probabilityColorMap()).split('\n', Qt::SkipEmptyParts);
    for (const auto& line : lines) {
        const auto parts = line.split(' ', Qt::SkipEmptyParts);
        if (parts.size() >= 4) {
            const auto alpha = parts.size() >= 5 ? parts[4].toInt() : 255;
            if (alpha == 0) {
                continue;   // skip the transparent "below 10%" entry
            }
            stops.push_back({parts[0].toDouble(), QColor{parts[1].toInt(), parts[2].toInt(), parts[3].toInt()}});
        }
    }
    if (stops.size() < 2) {
        return {};
    }

    const int barWidth = 24;
    const int barX = 6;
    const int width = 108;
    const int height = 400;
    const int top = 12;
    const int barHeight = height - 2 * top;
    const auto vmin = stops.front().value;
    const auto vmax = stops.back().value;
    const auto span = vmax - vmin;

    QImage img{width, height, QImage::Format_ARGB32};
    img.fill(Qt::transparent);
    QPainter painter{&img};
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

    painter.setPen(Qt::black);
    auto font = painter.font();
    font.setPointSize(9);
    painter.setFont(font);
    for (const auto& stop : stops) {
        const auto y = top + static_cast<int>((vmax - stop.value) / span * barHeight);
        painter.drawLine(barX, y, barX + barWidth + 3, y);
        painter.drawText(barX + barWidth + 7, y + 4, QString::number(stop.value, 'g', 3));
    }
    painter.drawText(barX, height - 3, "%");
    painter.end();

    QByteArray bytes;
    QBuffer buffer{&bytes};
    buffer.open(QIODevice::WriteOnly);
    img.save(&buffer, "PNG");
    return bytes;
}

void SpcPostViewer::moveBack() {
    const auto index = comboForecastHour.getIndex();
    if (index > 0) {
        comboForecastHour.setIndex(index - 1);
    }
}

void SpcPostViewer::moveForward() {
    const auto index = comboForecastHour.getIndex();
    if (index + 1 < comboForecastHour.getView()->count()) {
        comboForecastHour.setIndex(index + 1);
    }
}

void SpcPostViewer::resizeEventCustom() {
    // ZoomImage re-fits itself on resize while the user has not zoomed
}
