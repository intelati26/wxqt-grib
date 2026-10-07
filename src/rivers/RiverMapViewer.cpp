// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "rivers/RiverMapViewer.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <numbers>
#include <QFont>
#include <QMouseEvent>
#include <QPainter>
#include "objects/FutureVoid.h"
#include "radar/Projection.h"
#include "buoys/BuoyViewer.h"
#include "rivers/RiverGaugeViewer.h"
#include "util/Utility.h"
#include "settings/Location.h"
#include "util/UtilityUI.h"

namespace {
    // the flood-category colours (the usual NWS ones: purple, red, orange, yellow, green)
    struct Look {
        const char * status;
        const char * label;
        QColor color;
        int rank;       // drawn in this order, the worst last (on top)
        double radius;  // pixels
    };
    const std::vector<Look>& looks() {
        static const std::vector<Look> table{
            {"out_of_service", "Out of service", QColor{120, 120, 120}, 0, 2.5},
            {"not_defined", "No flood stages defined", QColor{150, 150, 160}, 1, 2.5},
            {"obs_not_current", "Observation not current", QColor{190, 190, 190}, 2, 2.5},
            {"low_threshold", "Below the low-water level", QColor{150, 100, 50}, 3, 3.5},
            {"no_flooding", "No flooding", QColor{40, 190, 70}, 4, 3.0},
            {"action", "Action stage", QColor{240, 210, 0}, 5, 4.5},
            {"minor", "Minor flooding", QColor{255, 140, 0}, 6, 5.5},
            {"moderate", "Moderate flooding", QColor{230, 40, 40}, 7, 6.5},
            {"major", "Major flooding", QColor{170, 40, 240}, 8, 7.5},
        };
        return table;
    }
    const Look & lookOf(const string& status) {
        for (const auto& look : looks()) {
            if (status == look.status) {
                return look;
            }
        }
        return looks()[1];
    }
    double mercatorOf(double lat) {
        return 180.0 / std::numbers::pi * std::log(std::tan(std::numbers::pi / 4.0 + lat * std::numbers::pi / 360.0));
    }
}

RiverMapViewer::RiverMapViewer(Window * parent)
    : Window{parent}
    , comboFilter{this, {"All gauges", "Action stage or higher", "Flooding (minor or higher)", "Not reporting (no current reading / out of service)"}}
    , buttonRefresh{this, None, "Refresh"}
    , comboBuoyColor{this, {"Buoys by wind", "Buoys by water temperature"}}
    , textStatus{this, "Loading the river gauges..."}
{
    setAttribute(Qt::WA_DeleteOnClose);
    setTitle("Rivers - NWS gauges");
    textStatus.setWordWrap(false);
    const auto dimens = UtilityUI::getScreenBounds();
    const auto side = std::max(300, std::min(dimens[0] - 20, dimens[1] - 160));
    radar = new MapWidget{
        this, 0, 1, true, Location::radarSite(), side, side,
        [this] (double z, [[maybe_unused]] int pane) { changeZoom(z); },
        [this] (double x, double y, [[maybe_unused]] int pane) { changePosition(x, y); }};
    radar->setFixedSize(side, side);
    radar->mapState.setRadar(Location::radarSite());
    radar->mapState.reset();
    radar->mapDraw.initGeom();
    showConus();
    radar->setMouseTracking(true);
    radar->installEventFilter(this);
    // a click on a gauge opens its page, and does not also zoom the map out (the radar widget's plain-click action)
    radar->clickHandler = [this] (const QPointF& at) {
        // the nearer of a gauge and a buoy (a buoy only while its layer is on)
        const auto * g = gaugeAt(at);
        const auto * b = buoyCheck != nullptr && buoyCheck->isChecked() ? buoyAt(at) : nullptr;
        if (g != nullptr && b != nullptr) {
            const auto p = projection();
            const auto gp = widgetOf(*g, p);
            const auto bp = pixelsOf(b->obs.lon, b->mercator, p);
            if (std::hypot(bp.x() - at.x(), bp.y() - at.y()) < std::hypot(gp.x() - at.x(), gp.y() - at.y())) {
                g = nullptr;
            } else {
                b = nullptr;
            }
        }
        if (g != nullptr) {
            new RiverGaugeViewer{this, g->lid};
            return true;
        }
        if (b != nullptr) {
            new BuoyViewer{this, *b};
            return true;
        }
        return false;
    };
    hoverLabel = new QLabel{radar};
    hoverLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    hoverLabel->setStyleSheet("QLabel { background-color: rgba(15, 15, 15, 215); color: #f2f2f2; padding: 4px 8px; border-radius: 3px; }");
    hoverLabel->hide();
    radar->dataLayer = [] (QPainter& painter) { painter.fillRect(painter.viewport(), QColor{18, 24, 34}); };   // no radar: a dark map
    radar->topLayer = [this] (QPainter& painter) { paintBuoys(painter); paintGauges(painter); paintLegend(painter); };
    comboFilter.connect([this] { summarize(); radar->update(); });
    buttonRefresh.connect([this] { loadGauges(); if (buoyCheck->isChecked()) { loadBuoys(); } });
    buoyCheck = new QCheckBox{"Buoys (NDBC)", this};
    buoyCheck->setChecked(Utility::readPref("RIVERS_BUOYS", "false") == "true");
    buoyCheck->setToolTip("NOAA's National Data Buoy Center buoys and coastal stations: the latest wind, waves, pressure and temperatures; click one for its history");
    QObject::connect(buoyCheck, &QCheckBox::toggled, [this] (bool on) {
        Utility::writePref("RIVERS_BUOYS", on ? "true" : "false");
        if (on && !buoys) {
            loadBuoys();
        }
        radar->update();
    });
    comboBuoyColor.connect([this] { radar->update(); });
    rowTop.addWidget(comboFilter);
    rowTop.addWidget(buttonRefresh);
    rowTop.addWidgetReal(buoyCheck);
    rowTop.addWidget(comboBuoyColor);
    rowTop.addStretch();
    box.addLayout(rowTop);
    box.addWidget(textStatus);
    box.addWidgetReal(radar, 0, Qt::AlignTop | Qt::AlignLeft);
    box.addStretch();
    box.getAndShow(this);
    loadGauges();
    if (buoyCheck->isChecked()) {
        loadBuoys();
    }
}

void RiverMapViewer::loadGauges() {
    const auto gen = ++generation;
    textStatus.setText(string{"Loading the river gauges..."});
    auto fresh = std::make_shared<vector<UtilityRivers::Gauge>>();
    auto error = std::make_shared<string>();
    auto ok = std::make_shared<bool>(false);
    new FutureVoid{this,
        [fresh, error, ok] { *ok = UtilityRivers::loadGauges(*fresh, *error); },
        [this, gen, fresh, error, ok] {
            if (closed || gen != generation) {
                return;
            }
            if (!*ok) {
                textStatus.setText(*error);
                return;
            }
            // the worst on top: draw the quiet ones first
            std::stable_sort(fresh->begin(), fresh->end(), [] (const auto& a, const auto& b) { return lookOf(a.status).rank < lookOf(b.status).rank; });
            gauges = fresh;
            summarize();
            radar->update();
        }};
}

void RiverMapViewer::summarize() {
    if (!gauges) {
        return;
    }
    std::map<string, int> counts;
    int shownCount = 0;
    for (const auto& g : *gauges) {
        counts[g.status] += 1;
        shownCount += shown(g) ? 1 : 0;
    }
    string text = std::to_string(gauges->size()) + " gauges";
    if (shownCount != static_cast<int>(gauges->size())) {
        text += ", " + std::to_string(shownCount) + " shown";
    }
    text += "   -   ";
    for (const char * status : {"major", "moderate", "minor", "action"}) {
        text += std::string{lookOf(status).label} + ": " + std::to_string(counts[status]) + "   ";
    }
    text += "   (click a gauge for its page)";
    textStatus.setText(text);
}

bool RiverMapViewer::shown(const UtilityRivers::Gauge& g) const {
    const auto& s = g.status;
    switch (comboFilter.getIndex()) {
        case 1: return s == "action" || s == "minor" || s == "moderate" || s == "major";
        case 2: return s == "minor" || s == "moderate" || s == "major";
        case 3: return s == "obs_not_current" || s == "out_of_service";
        default: return true;
    }
}

// the radar projection as x = ax * lon + bx, y = ay * mercator(lat) + by (two points fix it)
RiverMapViewer::Projection2 RiverMapViewer::projection() const {
    const auto& pn = radar->mapState.getPn();
    const auto project = [&pn] (double lat, double lon) {
        return Projection::computeMercatorNumbersFromLatLon(LatLon{lat, lon}.reverseLon(), pn);
    };
    const auto a = project(30.0, -100.0);
    const auto b = project(30.0, -99.0);
    const auto c = project(40.0, -100.0);
    const double ax = b[0] - a[0];
    const double ay = (c[1] - a[1]) / (mercatorOf(40.0) - mercatorOf(30.0));
    return {ax, a[0] + 100.0 * ax, ay, a[1] - ay * mercatorOf(30.0)};
}

// where a gauge is in the widget, in pixels
QPointF RiverMapViewer::widgetOf(const UtilityRivers::Gauge& g, const Projection2& p) const {
    return pixelsOf(g.lon, g.mercator, p);
}

QPointF RiverMapViewer::pixelsOf(double lon, double mercatorLat, const Projection2& p) const {
    const auto& state = radar->mapState;
    const double u = (p.ax * lon + p.bx) * state.zoom + state.xPos;
    const double v = (p.ay * mercatorLat + p.by) * state.zoom + state.yPos;
    return QPointF{(u + 500.0) * radar->width() / 1000.0, (v + 250.0) * radar->height() / 1000.0};
}

void RiverMapViewer::loadBuoys() {
    auto fresh = std::make_shared<std::vector<BuoyData::Marker>>();
    auto error = std::make_shared<std::string>();
    auto ok = std::make_shared<bool>(false);
    new FutureVoid{this,
        [fresh, error, ok] { *ok = BuoyData::loadLatest(*fresh, *error); },
        [this, fresh, error, ok] {
            if (closed) {
                return;
            }
            if (!*ok) {
                textStatus.setText(*error);
                return;
            }
            buoys = fresh;
            radar->update();
        }};
}

// wind (kt) or water temperature (F) in the buoy's colour; grey when it does not report the quantity
QColor RiverMapViewer::buoyColor(const BuoyData::Marker& m) const {
    if (comboBuoyColor.getIndex() == 1) {
        if (!UtilityBuoys::has(m.obs.waterTemperature)) {
            return QColor{120, 120, 130};
        }
        const double f = UtilityBuoys::fahrenheit(m.obs.waterTemperature);
        return f < 40 ? QColor{150, 90, 220} : f < 50 ? QColor{60, 100, 230} : f < 60 ? QColor{40, 190, 220} : f < 70 ? QColor{60, 190, 90} : f < 80 ? QColor{240, 200, 40} : QColor{235, 70, 50};
    }
    if (!UtilityBuoys::has(m.obs.windSpeed)) {
        return QColor{120, 120, 130};
    }
    const double kt = UtilityBuoys::knots(m.obs.windSpeed);
    return kt < 10 ? QColor{130, 170, 205} : kt < 20 ? QColor{60, 190, 90} : kt < 34 ? QColor{240, 210, 40} : kt < 48 ? QColor{255, 140, 0} : kt < 64 ? QColor{230, 40, 40} : QColor{170, 40, 240};
}

void RiverMapViewer::paintBuoys(QPainter& painter) {
    if (buoyCheck == nullptr || !buoyCheck->isChecked() || !buoys) {
        return;
    }
    const auto p = projection();
    const auto& state = radar->mapState;
    const double perPixel = 1000.0 / std::max(1, radar->width());
    const double half = (4.2 + std::min(2.5, std::log2(std::max(1.0, state.zoom * 7.0)) * 0.4)) * perPixel;
    painter.setRenderHint(QPainter::Antialiasing, true);
    for (const auto& m : *buoys) {
        const double u = (p.ax * m.obs.lon + p.bx) * state.zoom + state.xPos;
        const double v = (p.ay * m.mercator + p.by) * state.zoom + state.yPos;
        if (u < -520.0 || u > 520.0 || v < -270.0 || v > 770.0) {
            continue;
        }
        painter.setPen(QPen{QColor{255, 255, 255, 200}, 1.0 * perPixel});
        painter.setBrush(buoyColor(m));
        painter.drawRect(QRectF{u - half, v - half, 2 * half, 2 * half});   // a square: a buoy, as against a gauge's circle
    }
}

const BuoyData::Marker * RiverMapViewer::buoyAt(const QPointF& widgetPos) const {
    if (!buoys) {
        return nullptr;
    }
    const auto p = projection();
    const BuoyData::Marker * best = nullptr;
    double bestDistance = 11.0;   // pixels
    for (const auto& m : *buoys) {
        const auto at = pixelsOf(m.obs.lon, m.mercator, p);
        if (std::abs(at.x() - widgetPos.x()) > bestDistance || std::abs(at.y() - widgetPos.y()) > bestDistance) {
            continue;
        }
        const double distance = std::hypot(at.x() - widgetPos.x(), at.y() - widgetPos.y());
        if (distance <= bestDistance) {
            bestDistance = distance;
            best = &m;
        }
    }
    return best;
}

// the whole lower 48 in the middle of the square map
void RiverMapViewer::showConus() {
    auto& state = radar->mapState;
    const auto [ax, bx, ay, by] = projection();
    const double centerLon = -96.0;
    const double centerLat = 37.5;
    const double lonSpan = 62.0;
    state.zoom = 1000.0 / (std::abs(ax) * lonSpan);
    state.xPos = -(ax * centerLon + bx) * state.zoom;
    state.yPos = 250.0 - (ay * mercatorOf(centerLat) + by) * state.zoom;
}

void RiverMapViewer::fitRadar() {
    if (radar == nullptr) {
        return;
    }
    const int above = rowTop.getView()->sizeHint().height() + textStatus.getView()->sizeHint().height();
    const int side = std::max(300, std::min(width() - 16, height() - above - 32));
    if (radar->width() != side) {
        radar->setFixedSize(side, side);
        radar->mapState.originalWidth = side;
        radar->mapState.originalHeight = side;
        radar->mapTextObject.add();
    }
}

void RiverMapViewer::resizeEventCustom() {
    fitRadar();
}

void RiverMapViewer::changeZoom(double factor) {
    auto& state = radar->mapState;
    if (factor < 1.0 && state.zoom <= 0.02) {
        return;
    }
    const double oldZoom = state.zoom;
    state.zoom = std::min(state.zoom * factor, 60.0);
    const double change = state.zoom / oldZoom;
    double u = 0.0;
    double v = 0.0;
    if (pointerInside) {
        u = pointer.x() * 1000.0 / std::max(1, radar->width()) - 500.0;
        v = pointer.y() * 1000.0 / std::max(1, radar->height()) - 250.0;
    }
    state.xPos = u - (u - state.xPos) * change;
    state.yPos = v - (v - state.yPos) * change;
    radar->mapTextObject.add();
    radar->update();
}

void RiverMapViewer::changePosition(double dx, double dy) {
    auto& state = radar->mapState;
    const double unitsPerPixel = 1000.0 / std::max(1, radar->width());
    state.xPos += dx * unitsPerPixel;
    state.yPos += dy * unitsPerPixel;
    radar->mapTextObject.add();
    radar->update();
}

// painted over the map with the pan / zoom transform removed: window units (1000 wide, from -500; from -250 down)
void RiverMapViewer::paintGauges(QPainter& painter) {
    if (!gauges) {
        return;
    }
    const auto p = projection();
    const auto& state = radar->mapState;
    const double perPixel = 1000.0 / std::max(1, radar->width());
    painter.setRenderHint(QPainter::Antialiasing, true);
    for (const auto& g : *gauges) {
        if (!shown(g)) {
            continue;
        }
        const double u = (p.ax * g.lon + p.bx) * state.zoom + state.xPos;
        const double v = (p.ay * g.mercator + p.by) * state.zoom + state.yPos;
        if (u < -520.0 || u > 520.0 || v < -270.0 || v > 770.0) {
            continue;
        }
        const auto& look = lookOf(g.status);
        // a little larger as the map is zoomed in, so a gauge is easy to hit
        const double r = (look.radius + std::min(3.0, std::log2(std::max(1.0, state.zoom * 7.0)) * 0.5)) * perPixel;
        painter.setPen(QPen{QColor{0, 0, 0, 170}, 0.9 * perPixel});
        painter.setBrush(look.color);
        painter.drawEllipse(QPointF{u, v}, r, r);
    }
}

void RiverMapViewer::paintLegend(QPainter& painter) {
    const double perPixel = 1000.0 / std::max(1, radar->width());
    QFont font{painter.font()};
    font.setPointSizeF(9.0);
    painter.setFont(font);
    double x = -490.0;
    const double y = 735.0;
    // short names here (the longer ones are in the pointer popup)
    static const std::pair<const char *, const char *> entries[] = {
        {"major", "Major"}, {"moderate", "Moderate"}, {"minor", "Minor"}, {"action", "Action"}, {"no_flooding", "No flooding"},
        {"low_threshold", "Low water"}, {"obs_not_current", "Not current"}, {"not_defined", "No stages set"}};
    for (const auto& [status, name] : entries) {
        const auto& look = lookOf(status);
        painter.setPen(QPen{QColor{0, 0, 0, 170}, 0.9 * perPixel});
        painter.setBrush(look.color);
        painter.drawEllipse(QPointF{x + 6.0, y}, 6.0, 6.0);
        painter.setPen(QColor{235, 235, 235});
        const QString label = name;
        painter.drawText(QPointF{x + 16.0, y + 4.0}, label);
        x += 30.0 + QFontMetricsF{font}.horizontalAdvance(label);
    }
    if (buoyCheck != nullptr && buoyCheck->isChecked() && buoys) {
        // the buoys' colours: squares, one row above the gauges'
        const bool temperature = comboBuoyColor.getIndex() == 1;
        static const std::pair<const char *, QColor> wind[] = {{"<10 kt", QColor{130, 170, 205}}, {"10-20", QColor{60, 190, 90}}, {"20-34", QColor{240, 210, 40}}, {"34-48", QColor{255, 140, 0}},
                                                               {"48-64", QColor{230, 40, 40}}, {"64+ kt", QColor{170, 40, 240}}, {"no report", QColor{120, 120, 130}}};
        static const std::pair<const char *, QColor> water[] = {{"<40 F", QColor{150, 90, 220}}, {"40-50", QColor{60, 100, 230}}, {"50-60", QColor{40, 190, 220}}, {"60-70", QColor{60, 190, 90}},
                                                                {"70-80", QColor{240, 200, 40}}, {"80+ F", QColor{235, 70, 50}}, {"no report", QColor{120, 120, 130}}};
        double bx = -490.0;
        const double by = 712.0;
        painter.setPen(QColor{235, 235, 235});
        painter.drawText(QPointF{bx, by + 4.0}, temperature ? "Buoys, water:" : "Buoys, wind:");
        bx += 92.0;
        for (const auto& [name, color] : (temperature ? std::vector<std::pair<const char *, QColor>>(std::begin(water), std::end(water))
                                                     : std::vector<std::pair<const char *, QColor>>(std::begin(wind), std::end(wind)))) {
            painter.setPen(QPen{QColor{255, 255, 255, 200}, 0.9 * perPixel});
            painter.setBrush(color);
            painter.drawRect(QRectF{bx, by - 5.0, 10.0, 10.0});
            painter.setPen(QColor{235, 235, 235});
            const QString label = name;
            painter.drawText(QPointF{bx + 15.0, by + 4.0}, label);
            bx += 28.0 + QFontMetricsF{font}.horizontalAdvance(label);
        }
    }
}

const UtilityRivers::Gauge * RiverMapViewer::gaugeAt(const QPointF& widgetPos) const {
    if (!gauges) {
        return nullptr;
    }
    const auto p = projection();
    const UtilityRivers::Gauge * best = nullptr;
    double bestDistance = 11.0;   // pixels
    for (const auto& g : *gauges) {
        if (!shown(g)) {
            continue;
        }
        const auto at = widgetOf(g, p);
        if (std::abs(at.x() - widgetPos.x()) > bestDistance || std::abs(at.y() - widgetPos.y()) > bestDistance) {
            continue;
        }
        const double distance = std::hypot(at.x() - widgetPos.x(), at.y() - widgetPos.y());
        // the worse gauge wins a tie (they are sorted worst last)
        if (distance <= bestDistance) {
            bestDistance = distance;
            best = &g;
        }
    }
    return best;
}

bool RiverMapViewer::eventFilter(QObject * object, QEvent * event) {
    if (object != radar) {
        return false;
    }
    switch (event->type()) {
        case QEvent::MouseMove:
            pointer = static_cast<QMouseEvent *>(event)->position();
            pointerInside = true;
            showHover(pointer);
            break;
        case QEvent::Wheel:
            pointer = static_cast<QWheelEvent *>(event)->position();
            pointerInside = true;
            break;
        case QEvent::Leave:
            pointerInside = false;
            hoverLabel->hide();
            break;
        default:
            break;
    }
    return false;
}

void RiverMapViewer::showHover(const QPointF& widgetPos) {
    auto * g = gaugeAt(widgetPos);
    const auto * b = buoyCheck != nullptr && buoyCheck->isChecked() ? buoyAt(widgetPos) : nullptr;
    if (b != nullptr && g != nullptr) {
        const auto p = projection();
        const auto gp = widgetOf(*g, p);
        const auto bp = pixelsOf(b->obs.lon, b->mercator, p);
        if (std::hypot(bp.x() - widgetPos.x(), bp.y() - widgetPos.y()) < std::hypot(gp.x() - widgetPos.x(), gp.y() - widgetPos.y())) {
            g = nullptr;
        } else {
            b = nullptr;
        }
    }
    if (b != nullptr) {
        radar->setCursor(Qt::PointingHandCursor);
        const auto& s = b->station;
        hoverLabel->setText(QString::fromStdString(b->obs.id + (s.name.empty() ? "" : "  " + s.name)) + "\n" + BuoyViewer::summary(*b).replace(";  ", "\n") + "\n(click for its history)");
        hoverLabel->adjustSize();
        hoverLabel->move(12, 12);
        hoverLabel->show();
        hoverLabel->raise();
        return;
    }
    if (g == nullptr) {
        hoverLabel->hide();
        radar->setCursor(Qt::ArrowCursor);
        return;
    }
    radar->setCursor(Qt::PointingHandCursor);
    QString text = QString::fromStdString(g->lid + "  " + (g->name.empty() ? g->waterbody : g->name)) + "\n" +
        QString::fromStdString(g->waterbody + ", " + g->state) + "\n" + QString::fromStdString(lookOf(g->status).label);
    if (UtilityRivers::has(g->observed)) {
        text += "   " + QString::number(g->observed, 'f', 2) + " " + QString::fromStdString(g->units);
    }
    if (!g->obsTime.empty()) {
        text += "\n" + QString::fromStdString(g->obsTime) + " UTC";
    }
    hoverLabel->setText(text);
    hoverLabel->adjustSize();
    hoverLabel->move(12, 12);
    hoverLabel->show();
    hoverLabel->raise();
}
