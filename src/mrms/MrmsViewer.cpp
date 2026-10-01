// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "mrms/MrmsViewer.h"
#include <algorithm>
#include <cmath>
#include <memory>
#include <numbers>
#include <QFont>
#include <QPainter>
#include <QTimeZone>
#include "objects/FutureVoid.h"
#include "radar/Projection.h"
#include "settings/Location.h"
#include "util/To.h"
#include "util/UtilityUI.h"

namespace {
    vector<string> productLabels() {
        vector<string> labels;
        for (const auto& product : UtilityMrms::products()) {
            labels.push_back(product.label);
        }
        return labels;
    }

    const int loopCounts[] = {6, 12, 24};

    string timeText(const QDateTime& utc) {
        return utc.toString("ddd MMM d hh:mm").toStdString() + "Z  (" +
            utc.toLocalTime().toString("h:mm AP").toStdString() + " " +
            QTimeZone::systemTimeZone().abbreviation(utc.toLocalTime()).toStdString() + ")";
    }

    // what one background job hands back to the UI thread
    struct Loaded {
        vector<UtilityMrms::Scan> scans;
        UtilityMrms::Frame frame;
        string error;
    };
}

MrmsViewer::MrmsViewer(Window * parent)
    : Window{parent}
    , comboProduct{this, productLabels()}
    , comboScan{this, {"Loading..."}}
    , backForward{this, [this] { moveScan(1); }, [this] { moveScan(-1); }}
    , comboLoop{this, {"Loop: last 6 scans", "Loop: last 12 scans", "Loop: last 24 scans"}}
    , buttonLoop{this, Icon::Play, "Loop"}
    , textStatus{this, ""}
{
    setAttribute(Qt::WA_DeleteOnClose);
    setTitle("MRMS");
    const auto dimens = UtilityUI::getScreenBounds();
    const auto side = std::max(300, std::min(dimens[0] - 20, dimens[1] - 160));
    radar = new NexradWidget{
        this, 0, 1, true, Location::radarSite(), side, side,
        [] ([[maybe_unused]] int pane, [[maybe_unused]] const string& prod) {},
        [] ([[maybe_unused]] int pane, [[maybe_unused]] const string& sector) {},
        [] ([[maybe_unused]] double z, [[maybe_unused]] int pane) {},
        [] ([[maybe_unused]] double x, [[maybe_unused]] double y, [[maybe_unused]] int pane) {},
        [] {}};
    radar->setFixedSize(side, side);
    radar->nexradState.setRadar(Location::radarSite());
    radar->nexradState.reset();
    radar->nexradState.zoom = 0.14;   // the whole CONUS grid
    radar->nexradDraw.initGeom();
    radar->dataLayer = [this] (QPainter& painter) { paintData(painter); };
    radar->topLayer = [this] (QPainter& painter) { paintLegend(painter); };

    comboProduct.connect([this] { stopLoop(); loadScans(); });
    comboScan.connect([this] { stopLoop(); showScan(comboScan.getIndex()); });
    buttonLoop.getView()->setToolTip("Play the latest scans as a loop (they are downloaded and decoded first)");
    buttonLoop.connect([this] { looping ? stopLoop() : startLoop(); });
    loopTimer.setInterval(350);
    QObject::connect(&loopTimer, &QTimer::timeout, this, [this] { stepLoop(); });

    rowTop.addWidget(comboProduct);
    rowTop.addLayout(backForward);
    rowTop.addWidget(comboScan);
    rowTop.addWidget(comboLoop);
    rowTop.addWidget(buttonLoop);
    rowTop.addWidget(textStatus);
    rowTop.addStretch();
    box.addLayout(rowTop);
    box.addWidgetReal(radar, 0, Qt::AlignTop | Qt::AlignLeft);
    box.addStretch();
    box.getAndShow(this);
    loadScans();
}

const UtilityMrms::Product& MrmsViewer::product() const {
    const auto index = std::clamp(comboProduct.getIndex(), 0, static_cast<int>(UtilityMrms::products().size()) - 1);
    return UtilityMrms::products()[static_cast<size_t>(index)];
}

void MrmsViewer::closeEventCustom() {
    closed = true;
    loopTimer.stop();
}

void MrmsViewer::loadScans() {
    const auto gen = ++generation;
    const auto chosen = product();
    textStatus.setText("Loading " + chosen.label + "...");
    auto result = std::make_shared<Loaded>();
    new FutureVoid{this,
        [chosen, result] {
            if (UtilityMrms::scans(chosen, result->scans, result->error)) {
                UtilityMrms::frame(chosen, result->scans.back(), result->frame, result->error);
            }
        },
        [this, gen, result] {
            if (closed || gen != generation) {
                return;
            }
            scans = result->scans;
            comboToScan.clear();
            vector<string> labels;
            for (int i = static_cast<int>(scans.size()) - 1; i >= 0; i -= 1) {   // newest first
                labels.push_back(timeText(scans[static_cast<size_t>(i)].utc));
                comboToScan.push_back(i);
            }
            comboScan.block();
            comboScan.setList(labels.empty() ? vector<string>{"No scans"} : labels);
            comboScan.setIndex(0);
            comboScan.unblock();
            if (!result->error.empty()) {
                textStatus.setText(result->error);
                return;
            }
            setFrame(result->frame);
        }};
}

void MrmsViewer::showScan(int comboIndex) {
    if (comboIndex < 0 || comboIndex >= static_cast<int>(comboToScan.size())) {
        return;
    }
    const auto gen = ++generation;
    const auto chosen = product();
    const auto scan = scans[static_cast<size_t>(comboToScan[static_cast<size_t>(comboIndex)])];
    textStatus.setText("Loading " + timeText(scan.utc) + "...");
    auto result = std::make_shared<Loaded>();
    new FutureVoid{this,
        [chosen, scan, result] { UtilityMrms::frame(chosen, scan, result->frame, result->error); },
        [this, gen, result] {
            if (closed || gen != generation) {
                return;
            }
            if (!result->error.empty()) {
                textStatus.setText(result->error);
                return;
            }
            setFrame(result->frame);
        }};
}

void MrmsViewer::moveScan(int step) {
    const auto next = comboScan.getIndex() + step;
    if (next >= 0 && next < static_cast<int>(comboToScan.size())) {
        stopLoop();
        comboScan.setIndex(static_cast<size_t>(next));   // its signal loads the scan
    }
}

void MrmsViewer::setFrame(const UtilityMrms::Frame& frame) {
    current = frame;
    currentIndices = frame.indices();
    currentColors = UtilityMrms::colorTable(product());
    setTitle("MRMS - " + product().label + " - " + timeText(frame.utc));
    textStatus.setText(product().label + "  " + timeText(frame.utc));
    radar->update();
}

void MrmsViewer::startLoop() {
    if (scans.empty()) {
        return;
    }
    const auto gen = ++generation;
    const auto chosen = product();
    const int wanted = loopCounts[std::clamp(comboLoop.getIndex(), 0, 2)];
    const int newestIndex = comboToScan.empty() ? 0 : comboToScan[static_cast<size_t>(std::max(0, comboScan.getIndex()))];
    const int first = std::max(0, newestIndex - wanted + 1);
    auto list = std::make_shared<vector<UtilityMrms::Scan>>(scans.begin() + first, scans.begin() + newestIndex + 1);
    loopFrames.clear();
    looping = true;
    buttonLoop.setText("Stop");
    // decode one scan at a time so the status can say how far along it is
    auto loadNext = std::make_shared<std::function<void(size_t)>>();
    *loadNext = [this, gen, chosen, list, loadNext] (size_t index) {
        if (closed || gen != generation || !looping) {
            return;
        }
        if (index >= list->size()) {
            loopPosition = 0;
            textStatus.setText(product().label + "  loop of " + To::string(static_cast<int>(loopFrames.size())) + " scans");
            loopTimer.start();
            return;
        }
        textStatus.setText("Loading loop frame " + To::string(static_cast<int>(index) + 1) + " of " + To::string(static_cast<int>(list->size())) + "...");
        auto result = std::make_shared<Loaded>();
        new FutureVoid{this,
            [chosen, list, index, result] { UtilityMrms::frame(chosen, (*list)[index], result->frame, result->error); },
            [this, gen, index, result, loadNext] {
                if (closed || gen != generation || !looping) {
                    return;
                }
                if (!result->error.empty()) {
                    textStatus.setText(result->error);
                    stopLoop();
                    return;
                }
                loopFrames.push_back(result->frame);
                (*loadNext)(index + 1);
            }};
    };
    (*loadNext)(0);
}

void MrmsViewer::stopLoop() {
    if (looping) {
        ++generation;   // abandons a loop that is still loading
    }
    looping = false;
    loopTimer.stop();
    buttonLoop.setText("Loop");
}

void MrmsViewer::stepLoop() {
    if (loopFrames.empty()) {
        return;
    }
    loopPosition = (loopPosition + 1) % loopFrames.size();
    // the last frame lingers a little so the end of the loop is visible
    loopTimer.setInterval(loopPosition + 1 == loopFrames.size() ? 1100 : 350);
    setFrame(loopFrames[loopPosition]);
}

// Draws the scan at the grid's own resolution: every screen pixel is turned back into a latitude / longitude through
// the radar widget's projection and looked up in the grid. That projection is Mercator, so longitude depends only on the
// pixel's column and latitude only on its row - two small tables, then a plain lookup per pixel.
void MrmsViewer::paintData(QPainter& painter) {
    if (currentIndices.isEmpty()) {
        return;
    }
    const auto& pn = radar->nexradState.getPn();
    // the projection is linear in longitude and in Mercator y (degrees): find its coefficients from three map points
    const auto project = [&pn] (double lat, double lon) {
        return Projection::computeMercatorNumbersFromLatLon(LatLon{lat, lon}.reverseLon(), pn);
    };
    const auto mercator = [] (double lat) {
        return 180.0 / std::numbers::pi * std::log(std::tan(std::numbers::pi / 4.0 + lat * std::numbers::pi / 360.0));
    };
    const auto a = project(30.0, -100.0);
    const auto b = project(30.0, -99.0);
    const auto c = project(40.0, -100.0);
    const double ax = b[0] - a[0];
    const double bx = a[0] + 100.0 * ax;
    const double ay = (c[1] - a[1]) / (mercator(40.0) - mercator(30.0));
    const double by = a[1] - ay * mercator(30.0);

    const auto inverse = painter.combinedTransform().inverted();   // device pixel -> projected coordinates
    const double ratio = painter.device()->devicePixelRatio();
    // the rectangle of the paint device the radar occupies (the device itself can be a bigger window pixmap)
    const QRect viewport = painter.viewport();
    const int logicalWidth = viewport.width();
    const int logicalHeight = viewport.height();
    const int pixelWidth = static_cast<int>(std::lround(logicalWidth * ratio));
    const int pixelHeight = static_cast<int>(std::lround(logicalHeight * ratio));
    vector<int> columnOf(static_cast<size_t>(pixelWidth));
    for (int i = 0; i < pixelWidth; i += 1) {
        const double x = inverse.m11() * (viewport.left() + (i + 0.5) / ratio) + inverse.dx();
        const double lon = (x - bx) / ax;
        const int column = static_cast<int>(std::floor((lon - UtilityMrms::west) / UtilityMrms::cell));
        columnOf[static_cast<size_t>(i)] = (column >= 0 && column < UtilityMrms::columns) ? column : -1;
    }
    QImage image{pixelWidth, pixelHeight, QImage::Format_ARGB32_Premultiplied};
    const auto * cells = reinterpret_cast<const uchar *>(currentIndices.constData());
    for (int j = 0; j < pixelHeight; j += 1) {
        const double y = inverse.m22() * (viewport.top() + (j + 0.5) / ratio) + inverse.dy();
        const double lat = std::atan(std::sinh((y - by) / ay * std::numbers::pi / 180.0)) * 180.0 / std::numbers::pi;
        const int row = static_cast<int>(std::floor((UtilityMrms::north - lat) / UtilityMrms::cell));
        auto * out = reinterpret_cast<QRgb *>(image.scanLine(j));
        if (row < 0 || row >= UtilityMrms::rows) {
            std::fill(out, out + pixelWidth, qRgba(0, 0, 0, 0));
            continue;
        }
        const auto * source = cells + static_cast<qsizetype>(row) * UtilityMrms::columns;
        for (int i = 0; i < pixelWidth; i += 1) {
            const int column = columnOf[static_cast<size_t>(i)];
            out[i] = column < 0 ? qRgba(0, 0, 0, 0) : currentColors[source[column]];
        }
    }
    // the image is one pixel per device pixel; drawn through the current transform onto exactly the area it was sampled for
    painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
    painter.drawImage(inverse.mapRect(QRectF{viewport}), image, QRectF{image.rect()});
}

void MrmsViewer::paintLegend(QPainter& painter) {
    const auto& stops = product().stops;
    const double boxWidth = 38.0;
    const double x0 = -490.0;
    const double y0 = 715.0;
    QFont font{painter.font()};
    font.setPointSizeF(9.0);
    painter.setFont(font);
    for (size_t i = 0; i < stops.size(); i += 1) {
        const QRectF box{x0 + static_cast<double>(i) * boxWidth, y0, boxWidth, 14.0};
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor{stops[i].r, stops[i].g, stops[i].b});
        painter.drawRect(box);
        painter.setPen(Qt::white);
        painter.drawText(QRectF{box.left(), y0 + 14.0, boxWidth, 16.0}, Qt::AlignCenter,
                         QString::number(stops[i].value, 'g', 3));
    }
    painter.drawText(QPointF{x0 + static_cast<double>(stops.size()) * boxWidth + 8.0, y0 + 11.0},
                     QString::fromStdString(product().units));
}
