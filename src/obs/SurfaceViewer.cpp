// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "obs/SurfaceViewer.h"
#include <algorithm>
#include <cmath>
#include <ctime>
#include <numbers>
#include <unordered_set>
#include <QFont>
#include <QLocale>
#include <QMouseEvent>
#include <QPainter>
#include "hurricane/UtilityHdob.h"
#include "misc/TextViewerStatic.h"
#include "objects/FutureVoid.h"
#include "obs/SurfaceData.h"
#include "radar/Projection.h"
#include "settings/Location.h"
#include "settings/UIPreferences.h"
#include "ui/WindBarb.h"
#include "util/Utility.h"
#include "util/UtilityUI.h"

namespace {
    double mercatorOf(double lat) {
        return 180.0 / std::numbers::pi * std::log(std::tan(std::numbers::pi / 4.0 + lat * std::numbers::pi / 360.0));
    }

    // how far the reports of a network can be trusted, for deciding which station of a crowded patch to draw: airports, then the networks run by
    // agencies, the aggregator MesoWest, the citizen stations last
    int rankOf(const SurfaceStation& s) {
        if (s.airport) {
            return 0;
        }
        if (s.network == "APRSWXNET") {
            return 3;
        }
        if (s.network == "MesoWest" || s.network == "NonFedAWOS") {
            return 2;
        }
        return 1;
    }

    QString number(double v, int digits = 0) {
        return QLocale{QLocale::English}.toString(v, 'f', digits);
    }

    QString ageText(long seconds) {
        const long age = static_cast<long>(std::time(nullptr)) - seconds;
        if (age < 90) {
            return "just now";
        }
        if (age < 5400) {
            return QString::number(age / 60) + " min ago";
        }
        return number(static_cast<double>(age) / 3600.0, 1) + " h ago";
    }

    QString windText(const SurfaceStation& s) {
        if (!SurfaceStation::has(s.windSpeed)) {
            return "no wind reported";
        }
        if (s.windSpeed < 0.5) {
            return "calm";
        }
        QString text = SurfaceStation::has(s.windDirection) ? QString::number(static_cast<int>(std::lround(s.windDirection)), 10).rightJustified(3, '0') + " deg at " : QString{"variable at "};
        text += number(s.windSpeed) + " kt";
        if (SurfaceStation::has(s.windGust) && s.windGust > s.windSpeed + 0.5) {
            text += ", gusts " + number(s.windGust) + " kt";
        }
        return text;
    }
}

SurfaceViewer::SurfaceViewer(Window * parent)
    : Window{parent}
    , buttonRefresh{this, None, "Refresh"}
    , comboColor{this, {"Dots: flight category / network", "Dots: temperature"}}
    , comboUnits{this, {"Fahrenheit", "Celsius"}}
    , textStatus{this, "Loading the airport reports..."}
{
    setAttribute(Qt::WA_DeleteOnClose);
    setTitle("Surface observations - airports and mesonets");
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
    // a click on a station opens its card, and does not also zoom the map out
    radar->clickHandler = [this] (const QPointF& at) {
        const int index = pickAt(at);
        if (index < 0) {
            return false;
        }
        const auto& s = all[static_cast<size_t>(index)];
        new TextViewerStatic{this, details(s).toStdString(), s.id + (s.name.empty() ? "" : "  " + s.name), 640, 520};
        return true;
    };
    hoverLabel = new QLabel{radar};
    hoverLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    hoverLabel->setStyleSheet("QLabel { background-color: rgba(15, 15, 15, 215); color: #f2f2f2; padding: 4px 8px; border-radius: 3px; }");
    hoverLabel->hide();
    radar->dataLayer = [] (QPainter& painter) { painter.fillRect(painter.viewport(), QColor{18, 24, 34}); };
    radar->topLayer = [this] (QPainter& painter) { paintStations(painter); paintLegend(painter); };

    const auto addCheck = [this] (const char * label, const char * pref, bool on, const char * tip) {
        auto * check = new QCheckBox{label, this};
        check->setChecked(Utility::readPref(pref, on ? "true" : "false") == "true");
        check->setToolTip(tip);
        const string key{pref};
        QObject::connect(check, &QCheckBox::toggled, [this, key] (bool value) {
            Utility::writePref(key, value ? "true" : "false");
            radar->update();
        });
        return check;
    };
    airportCheck = addCheck("Airports (METAR)", "SURFACE_AIRPORTS", true, "The newest METAR of each airport weather station (ASOS, AWOS and the others), from the Aviation Weather Center");
    mesoCheck = addCheck("Mesonets and other networks (MADIS)", "SURFACE_MESONET", false,
        "State mesonets, RAWS, road weather, hydrological and citizen weather stations from NOAA MADIS' public mesonet files (about 30 MB each, two read)");
    barbCheck = addCheck("Wind barbs", "SURFACE_BARBS", true, "Knots: a pennant is 50, a long barb 10, a short barb 5");
    valueCheck = addCheck("Temperature and dew point", "SURFACE_VALUES", true, "Red the temperature, green the dew point");
    QObject::connect(mesoCheck, &QCheckBox::toggled, [this] (bool on) {
        if (on && mesonet.empty() && !mesonetLoading) {
            loadMesonet();
        }
        rebuild();
    });
    comboColor.setIndex(static_cast<size_t>(std::clamp(Utility::readPrefInt("SURFACE_COLOR", 0), 0, 1)));
    comboColor.connect([this] { Utility::writePrefInt("SURFACE_COLOR", comboColor.getIndex()); radar->update(); });
    comboUnits.setIndex(static_cast<size_t>(Utility::readPrefInt("SURFACE_UNITS", UIPreferences::unitsF ? 0 : 1)));
    comboUnits.connect([this] { Utility::writePrefInt("SURFACE_UNITS", comboUnits.getIndex()); radar->update(); });
    buttonRefresh.connect([this] { loadMetars(); });
    rowTop.addWidget(buttonRefresh);
    rowTop.addWidgetReal(airportCheck);
    rowTop.addWidgetReal(mesoCheck);
    rowTop.addWidgetReal(barbCheck);
    rowTop.addWidgetReal(valueCheck);
    rowTop.addWidget(comboColor);
    rowTop.addWidget(comboUnits);
    rowTop.addStretch();
    box.addLayout(rowTop);
    box.addWidget(textStatus);
    box.addWidgetReal(radar, 0, Qt::AlignTop | Qt::AlignLeft);
    box.addStretch();
    box.getAndShow(this);
    timer.setInterval(120000);   // the METAR file is renewed every minute; the mesonet files are read again every 30 minutes
    QObject::connect(&timer, &QTimer::timeout, [this] {
        ticks++;
        loadMetars();
        if (ticks % 15 == 0 && mesoCheck->isChecked()) {
            mesonet.clear();
            loadMesonet();
        }
    });
    timer.start();
    loadMetars();
}

void SurfaceViewer::loadMetars() {
    const int mine = ++generation;
    auto fresh = std::make_shared<vector<SurfaceStation>>();
    auto error = std::make_shared<string>();
    auto ok = std::make_shared<bool>(false);
    new FutureVoid{this,
        [fresh, error, ok] { *ok = SurfaceData::loadMetars(*fresh, *error); },
        [this, mine, fresh, error, ok] {
            if (closed || mine != generation) {
                return;
            }
            if (!*ok) {
                textStatus.setText(*error);
                return;
            }
            metars = std::move(*fresh);
            rebuild();
            if (mesoCheck->isChecked() && mesonet.empty() && !mesonetLoading) {
                loadMesonet();
            }
        }};
}

void SurfaceViewer::loadMesonet() {
    mesonetLoading = true;
    mesonetError.clear();
    textStatus.setText(string{"Reading the MADIS mesonet files (about 60 MB)..."});
    auto fresh = std::make_shared<vector<SurfaceStation>>();
    auto error = std::make_shared<string>();
    auto ok = std::make_shared<bool>(false);
    auto airports = std::make_shared<vector<SurfaceStation>>(metars);
    new FutureVoid{this,
        [fresh, error, ok, airports] { *ok = SurfaceData::loadMesonet(*fresh, *error, *airports); },
        [this, fresh, error, ok] {
            mesonetLoading = false;
            if (closed) {
                return;
            }
            if (*ok) {
                mesonet = std::move(*fresh);
            } else {
                mesonetError = *error;
            }
            rebuild();
        }};
}

// the airports first (so they win a crowded patch), then the mesonets by rank, those with a temperature and the newer ones first
void SurfaceViewer::rebuild() {
    all.clear();
    all.insert(all.end(), metars.begin(), metars.end());
    if (mesoCheck->isChecked()) {
        all.insert(all.end(), mesonet.begin(), mesonet.end());
    }
    std::stable_sort(all.begin(), all.end(), [] (const SurfaceStation& a, const SurfaceStation& b) {
        if (rankOf(a) != rankOf(b)) {
            return rankOf(a) < rankOf(b);
        }
        const bool ta = SurfaceStation::has(a.temperature);
        const bool tb = SurfaceStation::has(b.temperature);
        if (ta != tb) {
            return ta;
        }
        return a.seconds > b.seconds;
    });
    string text = std::to_string(metars.size()) + " airport reports";
    if (mesoCheck->isChecked()) {
        text += mesonetLoading ? ",  reading the mesonet files..." : mesonetError.empty() ? ",  " + std::to_string(mesonet.size()) + " mesonet and other stations (MADIS)" : ",  " + mesonetError;
    }
    text += "   -   hover a station for its report, click for everything it reported";
    textStatus.setText(text);
    radar->update();
}

QColor SurfaceViewer::dotColor(const SurfaceStation& s) const {
    if (comboColor.getIndex() == 1 || !s.airport) {
        if (comboColor.getIndex() == 0) {   // the mesonets without a temperature colouring: steel blue
            return QColor{110, 150, 215};
        }
        if (!SurfaceStation::has(s.temperature)) {
            return QColor{130, 130, 140};
        }
        const double f = s.temperature * 1.8 + 32.0;
        return f < 0 ? QColor{170, 110, 240} : f < 32 ? QColor{70, 110, 240} : f < 50 ? QColor{50, 190, 230} : f < 70 ? QColor{70, 200, 100} :
            f < 85 ? QColor{240, 215, 50} : f < 95 ? QColor{255, 150, 30} : QColor{235, 60, 50};
    }
    if (s.flight == "VFR") return QColor{60, 200, 80};
    if (s.flight == "MVFR") return QColor{70, 140, 255};
    if (s.flight == "IFR") return QColor{240, 60, 60};
    if (s.flight == "LIFR") return QColor{235, 70, 220};
    return QColor{140, 140, 150};
}

// painted over the map with the pan / zoom transform removed: window units (1000 wide, from -500; from -250 down)
void SurfaceViewer::paintStations(QPainter& painter) {
    drawn.clear();
    if (all.empty() || !airportCheck->isChecked()) {
        if (all.empty()) {
            return;
        }
    }
    const auto p = projection();
    const auto& state = radar->mapState;
    const int widthPx = std::max(1, radar->width());
    const double perPixel = 1000.0 / widthPx;
    const double staff = 24.0;
    const double spacing = (barbCheck->isChecked() || valueCheck->isChecked()) ? 46.0 : 14.0;
    painter.setRenderHint(QPainter::Antialiasing, true);
    QFont font{painter.font()};
    font.setPixelSize(std::max(6, static_cast<int>(std::lround(11.0 * perPixel))));
    painter.setFont(font);
    const int columns = static_cast<int>(std::ceil(widthPx / spacing)) + 2;
    std::unordered_set<long long> taken;
    const auto cellOf = [&] (double x, double y) { return static_cast<long long>(std::floor(y / spacing) + 1) * columns + static_cast<long long>(std::floor(x / spacing) + 1); };
    const long long rows = static_cast<long long>(std::ceil(radar->height() / spacing)) + 2;
    (void)rows;
    for (size_t i = 0; i < all.size(); i++) {
        const auto& s = all[i];
        if (s.airport && !airportCheck->isChecked()) {
            continue;
        }
        const double u = (p.ax * s.lon + p.bx) * state.zoom + state.xPos;
        const double v = (p.ay * s.mercator + p.by) * state.zoom + state.yPos;
        if (u < -500.0 || u > 500.0 || v < -250.0 || v > 750.0) {
            continue;
        }
        const double px = (u + 500.0) / perPixel;
        const double py = (v + 250.0) / perPixel;
        const auto cell = cellOf(px, py);
        if (!taken.insert(cell).second) {
            continue;
        }
        drawn.push_back({static_cast<int>(i), QPointF{px, py}});
        const bool old = static_cast<long>(std::time(nullptr)) - s.seconds > (s.airport ? 7200 : 10800);
        QColor color = dotColor(s);
        if (old) {
            color = QColor{110, 110, 118};
        }
        if (barbCheck->isChecked() && SurfaceStation::has(s.windSpeed) && !old) {
            const QColor barb{225, 228, 235};
            painter.setPen(QPen{barb, 1.2 * perPixel});
            painter.setBrush(barb);
            WindBarb::draw(painter, QPointF{u, v}, SurfaceStation::has(s.windDirection) ? s.windDirection : 0.0, s.windSpeed, staff * perPixel, s.lat < 0.0);
        }
        painter.setPen(QPen{QColor{255, 255, 255, 215}, 1.0 * perPixel});
        painter.setBrush(color);
        const double r = 3.6 * perPixel;
        if (s.airport) {
            painter.drawEllipse(QPointF{u, v}, r, r);
        } else {
            painter.drawRect(QRectF{u - r * 0.85, v - r * 0.85, r * 1.7, r * 1.7});   // a square: not an airport
        }
        if (valueCheck->isChecked() && !old) {
            const QFontMetricsF metrics{font};
            if (SurfaceStation::has(s.temperature)) {
                const QString t = number(shownTemperature(s.temperature));
                painter.setPen(QColor{255, 110, 100});
                painter.drawText(QPointF{u - r - 2.0 * perPixel - metrics.horizontalAdvance(t), v - 2.0 * perPixel}, t);
            }
            if (SurfaceStation::has(s.dewPoint)) {
                const QString d = number(shownTemperature(s.dewPoint));
                painter.setPen(QColor{90, 220, 110});
                painter.drawText(QPointF{u - r - 2.0 * perPixel - metrics.horizontalAdvance(d), v + metrics.ascent() * 0.9}, d);
            }
        }
    }
}

void SurfaceViewer::paintLegend(QPainter& painter) {
    const double perPixel = 1000.0 / std::max(1, radar->width());
    QFont font{painter.font()};
    font.setPointSizeF(9.0);
    painter.setFont(font);
    double x = -490.0;
    const double y = 735.0;
    const bool byTemperature = comboColor.getIndex() == 1;
    const auto entry = [&] (const QColor& color, const QString& label, bool square) {
        painter.setPen(QPen{QColor{255, 255, 255, 200}, 0.9 * perPixel});
        painter.setBrush(color);
        if (square) {
            painter.drawRect(QRectF{x, y - 5.0, 10.0, 10.0});
        } else {
            painter.drawEllipse(QPointF{x + 5.0, y}, 5.0, 5.0);
        }
        painter.setPen(QColor{235, 235, 235});
        painter.drawText(QPointF{x + 15.0, y + 4.0}, label);
        x += 28.0 + QFontMetricsF{font}.horizontalAdvance(label);
    };
    if (byTemperature) {
        for (const auto& [name, f] : {std::pair<const char *, double>{"<0 F", -10}, {"0-32", 10}, {"32-50", 40}, {"50-70", 60}, {"70-85", 78}, {"85-95", 90}, {"95+", 100}}) {
            SurfaceStation probe;
            probe.airport = true;
            probe.temperature = (f - 32.0) / 1.8;
            entry(dotColor(probe), name, false);
        }
    } else {
        for (const auto& [name, flight] : {std::pair<const char *, const char *>{"VFR", "VFR"}, {"MVFR", "MVFR"}, {"IFR", "IFR"}, {"LIFR", "LIFR"}}) {
            SurfaceStation probe;
            probe.airport = true;
            probe.flight = flight;
            entry(dotColor(probe), name, false);
        }
        if (mesoCheck->isChecked()) {
            entry(QColor{110, 150, 215}, "mesonet / other network", true);
        }
    }
    entry(QColor{110, 110, 118}, "old report", false);
}

SurfaceViewer::Projection2 SurfaceViewer::projection() const {
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

void SurfaceViewer::showConus() {
    auto& state = radar->mapState;
    const auto [ax, bx, ay, by] = projection();
    const double centerLon = -96.0;
    const double centerLat = 37.5;
    const double lonSpan = 62.0;
    state.zoom = 1000.0 / (std::abs(ax) * lonSpan);
    state.xPos = -(ax * centerLon + bx) * state.zoom;
    state.yPos = 250.0 - (ay * mercatorOf(centerLat) + by) * state.zoom;
}

void SurfaceViewer::fitRadar() {
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

void SurfaceViewer::resizeEventCustom() {
    fitRadar();
}

void SurfaceViewer::changeZoom(double factor) {
    auto& state = radar->mapState;
    if (factor < 1.0 && state.zoom <= 0.02) {
        return;
    }
    const double oldZoom = state.zoom;
    state.zoom = std::min(state.zoom * factor, 400.0);
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

void SurfaceViewer::changePosition(double dx, double dy) {
    auto& state = radar->mapState;
    const double unitsPerPixel = 1000.0 / std::max(1, radar->width());
    state.xPos += dx * unitsPerPixel;
    state.yPos += dy * unitsPerPixel;
    radar->mapTextObject.add();
    radar->update();
}

int SurfaceViewer::pickAt(const QPointF& widgetPos) const {
    int best = -1;
    double bestDistance = 13.0;
    for (const auto& d : drawn) {
        const double distance = std::hypot(d.at.x() - widgetPos.x(), d.at.y() - widgetPos.y());
        if (distance < bestDistance) {
            bestDistance = distance;
            best = d.index;
        }
    }
    return best;
}

bool SurfaceViewer::eventFilter(QObject * object, QEvent * event) {
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

void SurfaceViewer::showHover(const QPointF& widgetPos) {
    const int index = pickAt(widgetPos);
    if (index < 0) {
        hoverLabel->hide();
        radar->setCursor(Qt::ArrowCursor);
        return;
    }
    radar->setCursor(Qt::PointingHandCursor);
    hoverLabel->setText(summary(all[static_cast<size_t>(index)], comboUnits.getIndex() == 0) + "\n(click for the whole report)");
    hoverLabel->adjustSize();
    hoverLabel->move(12, 12);
    hoverLabel->show();
    hoverLabel->raise();
}

QString SurfaceViewer::summary(const SurfaceStation& s, bool fahrenheit) {
    const auto temperature = [fahrenheit] (double c) { return number(fahrenheit ? c * 1.8 + 32.0 : c) + (fahrenheit ? " F" : " C"); };
    QString text = QString::fromStdString(s.id + (s.name.empty() ? "" : "  " + s.name + (s.state.empty() ? "" : ", " + s.state)) + "   [" + s.network + "]");
    text += "\n" + QString::fromStdString(UtilityHdob::timeText(s.seconds)) + "  (" + ageText(s.seconds) + ")";
    text += "\nWind " + windText(s);
    if (SurfaceStation::has(s.temperature)) {
        text += "\nTemperature " + temperature(s.temperature);
        if (SurfaceStation::has(s.dewPoint)) {
            text += ", dew point " + temperature(s.dewPoint);
        }
    }
    if (SurfaceStation::has(s.altimeter)) {
        text += "\nAltimeter " + number(s.altimeter, 2) + " inHg";
    }
    if (!s.flight.empty()) {
        text += "   " + QString::fromStdString(s.flight);
    }
    return text;
}

QString SurfaceViewer::details(const SurfaceStation& s) {
    const auto both = [] (double c) { return number(c * 1.8 + 32.0, 1) + " F  (" + number(c, 1) + " C)"; };
    QString text = QString::fromStdString(s.id + (s.name.empty() ? "" : "  -  " + s.name)) + "\n\n";
    text += "Network:        " + QString::fromStdString(s.network) + (s.airport ? "  (airport METAR)" : "") + "\n";
    if (!s.state.empty()) {
        text += "State:          " + QString::fromStdString(s.state) + "\n";
    }
    text += "Position:       " + number(s.lat, 4) + ", " + number(s.lon, 4);
    if (SurfaceStation::has(s.elevation)) {
        text += "     elevation " + number(s.elevation) + " m  (" + number(s.elevation * 3.28084) + " ft)";
    }
    text += "\nObserved:       " + QString::fromStdString(UtilityHdob::timeText(s.seconds)) + "  (" + ageText(s.seconds) + ")\n\n";
    text += "Wind:           " + windText(s);
    if (SurfaceStation::has(s.windSpeed)) {
        text += "   (" + number(s.windSpeed * 1.15078) + " mph)";
    }
    text += "\n";
    if (SurfaceStation::has(s.temperature)) {
        text += "Temperature:    " + both(s.temperature) + "\n";
    }
    if (SurfaceStation::has(s.dewPoint)) {
        text += "Dew point:      " + both(s.dewPoint) + "\n";
    }
    if (SurfaceStation::has(s.humidity)) {
        text += "Humidity:       " + number(s.humidity) + " %\n";
    }
    if (SurfaceStation::has(s.altimeter)) {
        text += "Altimeter:      " + number(s.altimeter, 2) + " inHg  (" + number(s.altimeter * 33.8639, 1) + " mb)\n";
    }
    if (SurfaceStation::has(s.seaLevel)) {
        text += "Sea level:      " + number(s.seaLevel, 1) + " mb\n";
    }
    if (SurfaceStation::has(s.visibility)) {
        text += "Visibility:     " + number(s.visibility, 1) + " mi\n";
    }
    if (!s.weather.empty()) {
        text += "Weather:        " + QString::fromStdString(s.weather) + "\n";
    }
    if (!s.sky.empty()) {
        text += "Sky:            " + QString::fromStdString(s.sky) + "\n";
    }
    if (!s.flight.empty()) {
        text += "Flight category: " + QString::fromStdString(s.flight) + "\n";
    }
    if (s.quality != ' ' && s.quality != '\0') {
        text += QString{"Quality check:  "} + s.quality + "  (MADIS: V verified, S screened, C coarse pass, Q questioned)\n";
    }
    if (!s.raw.empty()) {
        text += "\n" + QString::fromStdString(s.raw) + "\n";
    }
    text += "\nAs reported; the mesonet and citizen stations are not all of the same quality.";
    return text;
}
