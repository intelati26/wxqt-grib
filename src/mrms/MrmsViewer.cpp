// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "mrms/MrmsViewer.h"
#include <algorithm>
#include <cmath>
#include <memory>
#include <numbers>
#include <QBuffer>
#include <QFont>
#include <QImage>
#include <QMouseEvent>
#include <QPainter>
#include <QTimeZone>
#include "models/UtilityGrib.h"
#include "objects/FutureVoid.h"
#include "objects/UtilityAnimationExport.h"
#include "radar/Projection.h"
#include "settings/Location.h"
#include "util/To.h"
#include "util/Utility.h"
#include "util/UtilityUI.h"

namespace {
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
    , comboProduct{this, {"Loading products..."}}
    , comboScan{this, {"Loading..."}}
    , backForward{this, [this] { moveScan(1); }, [this] { moveScan(-1); }}
    , comboLoop{this, {"Loop: last 6 scans", "Loop: last 12 scans", "Loop: last 24 scans"}}
    , comboUnits{this, {"US units", "Metric units"}}
    , comboAuto{this, {"Auto-update: 2 min", "Auto-update: 5 min", "Auto-update: off"}}
    , buttonLoop{this, Icon::Play, "Loop"}
    , buttonSave{this, Icon::None, "Save"}
    , shortcutSave{QKeySequence{"Ctrl+S"}, this}
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
        [this] (double z, [[maybe_unused]] int pane) { changeZoom(z); },
        [this] (double x, double y, [[maybe_unused]] int pane) { changePosition(x, y); },
        [] {}};
    radar->setFixedSize(side, side);   // resized to the window by fitRadar()
    radar->nexradState.setRadar(Location::radarSite());
    radar->nexradState.reset();
    radar->nexradState.zoom = 0.14;   // the whole CONUS grid
    radar->nexradDraw.initGeom();
    radar->setMouseTracking(true);
    radar->installEventFilter(this);
    hoverLabel = new QLabel{radar};
    hoverLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    hoverLabel->setStyleSheet("QLabel { background-color: rgba(15, 15, 15, 205); color: #f2f2f2; padding: 4px 8px; border-radius: 3px; }");
    hoverLabel->hide();
    radar->dataLayer = [this] (QPainter& painter) { paintData(painter); };
    radar->topLayer = [this] (QPainter& painter) { paintLegend(painter); };

    comboProduct.connect([this] {
        stopLoop();
        userPickedProduct = true;
        Utility::writePref("MRMS_LAST_PRODUCT", product().id);
        loadScans();
    });
    comboUnits.setIndex(static_cast<size_t>(Utility::readPref("MRMS_UNITS", "us") == "metric" ? 1 : 0));
    comboUnits.connect([this] {
        Utility::writePref("MRMS_UNITS", us() ? "us" : "metric");
        radar->update();
        textStatus.setText(product().label + "  " + timeText(current.utc));
    });
    comboLoop.setIndex(static_cast<size_t>(std::clamp(Utility::readPrefInt("MRMS_LOOP", 0), 0, 2)));
    comboLoop.connect([this] { Utility::writePrefInt("MRMS_LOOP", comboLoop.getIndex()); });
    comboAuto.setIndex(static_cast<size_t>(std::clamp(Utility::readPrefInt("MRMS_AUTO", 0), 0, 2)));
    const auto applyAuto = [this] {
        static const int seconds[] = {120, 300, 0};
        const int chosen = seconds[std::clamp(comboAuto.getIndex(), 0, 2)];
        autoTimer.stop();
        if (chosen > 0) {
            autoTimer.start(chosen * 1000);
        }
    };
    comboAuto.connect([this, applyAuto] { Utility::writePrefInt("MRMS_AUTO", comboAuto.getIndex()); applyAuto(); });
    QObject::connect(&autoTimer, &QTimer::timeout, this, [this] { refreshNewest(); });
    applyAuto();
    comboScan.connect([this] { stopLoop(); showScan(comboScan.getIndex()); });
    buttonLoop.getView()->setToolTip("Play the latest scans as a loop (they are downloaded and decoded first)");
    buttonLoop.connect([this] { looping ? stopLoop() : startLoop(); });
    buttonSave.getView()->setToolTip("Save the map - or the loaded loop - as PNG / animated PNG / WebP ..., with the product, valid time and save time in a border (Ctrl+S)");
    buttonSave.connect([this] { onSave(); });
    shortcutSave.connect([this] { onSave(); });
    loopTimer.setInterval(350);
    QObject::connect(&loopTimer, &QTimer::timeout, this, [this] { stepLoop(); });

    rowTop.addWidget(comboProduct);
    rowTop.addLayout(backForward);
    rowTop.addWidget(comboScan);
    rowTop.addWidget(comboLoop);
    rowTop.addWidget(comboUnits);
    rowTop.addWidget(comboAuto);
    rowTop.addWidget(buttonLoop);
    rowTop.addWidget(buttonSave);
    rowTop.addWidget(textStatus);
    rowTop.addStretch();
    box.addLayout(rowTop);
    box.addWidgetReal(radar, 0, Qt::AlignTop | Qt::AlignLeft);
    box.addStretch();
    box.getAndShow(this);
    fitRadar();
    rebuildProducts(Utility::readPref("MRMS_LAST_PRODUCT", UtilityMrms::products().front().id));
    loadScans();
    // the rest of the server's products, found in the background
    auto found = std::make_shared<vector<UtilityMrms::Product>>();
    new FutureVoid{this,
        [found] { string error; UtilityMrms::discoverMore(*found, error); },
        [this, found] {
            if (closed || found->empty()) {
                return;
            }
            extraProducts = *found;
            // the product saved last time may be one of the newly found ones: switch to it unless another was picked since
            const auto before = product().id;
            rebuildProducts(userPickedProduct ? before : Utility::readPref("MRMS_LAST_PRODUCT", before));
            if (product().id != before) {
                stopLoop();
                loadScans();
            }
        }};
}

void MrmsViewer::rebuildProducts(const string& selectId) {
    static const vector<string> order{"Reflectivity", "Hail", "Rotation", "Precipitation", "Storm structure", "Lightning", "Other products"};
    auto list = UtilityMrms::products();
    list.insert(list.end(), extraProducts.begin(), extraProducts.end());
    const auto rank = [] (const UtilityMrms::Product& p) {
        const auto found = std::find(order.begin(), order.end(), p.group);
        return found == order.end() ? order.size() : static_cast<size_t>(found - order.begin());
    };
    std::stable_sort(list.begin(), list.end(), [&rank] (const auto& a, const auto& b) { return rank(a) < rank(b); });
    productList = list;
    vector<string> labels;
    size_t selected = 0;
    for (size_t i = 0; i < productList.size(); i += 1) {
        labels.push_back(productList[i].group + " - " + productList[i].label);
        if (productList[i].id == selectId) {
            selected = i;
        }
    }
    comboProduct.block();
    comboProduct.setList(labels);
    comboProduct.setIndex(selected);
    comboProduct.unblock();
}

// auto-update: only while the newest scan is the one shown (not looping, not looking at an older scan)
void MrmsViewer::refreshNewest() {
    if (closed || looping || refreshing || comboScan.getIndex() != 0 || scans.empty()) {
        return;
    }
    refreshing = true;
    const auto chosen = product();
    const auto shownUtc = scans.back().utc;
    const auto gen = generation;
    auto result = std::make_shared<Loaded>();
    new FutureVoid{this,
        [chosen, shownUtc, result] {
            if (UtilityMrms::scans(chosen, result->scans, result->error) && result->scans.back().utc > shownUtc) {
                UtilityMrms::frame(chosen, result->scans.back(), result->frame, result->error);
            } else {
                result->frame.packed.clear();
            }
        },
        [this, result, gen] {
            refreshing = false;
            if (closed || gen != generation || result->frame.packed.isEmpty() || !result->error.empty()) {
                return;
            }
            scans = result->scans;
            comboToScan.clear();
            vector<string> labels;
            for (int i = static_cast<int>(scans.size()) - 1; i >= 0; i -= 1) {
                labels.push_back(timeText(scans[static_cast<size_t>(i)].utc));
                comboToScan.push_back(i);
            }
            comboScan.block();
            comboScan.setList(labels);
            comboScan.setIndex(0);
            comboScan.unblock();
            setFrame(result->frame);
        }};
}

const UtilityMrms::Product& MrmsViewer::product() const {
    const auto index = std::clamp(comboProduct.getIndex(), 0, static_cast<int>(productList.size()) - 1);
    return productList[static_cast<size_t>(index)];
}

// the radar widget only reports wheel / click / drag requests; the owner changes the view (as the radar screen does)
void MrmsViewer::changeZoom(double factor) {
    auto& state = radar->nexradState;
    if (factor < 1.0 && state.zoom <= 0.02) {
        return;
    }
    const double oldZoom = state.zoom;
    state.zoom = std::min(state.zoom * factor, 40.0);
    const double change = state.zoom / oldZoom;
    // zoom about the pointer (the middle of the map when it is outside): the spot under it stays where it is
    double u = 0.0;
    double v = 0.0;
    if (pointerInside) {
        u = pointer.x() * 1000.0 / std::max(1, radar->width()) - 500.0;
        v = pointer.y() * 1000.0 / std::max(1, radar->height()) - 250.0;
    }
    state.xPos = u - (u - state.xPos) * change;
    state.yPos = v - (v - state.yPos) * change;
    radar->resizePolygons();
    radar->nexradRenderTextObject.add();
    radar->update();
}

void MrmsViewer::changePosition(double dx, double dy) {
    auto& state = radar->nexradState;
    const double unitsPerPixel = 1000.0 / std::max(1, radar->width());   // the map is drawn on a 1000-unit window
    state.xPos += dx * unitsPerPixel;
    state.yPos += dy * unitsPerPixel;
    radar->nexradRenderTextObject.add();
    radar->update();
}

// the map is a square (the radar widget's projection assumes one): as large as fits under the controls
void MrmsViewer::fitRadar() {
    if (radar == nullptr) {
        return;
    }
    const int side = std::max(300, std::min(width() - 16, height() - rowTop.getView()->sizeHint().height() - 24));
    if (radar->width() != side) {
        radar->setFixedSize(side, side);
        radar->nexradState.originalWidth = side;
        radar->nexradState.originalHeight = side;
        radar->nexradRenderTextObject.add();
    }
}

void MrmsViewer::resizeEventCustom() {
    fitRadar();
}

void MrmsViewer::closeEventCustom() {
    closed = true;
    loopTimer.stop();
}

void MrmsViewer::loadScans() {
    const auto gen = ++generation;
    loopFrames.clear();
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
    loopFrames.clear();   // a loop belongs to the scan list it was made from
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
    currentColors = UtilityMrms::colorTable(product(), frame);
    setTitle("MRMS - " + product().label + " - " + timeText(frame.utc));
    textStatus.setText(product().label + "  " + timeText(frame.utc));
    radar->update();
}

// one scan drawn the way it is on screen (map, lines, legend), with the information bars of the other model viewers:
// product (and units) and when this file was saved on top; valid time and what it is at the bottom
QByteArray MrmsViewer::renderFrame(const UtilityMrms::Frame& frame, const QString& units, const QString& extra) {
    setFrame(frame);
    const QImage map = radar->grab().toImage();
    // a border above and below the map (the bars are drawn into it, so nothing - legend included - is covered)
    const int bar = UtilityGrib::headerBarHeight(map.width());
    QImage image{map.width(), map.height() + 2 * bar, QImage::Format_ARGB32_Premultiplied};
    image.fill(Qt::white);
    {
        QPainter painter{&image};
        painter.drawImage(0, bar, map);
    }
    UtilityGrib::MapHeader header;
    header.topLeft = "MRMS  " + QString::fromStdString(product().label) + (units.isEmpty() ? QString{} : " (" + units + ")");
    header.topRight = "Saved " + QDateTime::currentDateTimeUtc().toString("yyyy-MM-dd HH:mm") + "Z";
    const auto local = frame.utc.toLocalTime();
    header.bottomLeft = "Valid " + frame.utc.toUTC().toString("ddd yyyy-MM-dd HH:mm") + "Z  (" + local.toString("ddd h:mm AP") + " " +
        QTimeZone::systemTimeZone().abbreviation(local) + ")";
    header.bottomRight = extra;
    UtilityGrib::drawMapHeader(image, header);
    QByteArray bytes;
    QBuffer buffer{&bytes};
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    return bytes;
}

void MrmsViewer::onSave() {
    if (currentIndices.isEmpty()) {
        return;
    }
    const bool wasLooping = looping && !loopFrames.empty();
    loopTimer.stop();
    const auto shownFrame = current;
    const auto units = QString::fromStdString(UtilityMrms::unitsShown(product(), us()));
    const bool animated = loopFrames.size() >= 2;
    vector<QByteArray> frames;
    QByteArray still;
    QDateTime first = current.utc;
    QDateTime last;
    if (animated) {
        for (size_t i = 0; i < loopFrames.size(); i += 1) {
            frames.push_back(renderFrame(loopFrames[i], units, "NOAA MRMS   frame " + QString::number(i + 1) + " of " + QString::number(loopFrames.size())));
        }
        first = loopFrames.front().utc;
        last = loopFrames.back().utc;
    } else {
        still = renderFrame(current, units, "NOAA MRMS");
    }
    setFrame(shownFrame);   // back to what was on screen
    if (wasLooping) {
        loopTimer.start();
    }
    const auto name = UtilityAnimationExport::validName(first, last, "mrms_" + UtilityAnimationExport::slug(QString::fromStdString(product().id), 40));
    UtilityAnimationExport::saveWithDialog(this, frames, 350, still, name, QByteArray{}, false);
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

// The radar projection is linear in longitude and in Mercator y (degrees); its coefficients come from three map points.
MrmsViewer::Projection2 MrmsViewer::projection() const {
    const auto& pn = radar->nexradState.getPn();
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
    const double ay = (c[1] - a[1]) / (mercator(40.0) - mercator(30.0));
    return {ax, a[0] + 100.0 * ax, ay, a[1] - ay * mercator(30.0)};
}

// Draws the scan at the grid's own resolution: every screen pixel is turned back into a latitude / longitude through
// the radar widget's projection and looked up in the grid. That projection is Mercator, so longitude depends only on the
// pixel's column and latitude only on its row - two small tables, then a plain lookup per pixel.
void MrmsViewer::paintData(QPainter& painter) {
    if (currentIndices.isEmpty()) {
        return;
    }
    const auto [ax, bx, ay, by] = projection();
    const auto& grid = current.grid;
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
        const int column = static_cast<int>(std::floor((lon - grid.west) / grid.cell));
        columnOf[static_cast<size_t>(i)] = (column >= 0 && column < grid.columns) ? column : -1;
    }
    QImage image{pixelWidth, pixelHeight, QImage::Format_ARGB32_Premultiplied};
    const auto * cells = reinterpret_cast<const uchar *>(currentIndices.constData());
    for (int j = 0; j < pixelHeight; j += 1) {
        const double y = inverse.m22() * (viewport.top() + (j + 0.5) / ratio) + inverse.dy();
        const double lat = std::atan(std::sinh((y - by) / ay * std::numbers::pi / 180.0)) * 180.0 / std::numbers::pi;
        const int row = static_cast<int>(std::floor((grid.north - lat) / grid.cell));
        auto * out = reinterpret_cast<QRgb *>(image.scanLine(j));
        if (row < 0 || row >= grid.rows) {
            std::fill(out, out + pixelWidth, qRgba(0, 0, 0, 0));
            continue;
        }
        const auto * source = cells + static_cast<qsizetype>(row) * grid.columns;
        for (int i = 0; i < pixelWidth; i += 1) {
            const int column = columnOf[static_cast<size_t>(i)];
            out[i] = column < 0 ? qRgba(0, 0, 0, 0) : currentColors[source[column]];
        }
    }
    // the image is one pixel per device pixel; drawn through the current transform onto exactly the area it was sampled for
    painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
    painter.drawImage(inverse.mapRect(QRectF{viewport}), image, QRectF{image.rect()});
}

bool MrmsViewer::eventFilter(QObject * object, QEvent * event) {
    if (object == radar && (event->type() == QEvent::MouseMove || event->type() == QEvent::MouseButtonPress)) {
        pointer = static_cast<QMouseEvent *>(event)->position();
        pointerInside = true;
        if (event->type() == QEvent::MouseMove) {
            showHover(pointer);
        }
    } else if (object == radar && event->type() == QEvent::Wheel) {
        pointer = static_cast<QWheelEvent *>(event)->position();
        pointerInside = true;
    } else if (object == radar && event->type() == QEvent::Leave) {
        pointerInside = false;
        hoverShown = false;
        if (hoverLabel != nullptr) {
            hoverLabel->hide();
        }
        radar->update();
    }
    return false;
}

// the value under the pointer: widget pixel -> window units -> projected coordinates -> latitude / longitude -> grid cell
void MrmsViewer::showHover(const QPointF& widgetPos) {
    if (currentIndices.isEmpty() || looping) {
        return;
    }
    const auto [ax, bx, ay, by] = projection();
    const auto& grid = current.grid;
    const auto& state = radar->nexradState;
    const double u = widgetPos.x() * 1000.0 / radar->width() - 500.0;
    const double v = widgetPos.y() * 1000.0 / radar->height() - 250.0;
    const double x = (u - state.xPos) / state.zoom;
    const double y = (v - state.yPos) / state.zoom;
    const double lon = (x - bx) / ax;
    const double lat = std::atan(std::sinh((y - by) / ay * std::numbers::pi / 180.0)) * 180.0 / std::numbers::pi;
    const int column = static_cast<int>(std::floor((lon - grid.west) / grid.cell));
    const int row = static_cast<int>(std::floor((grid.north - lat) / grid.cell));
    // the popup: coordinates, then the value (the same two lines as the other map viewers)
    QString text = QString::number(std::abs(lat), 'f', 2) + QChar{0x00B0} + (lat >= 0 ? " N" : " S") + "   " +
        QString::number(std::abs(lon), 'f', 2) + QChar{0x00B0} + (lon < 0 ? " W" : " E") + "\n";
    if (column >= 0 && column < grid.columns && row >= 0 && row < grid.rows) {
        const int index = static_cast<uchar>(currentIndices[static_cast<qsizetype>(row) * grid.columns + column]);
        text += index == 0 ? QString{"no data"} : QString::number(UtilityMrms::shown(product(), current.valueAt(index), us()), 'g', 4) + " " +
            QString::fromStdString(UtilityMrms::unitsShown(product(), us()));
    } else {
        text += "outside the MRMS grid";
    }
    hoverShown = true;
    if (hoverLabel != nullptr) {
        hoverLabel->setText(text);
        hoverLabel->adjustSize();
        hoverLabel->move(12, 12);
        hoverLabel->show();
        hoverLabel->raise();
    }
    radar->update();
}

void MrmsViewer::paintLegend(QPainter& painter) {
    if (currentIndices.isEmpty()) {
        return;
    }
    if (hoverShown && !looping) {
        // the crosshair at the pointer (window units: the map is drawn on a 1000 x 1000 window)
        const double u = pointer.x() * 1000.0 / radar->width() - 500.0;
        const double v = pointer.y() * 1000.0 / radar->height() - 250.0;
        const double arm = 16.0 * 1000.0 / radar->width();
        const double gap = 4.0 * 1000.0 / radar->width();
        for (const auto& pen : {QPen{QColor{0, 0, 0, 200}, 3.5}, QPen{QColor{255, 255, 255}, 1.5}}) {
            painter.setPen(pen);
            painter.drawLine(QPointF{u - arm, v}, QPointF{u - gap, v});
            painter.drawLine(QPointF{u + gap, v}, QPointF{u + arm, v});
            painter.drawLine(QPointF{u, v - arm}, QPointF{u, v - gap});
            painter.drawLine(QPointF{u, v + gap}, QPointF{u, v + arm});
        }
    }
    const auto& stops = product().stops;
    // an automatic scale spans the scan's own range, so its stop values come from the scan
    const double lo = current.validMin;
    const double span = current.step * 254.0;
    const double boxWidth = 38.0;
    const double x0 = -490.0;
    const double y0 = 715.0;
    QFont font{painter.font()};
    font.setPointSizeF(9.0);
    painter.setFont(font);
    for (size_t i = 0; i < stops.size(); i += 1) {
        const double value = product().autoRange ? lo + stops[i].value * span : stops[i].value;
        const QRectF box{x0 + static_cast<double>(i) * boxWidth, y0, boxWidth, 14.0};
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor{stops[i].r, stops[i].g, stops[i].b});
        painter.drawRect(box);
        painter.setPen(Qt::white);
        painter.drawText(QRectF{box.left(), y0 + 14.0, boxWidth, 16.0}, Qt::AlignCenter,
                         QString::number(UtilityMrms::shown(product(), value, us()), 'g', 3));
    }
    painter.drawText(QPointF{x0 + static_cast<double>(stops.size()) * boxWidth + 8.0, y0 + 11.0},
                     QString::fromStdString(UtilityMrms::unitsShown(product(), us())));
}
