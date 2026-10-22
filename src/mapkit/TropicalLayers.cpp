// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "mapkit/MarkText.h"
#include "mapkit/TropicalLayers.h"
#include <algorithm>
#include <cmath>
#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QLabel>
#include <QPainterPath>
#include <QPushButton>
#include <QVBoxLayout>
#include "hurricane/EnsembleStyle.h"
#include "hurricane/HurricaneViewer.h"
#include "tropical/CiraStorm.h"
#include "objects/URL.h"
#include "hurricane/UtilityWeatherLab.h"
#include "hurricane/UtilityAtcf.h"
#include "hurricane/UtilityNhcGis.h"
#include "hurricane/UtilityTropicalAlerts.h"
#include "hurricane/UtilityWindProbability.h"
#include "ui/UiStandards.h"

namespace {
    QColor categoryColor(int wind) {
        static const QColor colors[] = {QColor{94, 186, 255}, QColor{0, 235, 230}, QColor{255, 255, 204}, QColor{255, 231, 117},
                                        QColor{255, 193, 64}, QColor{255, 143, 32}, QColor{255, 96, 96}};
        return colors[std::clamp(wind >= 0 ? UtilityAtcf::categoryOf(wind) : 0, 0, 6)];
    }

    // JMA's classes (10 minute winds), drawn as the NHC draws D / S / H / M: a letter in a dot of the class's colour
    QColor jmaColor(const string& category, int knots) {
        if (category == "TD") return QColor{94, 186, 255};
        if (category == "TS") return QColor{0, 235, 230};
        if (category == "STS") return QColor{255, 231, 117};
        if (category == "TY") return knots >= 105 ? QColor{255, 96, 96} : knots >= 85 ? QColor{255, 143, 32} : QColor{255, 193, 64};
        return QColor{190, 190, 190};   // a low, an extratropical cyclone
    }
    QString jmaLetter(const string& category, int knots) {
        if (category == "TD") return "D";
        if (category == "TS") return "S";
        if (category == "STS") return "SS";
        if (category == "TY") return knots >= 105 ? "VT" : "T";
        return "L";
    }
    QString jmaWords(const string& category) {
        if (category == "TD") return "tropical depression";
        if (category == "TS") return "tropical storm";
        if (category == "STS") return "severe tropical storm";
        if (category == "TY") return "typhoon";
        return category.empty() ? QString{"low"} : QString::fromStdString(category);
    }

    // the wind field of one level (34, 50 or 64 kt) as JTWC gives it: a radius in nautical miles in each quadrant (NE, SE, SW, NW), as a shape round the centre
    QPolygonF windField(const MapView::Transform& t, double lat, double lon, const double radiusNm[4]) {
        QPolygonF polygon;
        const double cosLat = std::max(0.05, std::cos(lat * 3.14159265358979 / 180.0));
        for (int q = 0; q < 4; q++) {
            const double km = radiusNm[q] * 1.852;
            for (int step = 0; step <= 12; step++) {
                const double bearing = (q * 90.0 + step * 7.5) * 3.14159265358979 / 180.0;
                polygon << t(lat + km * std::cos(bearing) / 111.2, lon + km * std::sin(bearing) / (111.2 * cosLat));
            }
        }
        return polygon;
    }

    QString knots(int wind) {
        return QString::fromStdString(UtilityAtcf::windLabel(wind));
    }

    QPainterPath ringsPath(const MapView::Transform& t, const std::vector<std::vector<std::pair<double, double>>>& rings) {
        QPainterPath path;
        for (const auto& ring : rings) {
            QPolygonF polygon;
            for (const auto& [lon, lat] : ring) {
                polygon << t(lat, lon);
            }
            path.addPolygon(polygon);
            path.closeSubpath();
        }
        return path;
    }
}

// ---- storms outside the NHC's basins (typhoons ...) ----

string WorldStormsLayer::summary() const {
    if (loading && !storms) return "reading the storms of the world...";
    if (!error.empty()) return error;
    if (!storms) return {};
    return storms->empty() ? string{"no active storms outside the Atlantic and East Pacific"} : std::to_string(storms->size()) + " storms (" + [&] {
        string names;
        for (const auto& s : *storms) {
            names += (names.empty() ? "" : ", ") + (s.info.name.empty() ? s.info.id : s.info.name);
        }
        return names;
    }() + ")";
}

void WorldStormsLayer::refresh(MapHost& host) {
    if (loading) {
        return;
    }
    loading = true;
    auto fresh = std::make_shared<vector<Storm>>();
    auto message = std::make_shared<string>();
    host.background(
        [fresh, message] {
            vector<UtilityCira::Storm> all;
            if (!UtilityCira::activeStorms(all, *message)) {
                return;
            }
            for (const auto& info : all) {
                const auto prefix = info.id.substr(0, 2);
                if (prefix == "al" || prefix == "ep" || prefix == "cp") {   // the NHC's: the active storms layer draws them
                    continue;
                }
                Storm s;
                s.info = info;
                string problem;
                if (UtilityCira::stormPage(info.id, s.page, problem)) {
                    if (prefix == "wp") {
                        UtilityJma::forecastCircles(info.name, s.circles);   // (the western Pacific is JMA's)
                    }
                    if (const auto code = UtilityJtwc::codeOf(info.id); !code.empty()) {
                        s.warning = UtilityJtwc::parseWarning(URL::getText(UtilityJtwc::warningUrl(code)));
                    }
                    fresh->push_back(std::move(s));
                }
            }
        },
        [this, &host, fresh, message] {
            loading = false;
            storms = fresh;
            error = fresh->empty() ? *message : string{};
            host.redraw();
        });
}

void WorldStormsLayer::paint(QPainter& painter, MapHost& host) {
    if (!storms) {
        return;
    }
    const auto t = host.view().transform();
    const double px = host.view().unitsPerPixel();
    QFont font{painter.font()};
    font.setPixelSize(static_cast<int>(11 * px));
    painter.setFont(font);
    for (const auto& s : *storms) {
        if (showDanger && s.circles.size() < 2 && s.warning.size() >= 2) {   // no JMA cone (the Indian Ocean, the Southern Hemisphere): JTWC's own area, the 34 kt wind radii along the forecast
            QPainterPath area;
            area.setFillRule(Qt::WindingFill);
            for (size_t i = 0; i + 1 < s.warning.size(); i++) {
                const auto& a = s.warning[i];
                const auto& b = s.warning[i + 1];
                for (int k = 0; k <= 8; k++) {
                    const double f = k / 8.0;
                    double radii[4];
                    for (int q = 0; q < 4; q++) {
                        radii[q] = a.radiusNm[0][q] + (b.radiusNm[0][q] - a.radiusNm[0][q]) * f;
                    }
                    area.addPolygon(windField(t, a.lat + (b.lat - a.lat) * f, a.lon + (b.lon - a.lon) * f, radii));
                    area.closeSubpath();
                }
            }
            painter.setPen(QPen{QColor{255, 255, 255, 190}, 1.4 * px, Qt::DashLine});
            painter.setBrush(QColor{255, 255, 255, 38});
            painter.drawPath(area.simplified());
        }
        if (showCone && s.circles.size() >= 2) {   // the cone: the circles along the forecast and between them, as one shape
            QPainterPath cone;
            cone.setFillRule(Qt::WindingFill);   // (overlapping circles join, they do not cancel)
            const auto circleAt = [&] (double lat, double lon, double km) {
                const double cosLat = std::max(0.05, std::cos(lat * 3.14159265358979 / 180.0));
                const auto centre = t(lat, lon);
                const double radius = std::abs(t(lat, lon + km / (111.2 * cosLat)).x() - centre.x());
                cone.addEllipse(centre, radius, radius);
            };
            for (size_t i = 0; i + 1 < s.circles.size(); i++) {
                const auto& a = s.circles[i];
                const auto& b = s.circles[i + 1];
                for (int k = 0; k <= 12; k++) {
                    const double f = k / 12.0;
                    circleAt(a.lat + (b.lat - a.lat) * f, a.lon + (b.lon - a.lon) * f, a.radiusKm + (b.radiusKm - a.radiusKm) * f);
                }
            }
            painter.setPen(QPen{QColor{255, 255, 255, 190}, 1.4 * px, Qt::DashLine});
            painter.setBrush(QColor{255, 255, 255, 38});
            painter.drawPath(cone.simplified());
        }
        const auto& history = s.page.history;
        if (history.size() >= 2) {   // newest first
            QPainterPath path;
            for (size_t i = 0; i < history.size(); i++) {
                const auto p = t(history[i].lat, history[i].lon);
                i == 0 ? path.moveTo(p) : path.lineTo(p);
            }
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen{QColor{0, 0, 0, 200}, 4.0 * px});
            painter.drawPath(path);
            painter.setPen(QPen{QColor{235, 235, 235}, 1.8 * px});
            painter.drawPath(path);
            for (size_t i = 0; i < history.size(); i += 2) {
                painter.setPen(QPen{QColor{0, 0, 0, 220}, 0.9 * px});
                painter.setBrush(categoryColor(history[i].knots));
                painter.drawEllipse(t(history[i].lat, history[i].lon), 3.0 * px, 3.0 * px);
            }
        }
        if (s.circles.size() >= 2) {   // JMA's forecast, as the NHC shows its own: the line, a lettered dot of the class at each fix with its day and hour (UTC); JTWC's goes thin beside it
            QPainterPath jtwc;
            for (size_t i = 0; i < s.page.forecast.size(); i++) {
                const auto p = t(s.page.forecast[i].lat, s.page.forecast[i].lon);
                i == 0 ? jtwc.moveTo(p) : jtwc.lineTo(p);
            }
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen{QColor{190, 190, 190, 200}, 1.2 * px, Qt::DotLine});
            painter.drawPath(jtwc);
            QPainterPath line;
            for (size_t i = 0; i < s.circles.size(); i++) {
                const auto p = t(s.circles[i].lat, s.circles[i].lon);
                i == 0 ? line.moveTo(p) : line.lineTo(p);
            }
            painter.setPen(QPen{QColor{0, 0, 0, 200}, 5.0 * px});
            painter.drawPath(line);
            painter.setPen(QPen{QColor{255, 255, 255}, 2.6 * px});
            painter.drawPath(line);
            QFont letterFont{font};
            letterFont.setBold(true);
            letterFont.setPixelSize(static_cast<int>(8 * px));
            for (size_t i = 1; i < s.circles.size(); i++) {
                const auto& c = s.circles[i];
                const auto p = t(c.lat, c.lon);
                painter.setPen(QPen{QColor{0, 0, 0, 230}, 1.0 * px});
                painter.setBrush(jmaColor(c.category, c.knots));
                painter.drawEllipse(p, 7.0 * px, 7.0 * px);
                painter.setFont(letterFont);
                painter.setPen(QColor{20, 20, 20});
                MarkText::centred(host, painter, px, c.lat, c.lon, 7.0, jmaLetter(c.category, c.knots));
                painter.setFont(font);
                const auto valid = QDateTime::fromString(QString::fromStdString(c.validUtc), Qt::ISODate).toUTC();
                if (valid.isValid()) {
                    painter.setPen(QColor{255, 255, 255});
                    MarkText::at(host, painter, px, c.lat, c.lon, QPointF{9.0, 4.0}, valid.toString("ddd HH'Z'"));
                }
            }
        }
        const auto& forecast = s.circles.size() >= 2 ? vector<UtilityCira::Fix>{} : s.page.forecast;
        if (forecast.size() >= 2) {
            QPainterPath path;
            for (size_t i = 0; i < forecast.size(); i++) {
                const auto p = t(forecast[i].lat, forecast[i].lon);
                i == 0 ? path.moveTo(p) : path.lineTo(p);
            }
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen{QColor{0, 0, 0, 200}, 5.0 * px});
            painter.drawPath(path);
            painter.setPen(QPen{QColor{255, 255, 255}, 2.6 * px});
            painter.drawPath(path);
            for (const auto& f : forecast) {
                const auto p = t(f.lat, f.lon);
                painter.setPen(QPen{QColor{0, 0, 0, 220}, 1.0 * px});
                painter.setBrush(categoryColor(f.knots));
                painter.drawEllipse(p, 4.2 * px, 4.2 * px);
                if (f.hour > 0 && f.hour % 24 == 0) {
                    painter.setPen(QColor{255, 255, 255});
                    MarkText::at(host, painter, px, f.lat, f.lon, QPointF{7.0, -5.0}, QString::number(f.hour / 24) + " d");
                }
            }
        }
        if (showRadii && !s.warning.empty()) {   // the wind field now (34 kt yellow, 50 orange, 64 red), and the 34 kt outline at each forecast position
            static const QColor fills[3] = {QColor{255, 231, 117, 70}, QColor{255, 143, 32, 90}, QColor{255, 60, 60, 110}};
            painter.setPen(QPen{QColor{255, 255, 255, 120}, 0.8 * px});
            for (size_t i = 1; i < s.warning.size(); i++) {
                painter.setBrush(Qt::NoBrush);
                painter.drawPolygon(windField(t, s.warning[i].lat, s.warning[i].lon, s.warning[i].radiusNm[0]));
            }
            for (int level = 0; level < 3; level++) {
                painter.setBrush(fills[level]);
                painter.setPen(QPen{fills[level].darker(150), 1.0 * px});
                painter.drawPolygon(windField(t, s.warning.front().lat, s.warning.front().lon, s.warning.front().radiusNm[level]));
            }
        }
        const auto& first = !s.page.forecast.empty() ? s.page.forecast.front() : (!history.empty() ? history.front() : UtilityCira::Fix{});
        if (!s.page.forecast.empty() || !history.empty()) {
            const QPointF now = t(first.lat, first.lon);
            painter.setPen(QPen{QColor{0, 0, 0, 230}, 1.4 * px});
            painter.setBrush(categoryColor(first.knots));
            painter.drawEllipse(now, 8.0 * px, 8.0 * px);
            painter.setPen(QColor{255, 255, 255});
            QFont bold{font};
            bold.setBold(true);
            painter.setFont(bold);
            MarkText::at(host, painter, px, first.lat, first.lon, QPointF{11.0, 4.0}, QString::fromStdString(s.info.name.empty() ? s.info.id : s.info.id + " " + s.info.name));
            painter.setFont(font);
        }
    }
}

MapHit WorldStormsLayer::pick(const QPointF& pixels, MapHost& host) const {
    MapHit hit;
    hit.reach = 14.0;
    hit.priority = 30;
    if (!storms) {
        return hit;
    }
    auto& view = host.view();
    for (const auto& s : *storms) {
        const QString name = QString::fromStdString(s.info.title);
        const auto consider = [&] (const UtilityCira::Fix& f, const QString& what) {
            const auto at = view.toPixels(f.lat, f.lon);
            const double distance = std::hypot(at.x() - pixels.x(), at.y() - pixels.y());
            if (distance < hit.distance) {
                hit.distance = distance;
                hit.text = name + "\n" + what + (f.knots >= 0 ? ": " + knots(f.knots) : QString{});
                const string id = s.info.id, title = s.info.title;
                hit.open = [id, title] (Window * parent) { new CiraStorm{parent, id, title}; };
            }
        };
        for (const auto& f : s.page.forecast) {
            consider(f, f.hour == 0 ? QString{"JTWC now"} : "JTWC forecast +" + QString::number(f.hour) + " h");
        }
        for (const auto& c : s.circles) {   // JMA: the class, the 10 minute wind, the pressure, the motion and the circle
            UtilityCira::Fix f;
            f.lat = c.lat;
            f.lon = c.lon;
            const auto at = view.toPixels(c.lat, c.lon);
            const double distance = std::hypot(at.x() - pixels.x(), at.y() - pixels.y());
            if (distance < hit.distance) {
                hit.distance = distance;
                const auto valid = QDateTime::fromString(QString::fromStdString(c.validUtc), Qt::ISODate).toUTC();
                hit.text = name + "\nJMA " + (c.hour == 0 ? QString{"analysis"} : "forecast +" + QString::number(c.hour) + " h") + (valid.isValid() ? " (" + valid.toString("ddd d MMM HH'Z'") + ")" : QString{}) +
                           "\n" + jmaWords(c.category) + ", " + QString::number(c.knots) + " kt (10 min), gusts " + QString::number(c.gustKnots) + " kt" + (c.pressure > 0 ? ", " + QString::number(c.pressure) + " hPa" : QString{}) +
                           (c.motion.empty() ? QString{} : "\nmoving " + QString::fromStdString(c.motion)) + (c.radiusKm > 0 ? "\n70% probability circle: " + QString::number(std::lround(c.radiusKm / 1.852)) + " nm (" + QString::number(std::lround(c.radiusKm)) + " km)" : QString{});
                const string id = s.info.id, title = s.info.title;
                hit.open = [id, title] (Window * parent) { new CiraStorm{parent, id, title}; };
            }
        }
        for (const auto& f : s.page.history) {
            consider(f, QString::fromStdString(f.time) + "Z");
        }
    }
    if (hit.valid()) {
        hit.text += "\n(click for its page, loops and SAR winds)";
    }
    return hit;
}

vector<MapLegendRow> WorldStormsLayer::legend() const {
    MapLegendRow row;
    row.title = "Storm intensity (1 min wind):";
    static const char * names[] = {"TD", "TS", "Cat 1", "Cat 2", "Cat 3", "Cat 4", "Cat 5"};
    static const int winds[] = {20, 40, 70, 85, 100, 120, 140};
    for (int c = 0; c < 7; c++) {
        row.entries.push_back({MapLegendEntry::Circle, categoryColor(winds[c]), names[c]});
    }
    MapLegendRow cone;
    cone.title = "Cone:";
    cone.entries = {{MapLegendEntry::Line, QColor{255, 255, 255}, "JMA 70% probability circles (typhoons), else JTWC's 34 kt wind area"},
                    {MapLegendEntry::Square, QColor{255, 231, 117}, "34 kt"}, {MapLegendEntry::Square, QColor{255, 143, 32}, "50 kt"}, {MapLegendEntry::Square, QColor{255, 60, 60}, "64 kt wind radii"}};
    MapLegendRow jma;
    jma.title = "JMA forecast points:";
    jma.entries = {{MapLegendEntry::Circle, jmaColor("TD", 0), "D depression"}, {MapLegendEntry::Circle, jmaColor("TS", 0), "S tropical storm"}, {MapLegendEntry::Circle, jmaColor("STS", 0), "SS severe tropical storm"},
                   {MapLegendEntry::Circle, jmaColor("TY", 70), "T typhoon"}, {MapLegendEntry::Circle, jmaColor("TY", 105), "VT 105 kt or more"}, {MapLegendEntry::Line, QColor{190, 190, 190}, "JTWC forecast (dotted)"}};
    return {row, jma, cone};
}

QWidget * WorldStormsLayer::options(QWidget * parent, const std::function<void()>& changed) {
    auto * widget = new QWidget{parent};
    auto * layout = new QVBoxLayout{widget};
    layout->setContentsMargins(0, 0, 0, 0);
    auto * c = new QCheckBox{"Cone (JMA's 70% probability circles, western Pacific)", widget};
    c->setChecked(showCone);
    QObject::connect(c, &QCheckBox::toggled, [this, changed] (bool on) { showCone = on; changed(); });
    layout->addWidget(c);
    auto * radii = new QCheckBox{"Wind radii (JTWC: 34 / 50 / 64 kt now, 34 kt along the forecast)", widget};
    radii->setChecked(showRadii);
    QObject::connect(radii, &QCheckBox::toggled, [this, changed] (bool on) { showRadii = on; changed(); });
    layout->addWidget(radii);
    auto * danger = new QCheckBox{"Where JMA has no cone: JTWC's 34 kt wind area along the forecast", widget};
    danger->setChecked(showDanger);
    QObject::connect(danger, &QCheckBox::toggled, [this, changed] (bool on) { showDanger = on; changed(); });
    layout->addWidget(danger);
    return widget;
}

// ---- active storms ----

string ActiveStormsLayer::summary() const {
    if (loading && !storms) return "reading the active storms...";
    if (!error.empty()) return error;
    if (!storms) return {};
    int named = 0;
    for (const auto& s : *storms) named += s.entry.classification == "TD" || s.entry.classification == "TS" || s.entry.classification == "HU" ? 1 : 0;
    return std::to_string(named) + " active storms, " + std::to_string(storms->size() - static_cast<size_t>(named)) + " other systems";
}

void ActiveStormsLayer::refresh(MapHost& host) {
    if (loading) {
        return;
    }
    loading = true;
    auto fresh = std::make_shared<vector<Storm>>();
    auto message = std::make_shared<string>();
    host.background(
        [fresh, message] {
            for (const auto * basin : {"al", "ep", "cp"}) {
                vector<HurricaneData::StormEntry> entries;
                string error;
                if (!HurricaneData::loadStormList(entries, error, basin)) {
                    *message = error;
                    continue;
                }
                for (const auto& entry : entries) {
                    if (!entry.active) {
                        continue;
                    }
                    Storm s;
                    s.entry = entry;
                    s.basin = basin;
                    s.data = std::make_shared<HurricaneData::StormData>();
                    HurricaneData::loadStorm(entry.id, *s.data);
                    s.gis = std::make_shared<HurricaneData::GisData>();
                    HurricaneData::loadGis(entry, *s.gis);
                    fresh->push_back(std::move(s));
                }
            }
        },
        [this, &host, fresh, message] {
            loading = false;
            storms = fresh;
            error = fresh->empty() ? *message : string{};
            host.redraw();
        });
}

void ActiveStormsLayer::paint(QPainter& painter, MapHost& host) {
    if (!storms) {
        return;
    }
    const auto t = host.view().transform();
    const double px = host.view().unitsPerPixel();
    QFont font{painter.font()};
    font.setPixelSize(static_cast<int>(11 * px));
    painter.setFont(font);
    for (const auto& s : *storms) {
        const auto& gis = *s.gis;
        // the zones in effect inland, under everything else of the storm
        if (showInland && showWarnings) {
            vector<const UtilityTropicalAlerts::Area *> ordered;
            for (const auto& a : gis.inland) ordered.push_back(&a);
            std::stable_sort(ordered.begin(), ordered.end(), [] (const auto * a, const auto * b) { return UtilityTropicalAlerts::rank(a->code) < UtilityTropicalAlerts::rank(b->code); });
            for (const auto * a : ordered) {
                QColor color{QString::fromStdString(UtilityNhcGis::colorFor(a->code))};
                QColor fill = color;
                fill.setAlpha(a->code == "SSW" || a->code == "SSA" ? 55 : 95);
                painter.setPen(QPen{QColor{color.red(), color.green(), color.blue(), 200}, 1.0 * px});
                painter.setBrush(fill);
                painter.drawPath(ringsPath(t, a->rings));
            }
        }
        if (showCone && gis.cone.ok) {
            painter.setPen(QPen{QColor{255, 255, 255, 190}, 1.4 * px, Qt::DashLine});
            painter.setBrush(QColor{255, 255, 255, 38});
            painter.drawPath(ringsPath(t, gis.cone.polygons));
        }
        // the best track so far
        const auto& best = s.data->best;
        if (best.size() >= 2) {
            QPainterPath path;
            for (size_t i = 0; i < best.size(); i++) {
                const auto p = t(best[i].lat, best[i].lon);
                i == 0 ? path.moveTo(p) : path.lineTo(p);
            }
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen{QColor{0, 0, 0, 200}, 4.0 * px});
            painter.drawPath(path);
            painter.setPen(QPen{QColor{235, 235, 235}, 1.8 * px});
            painter.drawPath(path);
            for (size_t i = 0; i < best.size(); i += 2) {
                painter.setPen(QPen{QColor{0, 0, 0, 220}, 0.9 * px});
                painter.setBrush(categoryColor(best[i].wind));
                painter.drawEllipse(t(best[i].lat, best[i].lon), 3.0 * px, 3.0 * px);
            }
        }
        // NHC's forecast
        const auto& fixes = s.data->official.fixes;
        if (showForecast && !fixes.empty()) {
            QPainterPath path;
            for (size_t i = 0; i < fixes.size(); i++) {
                const auto p = t(fixes[i].lat, fixes[i].lon);
                i == 0 ? path.moveTo(p) : path.lineTo(p);
            }
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen{QColor{0, 0, 0, 200}, 5.0 * px});
            painter.drawPath(path);
            painter.setPen(QPen{QColor{255, 255, 255}, 2.6 * px});
            painter.drawPath(path);
            for (const auto& f : fixes) {
                const auto p = t(f.lat, f.lon);
                painter.setPen(QPen{QColor{0, 0, 0, 220}, 1.0 * px});
                painter.setBrush(categoryColor(f.wind));
                painter.drawEllipse(p, 4.2 * px, 4.2 * px);
                if (f.tau > 0 && f.tau % 24 == 0) {
                    painter.setPen(QColor{255, 255, 255});
                    MarkText::at(host, painter, px, f.lat, f.lon, QPointF{7.0, -5.0}, QString::number(f.tau / 24) + " d");
                }
            }
        }
        if (showWarnings) {
            for (const auto& w : gis.watchWarnings) {
                QPainterPath path;
                for (const auto& ring : w.lines) {
                    bool started = false;
                    for (const auto& [lon, lat] : ring) {
                        const auto pt = t(lat, lon);
                        started ? path.lineTo(pt) : path.moveTo(pt);
                        started = true;
                    }
                }
                painter.setBrush(Qt::NoBrush);
                painter.setPen(QPen{QColor{0, 0, 0, 220}, 8.0 * px, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin});
                painter.drawPath(path);
                painter.setPen(QPen{QColor{QString::fromStdString(UtilityNhcGis::colorFor(w.code))}, 5.0 * px, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin});
                painter.drawPath(path);
            }
        }
        // where it is now, with its name
        const QPointF now = t(s.entry.lat, s.entry.lon);
        painter.setPen(QPen{QColor{0, 0, 0, 230}, 1.4 * px});
        painter.setBrush(categoryColor(s.entry.wind));
        painter.drawEllipse(now, 8.0 * px, 8.0 * px);
        painter.setPen(QColor{255, 255, 255});
        QFont bold{font};
        bold.setBold(true);
        painter.setFont(bold);
        MarkText::at(host, painter, px, s.entry.lat, s.entry.lon, QPointF{11.0, 4.0}, QString::fromStdString(HurricaneData::idLabel(s.entry.id) + (s.entry.name.empty() ? "" : " " + s.entry.name)));
        painter.setFont(font);
    }
}

MapHit ActiveStormsLayer::pick(const QPointF& pixels, MapHost& host) const {
    MapHit hit;
    hit.reach = 14.0;
    hit.priority = 30;
    if (!storms) {
        return hit;
    }
    auto& view = host.view();
    const auto consider = [&] (double lat, double lon, const QString& text, const Storm& s) {
        const auto at = view.toPixels(lat, lon);
        const double distance = std::hypot(at.x() - pixels.x(), at.y() - pixels.y());
        if (distance < hit.distance) {
            hit.distance = distance;
            hit.text = text;
            const string basin = s.basin;
            const string id = s.entry.id;
            hit.open = [basin, id] (Window * parent) { new HurricaneViewer{parent, basin, id}; };
        }
    };
    for (const auto& s : *storms) {
        const QString name = QString::fromStdString(HurricaneData::idLabel(s.entry.id) + (s.entry.name.empty() ? "" : " " + s.entry.name));
        consider(s.entry.lat, s.entry.lon, name + "\n" + (s.entry.wind >= 0 ? knots(s.entry.wind) : QString{}) + (s.entry.pressure > 0 ? ", " + QString::number(s.entry.pressure) + " mb" : QString{}) +
            "\n(click for its track map and recon)", s);
        for (const auto& f : s.data->official.fixes) {
            consider(f.lat, f.lon, name + "\nNHC forecast +" + QString::number(f.tau) + " h: " + knots(f.wind), s);
        }
        for (const auto& f : s.data->best) {
            consider(f.lat, f.lon, name + "\n" + QString::fromStdString(UtilityAtcf::formatTime(f.time)) + ": " + knots(f.wind) + (f.pressure > 0 ? ", " + QString::number(f.pressure) + " mb" : QString{}), s);
        }
    }
    return hit;
}

vector<MapLegendRow> ActiveStormsLayer::legend() const {
    MapLegendRow row;
    row.title = "Storm intensity:";
    static const char * names[] = {"TD", "TS", "Cat 1", "Cat 2", "Cat 3", "Cat 4", "Cat 5"};
    static const int winds[] = {20, 40, 70, 85, 100, 120, 140};
    for (int c = 0; c < 7; c++) {
        row.entries.push_back({MapLegendEntry::Circle, categoryColor(winds[c]), names[c]});
    }
    MapLegendRow warnings;
    warnings.title = "Watches and warnings:";
    warnings.entries = {{MapLegendEntry::Line, QColor{255, 0, 0}, "hurricane warning"}, {MapLegendEntry::Line, QColor{255, 128, 192}, "hurricane watch"},
                        {MapLegendEntry::Line, QColor{0, 85, 255}, "tropical storm warning"}, {MapLegendEntry::Line, QColor{255, 215, 0}, "tropical storm watch"}};
    return {row, warnings};
}

QWidget * ActiveStormsLayer::options(QWidget * parent, const std::function<void()>& changed) {
    auto * widget = new QWidget{parent};
    auto * layout = new QVBoxLayout{widget};
    layout->setContentsMargins(0, 0, 0, 0);
    const auto check = [&] (const char * label, bool& value) {
        auto * c = new QCheckBox{label, widget};
        c->setChecked(value);
        QObject::connect(c, &QCheckBox::toggled, [&value, changed] (bool on) { value = on; changed(); });
        layout->addWidget(c);
    };
    check("NHC forecast track", showForecast);
    check("Forecast cone", showCone);
    check("Watches and warnings", showWarnings);
    check("  inland zones (NWS counties)", showInland);
    return widget;
}

// ---- outlook ----

string OutlookLayer::summary() const {
    return loading && !data ? "reading the outlook..." : data ? std::to_string(data->areas.size()) + " development areas" : string{};
}

void OutlookLayer::refresh(MapHost& host) {
    if (loading) {
        return;
    }
    loading = true;
    auto fresh = std::make_shared<HurricaneData::OutlookData>();
    host.background([fresh] { HurricaneData::loadOutlook(*fresh); }, [this, &host, fresh] {
        loading = false;
        data = fresh;
        host.redraw();
    });
}

void OutlookLayer::paint(QPainter& painter, MapHost& host) {
    if (!data) {
        return;
    }
    const auto t = host.view().transform();
    const double px = host.view().unitsPerPixel();
    QFont font{painter.font()};
    font.setPixelSize(static_cast<int>(12 * px));
    font.setBold(true);
    painter.setFont(font);
    for (const auto& area : data->areas) {
        const auto& risk = area.risk7.empty() ? area.risk2 : area.risk7;
        const QColor color = risk == "High" ? QColor{255, 60, 60} : risk == "Medium" ? QColor{255, 150, 30} : QColor{255, 225, 60};
        QColor fill = color;
        fill.setAlpha(45);
        painter.setPen(QPen{color, 2.2 * px, Qt::DashLine});
        painter.setBrush(fill);
        painter.drawPath(ringsPath(t, area.rings));
        painter.setPen(color.lighter(130));
        MarkText::at(host, painter, px, area.centerLat, area.centerLon, QPointF{-30.0, 4.0}, QString::number(area.prob2) + "% / " + QString::number(area.prob7) + "%");
    }
}

MapHit OutlookLayer::pick(const QPointF& pixels, MapHost& host) const {
    MapHit hit;
    hit.reach = 1.0;
    hit.priority = 1;
    if (!data) {
        return hit;
    }
    const auto [lat, lon] = host.view().toLatLon(pixels);
    for (const auto& area : data->areas) {
        for (const auto& ring : area.rings) {
            if (ring.size() >= 3 && UtilityWindProbability::inside(ring, lat, lon)) {
                hit.distance = 0.0;
                hit.text = "Tropical Weather Outlook area " + QString::fromStdString(area.area) + "\nChance of formation: " + QString::number(area.prob2) + " % in 2 days (" + QString::fromStdString(area.risk2) +
                    "), " + QString::number(area.prob7) + " % in 7 days (" + QString::fromStdString(area.risk7) + ")";
                return hit;
            }
        }
    }
    return hit;
}

vector<MapLegendRow> OutlookLayer::legend() const {
    return {MapLegendRow{"Development chance:", {{MapLegendEntry::Square, QColor{255, 225, 60}, "low"}, {MapLegendEntry::Square, QColor{255, 150, 30}, "medium"}, {MapLegendEntry::Square, QColor{255, 60, 60}, "high"}}}};
}

// ---- wind probabilities ----

string WindProbabilityLayer::summary() const {
    return loading && !data ? "reading the wind probabilities..." : data && !data->map.ok ? "no wind speed probabilities right now (no storm threatens)" : string{};
}

void WindProbabilityLayer::refresh(MapHost& host) {
    if (loading) {
        return;
    }
    loading = true;
    auto fresh = std::make_shared<HurricaneData::WspData>();
    host.background([fresh] { HurricaneData::loadWindProbabilities(*fresh); }, [this, &host, fresh] {
        loading = false;
        data = fresh;
        host.redraw();
    });
}

static QColor bandColor(int low) {
    if (low < 5) return QColor{120, 160, 220, 28};
    if (low < 10) return QColor{80, 190, 230, 60};
    if (low < 20) return QColor{70, 200, 120, 80};
    if (low < 30) return QColor{200, 220, 60, 95};
    if (low < 40) return QColor{250, 200, 40, 105};
    if (low < 50) return QColor{250, 150, 40, 115};
    if (low < 70) return QColor{240, 90, 50, 125};
    return QColor{210, 40, 120, 140};
}

void WindProbabilityLayer::paint(QPainter& painter, MapHost& host) {
    if (!data || !data->map.ok) {
        return;
    }
    const auto t = host.view().transform();
    // the bands are rings round the storm's track: a band's polygon covers the area above its level, so drawing each in turn, the lowest first,
    // builds up the colours inward
    painter.setPen(Qt::NoPen);
    for (const auto& band : data->map.bands[static_cast<size_t>(threshold)]) {
        if (band.rings.empty()) {
            continue;
        }
        painter.setBrush(bandColor(band.low));
        painter.drawPath(ringsPath(t, band.rings));
    }
}

MapHit WindProbabilityLayer::pick(const QPointF& pixels, MapHost& host) const {
    MapHit hit;
    hit.reach = 1.0;
    hit.priority = 0;
    if (!data || !data->map.ok) {
        return hit;
    }
    const auto [lat, lon] = host.view().toLatLon(pixels);
    const auto chance = UtilityWindProbability::at(data->map, threshold, lat, lon);
    if (chance.covered) {
        static const char * names[] = {"34", "50", "64"};
        hit.distance = 0.0;
        hit.text = QString{"Chance of "} + names[threshold] + " kt winds in the next 5 days: " + QString::fromStdString(chance.text());
    }
    return hit;
}

vector<MapLegendRow> WindProbabilityLayer::legend() const {
    MapLegendRow row;
    static const char * names[] = {"34", "50", "64"};
    row.title = QString{"Chance of "} + names[threshold] + " kt:";
    for (const auto& [label, low] : {std::pair<const char *, int>{"<5%", 0}, {"5-10", 5}, {"10-20", 10}, {"20-30", 20}, {"30-40", 30}, {"40-50", 40}, {"50-70", 50}, {"70%+", 70}}) {
        auto c = bandColor(low);
        c.setAlpha(255);
        row.entries.push_back({MapLegendEntry::Square, c, label});
    }
    return {row};
}

QWidget * WindProbabilityLayer::options(QWidget * parent, const std::function<void()>& changed) {
    auto * widget = new QWidget{parent};
    auto * layout = new QVBoxLayout{widget};
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(new QLabel{"Wind of at least:", widget});
    auto * combo = UiStandards::combo(widget);
    combo->addItems({"34 kt (tropical storm)", "50 kt", "64 kt (hurricane)"});
    combo->setCurrentIndex(threshold);
    QObject::connect(combo, &QComboBox::currentIndexChanged, [this, changed] (int index) { threshold = index; changed(); });
    layout->addWidget(combo);
    return widget;
}

// ---- raw ensemble spaghetti ----

int EnsembleSpaghettiLayer::modelIndex(const string& label) {
    static const char * labels[modelCount] = {"GEFS", "IFS ENS", "AIFS ENS", "DeepMind FNV3", "DeepMind WNV3"};
    for (int i = 0; i < modelCount; i++) {
        if (label == labels[i]) {
            return i;
        }
    }
    return -1;
}

void EnsembleSpaghettiLayer::showSystem(const string& atcfId) {
    system.clear();
    if (atcfId.size() >= 8) {   // wp122026: the number and the basin's letters ("12" and W)
        static const std::map<string, string> letters{{"al", "L"}, {"ep", "E"}, {"cp", "C"}, {"wp", "W"}, {"io", "AB"}, {"sh", "SPAU"}};
        string basin = atcfId.substr(0, 2);
        std::transform(basin.begin(), basin.end(), basin.begin(), [] (unsigned char c) { return static_cast<char>(std::tolower(c)); });
        const auto found = letters.find(basin);
        system = atcfId.substr(2, 2) + (found != letters.end() ? found->second : string{});
    }
}

bool EnsembleSpaghettiLayer::wanted(const UtilityEcmwfTracks::Storm& storm) const {
    if (system.size() < 3) {
        return true;   // every system
    }
    return storm.id.size() == 3 && storm.id.compare(0, 2, system, 0, 2) == 0 && system.find(storm.id[2], 2) != string::npos;
}

int EnsembleSpaghettiLayer::limitHours() const {
    static const int hours[] = {10000, 240, 168, 120, 72, 48, 24};
    return hours[std::clamp(hoursChoice, 0, 6)];
}

string EnsembleSpaghettiLayer::summary() const {
    if (loading && !runs) return "reading the ensembles' tracks (GEFS, ECMWF, DeepMind)...";
    if (!runs) return error;
    string text;
    for (const auto& run : *runs) {
        size_t members = 0;
        for (const auto& storm : run.storms) members += storm.members.size();
        text += (text.empty() ? "" : "; ") + run.label + " " + (run.cycle.size() == 10 ? run.cycle.substr(8, 2) + "z" : string{}) + ": " + std::to_string(run.storms.size()) + " systems, " + std::to_string(members) + " tracks";
    }
    return text.empty() ? error : text;
}

void EnsembleSpaghettiLayer::refresh(MapHost& host) {
    if (loading) {
        return;
    }
    loading = true;
    auto fresh = std::make_shared<vector<Run>>();
    auto message = std::make_shared<string>();
    host.background(
        [fresh, message] {
            const auto add = [&] (Run run) {
                for (const auto& storm : run.storms) {
                    vector<UtilityEnsembleStats::Hour> mean;
                    for (const auto& hour : UtilityEnsembleStats::compute(storm, 6)) {
                        if (hour.alive * 2 < hour.total || hour.centerLat <= UtilityEnsembleStats::missing + 1.0) {
                            break;
                        }
                        mean.push_back(hour);
                    }
                    run.means.push_back(std::move(mean));
                }
                fresh->push_back(std::move(run));
            };
            {
                Run run;
                string e;
                run.label = "GEFS";
                if (HurricaneData::loadGefsAll(run.storms, run.cycle, e)) add(std::move(run)); else *message = e;
            }
            struct Ecmwf { const char * label; const char * model; const char * stream; };
            for (const auto& m : {Ecmwf{"IFS ENS", "ifs", "enfo"}, Ecmwf{"AIFS ENS", "aifs-ens", "enfo"}}) {
                Run run;
                string e;
                run.label = m.label;
                if (HurricaneData::loadEcmwfModel(m.model, m.stream, run.storms, run.cycle, e)) add(std::move(run)); else if (!e.empty()) *message = string{m.label} + ": " + e;
            }
            for (const auto& model : UtilityWeatherLab::models()) {
                Run run;
                string e;
                run.label = model.label;
                if (HurricaneData::loadWeatherLabModel(model.folder, run.storms, run.cycle, e)) add(std::move(run)); else *message = e;
            }
        },
        [this, &host, fresh, message] {
            loading = false;
            runs = fresh;
            error = fresh->empty() ? (message->empty() ? "no ensemble tracks were found" : *message) : string{};
            host.redraw();
        });
}

QColor EnsembleSpaghettiLayer::colorOf(const Run& run, int hour, double wind) const {
    if (colorMode == 1) {
        return QColor::fromHsvF(0.78 * (1.0 - std::min(hour, 240) / 240.0), 0.9, 0.95);
    }
    if (colorMode == 2) {
        return UtilityEcmwfTracks::has(wind) ? categoryColor(static_cast<int>(std::lround(wind))) : QColor{150, 150, 150};
    }
    static const QColor colors[modelCount] = {QColor{70, 150, 255}, QColor{255, 140, 30}, QColor{40, 255, 170}, QColor{214, 110, 255}, QColor{255, 110, 180}};
    const int index = modelIndex(run.label);
    return index >= 0 ? colors[index] : QColor{200, 200, 200};
}

void EnsembleSpaghettiLayer::paint(QPainter& painter, MapHost& host) {
    if (!runs) {
        return;
    }
    const auto t = host.view().transform();
    const double px = host.view().unitsPerPixel();
    QFont font{painter.font()};
    font.setPixelSize(static_cast<int>(11 * px));
    font.setBold(true);
    painter.setFont(font);
    const int limit = limitHours();
    painter.setBrush(Qt::NoBrush);
    if (showMembers) {
        for (const auto& run : *runs) {
            const int index = modelIndex(run.label);
            if (index < 0 || !showModel[index]) {
                continue;
            }
            for (const auto& storm : run.storms) {
                if (!wanted(storm)) {
                    continue;
                }
                for (const auto& m : storm.members) {
                    if (colorMode == 0) {   // one path per member
                        QColor color = colorOf(run, 0, 0.0);
                        color.setAlpha(110);
                        painter.setPen(QPen{color, 1.1 * px});
                        QPainterPath path;
                        bool started = false;
                        double lastLon = 0.0;
                        for (const auto& s : m.steps) {
                            if (s.hour > limit) break;
                            if (!UtilityEcmwfTracks::has(s.lat) || !UtilityEcmwfTracks::has(s.lon)) { started = false; continue; }
                            if (started && std::abs(s.lon - lastLon) > 180.0) started = false;
                            const auto p = t(s.lat, s.lon);
                            started ? path.lineTo(p) : path.moveTo(p);
                            started = true;
                            lastLon = s.lon;
                        }
                        painter.drawPath(path);
                    } else {                // a segment at a time, colored by its hour or its wind
                        const UtilityEcmwfTracks::Step * before = nullptr;
                        for (const auto& s : m.steps) {
                            if (s.hour > limit) break;
                            if (!UtilityEcmwfTracks::has(s.lat) || !UtilityEcmwfTracks::has(s.lon)) { before = nullptr; continue; }
                            if (before != nullptr && std::abs(s.lon - before->lon) < 180.0) {
                                QColor color = colorOf(run, s.hour, s.wind);
                                color.setAlpha(150);
                                painter.setPen(QPen{color, 1.3 * px});
                                painter.drawLine(t(before->lat, before->lon), t(s.lat, s.lon));
                            }
                            before = &s;
                        }
                    }
                    if (showDays) {   // a dot at each whole day of the member
                        for (const auto& s : m.steps) {
                            if (s.hour > limit) break;
                            if (s.hour == 0 || s.hour % 24 != 0 || !UtilityEcmwfTracks::has(s.lat) || !UtilityEcmwfTracks::has(s.lon)) continue;
                            QColor color = colorOf(run, s.hour, s.wind);
                            color.setAlpha(210);
                            painter.setPen(Qt::NoPen);
                            painter.setBrush(color);
                            painter.drawEllipse(t(s.lat, s.lon), 1.7 * px, 1.7 * px);
                        }
                        painter.setBrush(Qt::NoBrush);
                    }
                }
            }
        }
    }
    if (!showMean) {
        return;
    }
    for (const auto& run : *runs) {
        const int index = modelIndex(run.label);
        if (index < 0 || !showModel[index]) {
            continue;
        }
        QColor color = colorOf(run, 0, 0.0);
        if (colorMode != 0) {
            color = QColor{255, 255, 255};
        }
        for (size_t i = 0; i < run.storms.size(); i++) {
            if (!wanted(run.storms[i])) {
                continue;
            }
            const auto& hours = run.means[i];
            if (hours.size() < 2) {
                continue;
            }
            QPainterPath path;
            bool started = false;
            double lastLon = 0.0;
            for (const auto& h : hours) {
                if (h.hour > limit) break;
                if (started && std::abs(h.centerLon - lastLon) > 180.0) started = false;
                const auto p = t(h.centerLat, h.centerLon);
                started ? path.lineTo(p) : path.moveTo(p);
                started = true;
                lastLon = h.centerLon;
            }
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen{QColor{0, 0, 0, 170}, 3.6 * px});
            painter.drawPath(path);
            painter.setPen(QPen{color, 2.0 * px});
            painter.drawPath(path);
            const auto& first = hours.front();
            const auto p = t(first.centerLat, first.centerLon);
            painter.setPen(QPen{QColor{0, 0, 0, 220}, 1.0 * px});
            painter.setBrush(color);
            painter.drawEllipse(p, 4.0 * px, 4.0 * px);
        }
    }
    // one label for each system, from the first model that has it
    std::vector<string> labelled;
    for (const auto& run : *runs) {
        const int index = modelIndex(run.label);
        if (index < 0 || !showModel[index]) {
            continue;
        }
        for (size_t i = 0; i < run.storms.size(); i++) {
            if (!wanted(run.storms[i])) {
                continue;
            }
            const auto& hours = run.means[i];
            const string name = run.storms[i].id;
            if (hours.empty() || std::find(labelled.begin(), labelled.end(), name) != labelled.end()) {
                continue;
            }
            labelled.push_back(name);
            const auto p = t(hours.front().centerLat, hours.front().centerLon);
            painter.setPen(QColor{255, 255, 255});
            MarkText::at(host, painter, px, hours.front().centerLat, hours.front().centerLon, QPointF{7.0, -6.0}, QString::fromStdString(name));
        }
    }
}

MapHit EnsembleSpaghettiLayer::pick(const QPointF& pixels, MapHost& host) const {
    MapHit best;
    best.reach = 12.0;
    best.priority = 5;
    if (!runs) {
        return best;
    }
    const int limit = limitHours();
    const auto consider = [&] (const Run& run, const UtilityEcmwfTracks::Storm& storm, int hour, double lat, double lon, const string& what) {
        const auto p = host.view().toPixels(lat, lon);
        const double d = std::hypot(p.x() - pixels.x(), p.y() - pixels.y());
        if (d >= best.distance) {
            return;
        }
        best.distance = d;
        best.text = QString::fromStdString(run.label + " " + (run.cycle.size() == 10 ? run.cycle.substr(8, 2) + "z" : string{}) + "   system " + storm.id + "\n+" + std::to_string(hour) + " h  " + what);
        // the screen of a storm of the NHC basins: the NHC id when the file has it, else the number and the basin letter of the short id
        string nhc;
        string lower = storm.name;
        std::transform(lower.begin(), lower.end(), lower.begin(), [] (unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (lower.size() == 8 && std::isalpha(static_cast<unsigned char>(lower[0])) && std::isdigit(static_cast<unsigned char>(lower[2]))) {
            nhc = lower;
        } else if (storm.id.size() == 3 && run.cycle.size() == 10) {
            const char letter = storm.id[2];
            const string basin = letter == 'L' ? "al" : letter == 'E' ? "ep" : letter == 'C' ? "cp" : "";
            if (!basin.empty()) {
                nhc = basin + storm.id.substr(0, 2) + run.cycle.substr(0, 4);
            }
        }
        const string basin = nhc.size() >= 2 ? nhc.substr(0, 2) : string{};
        if (basin == "al" || basin == "ep" || basin == "cp") {
            best.open = [basin, nhc] (Window * window) { new HurricaneViewer{window, basin, nhc}; };
        } else {
            best.open = nullptr;
        }
    };
    for (const auto& run : *runs) {
        const int index = modelIndex(run.label);
        if (index < 0 || !showModel[index]) {
            continue;
        }
        for (size_t i = 0; i < run.storms.size(); i++) {
            if (!wanted(run.storms[i])) {
                continue;
            }
            const auto& storm = run.storms[i];
            if (showMembers) {
                for (const auto& m : storm.members) {
                    for (const auto& s : m.steps) {
                        if (s.hour > limit) break;
                        if (!UtilityEcmwfTracks::has(s.lat) || !UtilityEcmwfTracks::has(s.lon)) continue;
                        string what = "member " + std::to_string(m.number);
                        if (UtilityEcmwfTracks::has(s.wind)) what += ", " + std::to_string(std::lround(s.wind)) + " kt";
                        if (UtilityEcmwfTracks::has(s.pressure)) what += ", " + std::to_string(std::lround(s.pressure)) + " mb";
                        consider(run, storm, s.hour, s.lat, s.lon, what);
                    }
                }
            }
            if (showMean) {
                for (const auto& h : run.means[i]) {
                    if (h.hour > limit) break;
                    string what = "mean of " + std::to_string(h.alive) + " of " + std::to_string(h.total) + " members still a cyclone";
                    if (h.windMedian > UtilityEnsembleStats::missing + 1.0) what += ", median " + std::to_string(std::lround(h.windMedian)) + " kt (" + std::to_string(std::lround(h.wind10)) + " to " + std::to_string(std::lround(h.wind90)) + ")";
                    consider(run, storm, h.hour, h.centerLat, h.centerLon, what);
                }
            }
        }
    }
    return best;
}

vector<MapLegendRow> EnsembleSpaghettiLayer::legend() const {
    vector<MapLegendRow> rows;
    MapLegendRow row;
    if (colorMode == 0) {
        row.title = "Ensemble tracks (each line a member):";
        static const char * labels[modelCount] = {"GEFS (NOAA)", "IFS ENS (ECMWF)", "AIFS ENS (ECMWF)", "DeepMind FNV3", "DeepMind WNV3"};
        static const QColor colors[modelCount] = {QColor{70, 150, 255}, QColor{255, 140, 30}, QColor{40, 255, 170}, QColor{214, 110, 255}, QColor{255, 110, 180}};
        for (int i = 0; i < modelCount; i++) {
            if (showModel[i]) row.entries.push_back({MapLegendEntry::Line, colors[i], labels[i]});
        }
    } else if (colorMode == 1) {
        row.title = "Forecast hour:";
        for (const int hour : {0, 48, 96, 144, 192, 240}) {
            row.entries.push_back({MapLegendEntry::Line, QColor::fromHsvF(0.78 * (1.0 - hour / 240.0), 0.9, 0.95), QString::number(hour) + " h"});
        }
    } else {
        row.title = "Wind of the member (kt):";
        for (const auto& [wind, label] : {std::pair<int, const char *>{20, "<34"}, {40, "34-63"}, {70, "64-82"}, {90, "83-95"}, {100, "96-112"}, {120, "113-136"}, {150, "137+"}}) {
            row.entries.push_back({MapLegendEntry::Line, categoryColor(wind), label});
        }
    }
    if (showMean) {
        row.entries.push_back({MapLegendEntry::Line, QColor{255, 255, 255}, "mean (thick)"});
    }
    rows.push_back(row);
    if (showModel[3] || showModel[4]) {
        MapLegendRow credit;
        credit.title = "(c) 2024-6 Google LLC, DeepMind Weather Lab (experimental; not for real world use)";
        rows.push_back(credit);
    }
    return rows;
}

QWidget * EnsembleSpaghettiLayer::options(QWidget * parent, const std::function<void()>& changed) {
    auto * widget = new QWidget{parent};
    auto * layout = new QVBoxLayout{widget};
    layout->setContentsMargins(0, 0, 0, 0);
    const auto check = [&] (const char * label, bool& value) {
        auto * c = new QCheckBox{label, widget};
        c->setChecked(value);
        QObject::connect(c, &QCheckBox::toggled, [&value, changed] (bool on) { value = on; changed(); });
        layout->addWidget(c);
    };
    check("GEFS (31 members, NHC's systems)", showModel[0]);
    check("ECMWF IFS ENS (51)", showModel[1]);
    check("ECMWF AIFS ENS (51)", showModel[2]);
    check("DeepMind FNV3 (50)", showModel[3]);
    check("DeepMind WeatherNext 3 (64, experimental)", showModel[4]);
    check("Each member's track", showMembers);
    check("A dot for each day of a member", showDays);
    check("Ensemble mean", showMean);
    layout->addWidget(new QLabel{"Colored by:", widget});
    auto * colors = UiStandards::combo(widget);
    colors->addItems({"Model", "Forecast hour", "Wind of the member"});
    colors->setCurrentIndex(colorMode);
    QObject::connect(colors, QOverload<int>::of(&QComboBox::currentIndexChanged), [this, changed] (int index) { colorMode = index; changed(); });
    layout->addWidget(colors);
    layout->addWidget(new QLabel{"Forecast shown:", widget});
    auto * combo = UiStandards::combo(widget);
    combo->addItems({"All of it", "10 days", "7 days", "5 days", "3 days", "2 days", "1 day"});
    combo->setCurrentIndex(hoursChoice);
    QObject::connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged), [this, changed] (int index) { hoursChoice = index; changed(); });
    layout->addWidget(combo);
    auto * only = UiStandards::button(widget, "Show only this layer");
    QObject::connect(only, &QPushButton::clicked, [this] {
        if (hostPointer != nullptr) {
            hostPointer->onlyLayer(id());
        }
    });
    layout->addWidget(only);
    return widget;
}

// ---- DeepMind: where new storms may form ----

namespace {
    const double chanceSteps[] = {0.01, 0.02, 0.05, 0.10, 0.25};
    const int windowHours[] = {10000, 168, 120, 72};
}

QColor DeepMindGenesisLayer::colorFor(double chance) {
    if (chance >= 0.50) return QColor{142, 36, 170};
    if (chance >= 0.25) return QColor{229, 57, 53};
    if (chance >= 0.10) return QColor{255, 112, 67};
    if (chance >= 0.05) return QColor{255, 179, 71};
    return QColor{255, 224, 102};
}

vector<DeepMindGenesisLayer::Cluster> DeepMindGenesisLayer::clusters() const {
    vector<Cluster> out;
    if (!data || members <= 0) {
        return out;
    }
    const int window = windowHours[std::clamp(windowChoice, 0, 3)];
    for (const auto& g : *data) {
        Cluster c;
        c.track = g.track;
        for (const auto& point : g.points) {
            if (point.hour <= window) {
                c.points.push_back(point);
            }
        }
        c.chance = static_cast<double>(c.points.size()) / members;
        if (c.chance < chanceSteps[std::clamp(chanceChoice, 0, 4)] || c.points.empty()) {
            continue;
        }
        // the middle of the members' formation points (longitudes taken near the first so that a cluster across the date line is not averaged across the world)
        std::vector<double> lats, lons, hours, pressures, winds;
        const double reference = c.points.front().lon;
        for (const auto& point : c.points) {
            double lon = point.lon;
            while (lon - reference > 180.0) lon -= 360.0;
            while (lon - reference < -180.0) lon += 360.0;
            lats.push_back(point.lat);
            lons.push_back(lon);
            hours.push_back(point.hour);
            if (point.pressure > -9000.0) pressures.push_back(point.pressure);
            if (point.wind > -9000.0) winds.push_back(point.wind);
        }
        c.lat = UtilityEnsembleStats::percentile(lats, 0.5);
        c.lon = UtilityEnsembleStats::percentile(lons, 0.5);
        if (c.lon > 180.0) c.lon -= 360.0;
        if (c.lon < -180.0) c.lon += 360.0;
        c.hourMedian = UtilityEnsembleStats::percentile(hours, 0.5);
        c.pressureMedian = UtilityEnsembleStats::percentile(pressures, 0.5);
        c.windMedian = UtilityEnsembleStats::percentile(winds, 0.5);
        out.push_back(std::move(c));
    }
    return out;
}

string DeepMindGenesisLayer::summary() const {
    if (loading && !data) return "reading DeepMind's large ensemble (37 MB)...";
    if (!data) return error;
    return std::to_string(clusters().size()) + " possible new storms (" + UtilityAtcf::formatTime(cycle) + ", " + std::to_string(members) + " members)";
}

void DeepMindGenesisLayer::refresh(MapHost& host) {
    if (loading) {
        return;
    }
    loading = true;
    auto fresh = std::make_shared<vector<UtilityWeatherLab::Genesis>>();
    auto count = std::make_shared<int>(0);
    auto when = std::make_shared<string>();
    auto message = std::make_shared<string>();
    host.background(
        [fresh, count, when, message] {
            if (!HurricaneData::loadWeatherLabGenesis(*fresh, *count, *when, *message)) {
                fresh->clear();
            }
        },
        [this, &host, fresh, count, when, message] {
            loading = false;
            if (!fresh->empty() || message->empty()) {
                data = fresh;
                members = *count;
                cycle = *when;
                error = fresh->empty() ? string{"no new storms are forecast"} : string{};
            } else {
                error = *message;
            }
            host.redraw();
        });
}

void DeepMindGenesisLayer::paint(QPainter& painter, MapHost& host) {
    if (!data) {
        return;
    }
    const auto t = host.view().transform();
    const double px = host.view().unitsPerPixel();
    QFont font{painter.font()};
    font.setPixelSize(static_cast<int>(12 * px));
    font.setBold(true);
    painter.setFont(font);
    const auto all = clusters();
    for (const auto& c : all) {   // the members' points, faint, then the labels over them
        QColor dot = colorFor(c.chance);
        dot.setAlpha(80);
        painter.setPen(Qt::NoPen);
        painter.setBrush(dot);
        for (const auto& point : c.points) {
            painter.drawEllipse(t(point.lat, point.lon), 2.4 * px, 2.4 * px);
        }
    }
    for (const auto& c : all) {
        const auto p = t(c.lat, c.lon);
        const double radius = (5.0 + 14.0 * std::sqrt(std::min(1.0, c.chance))) * px;
        painter.setPen(QPen{QColor{0, 0, 0, 200}, 1.4 * px});
        QColor ring = colorFor(c.chance);
        ring.setAlpha(60);
        painter.setBrush(ring);
        painter.drawEllipse(p, radius, radius);
        painter.setPen(QColor{255, 255, 255});
        MarkText::at(host, painter, px, c.lat, c.lon, QPointF{radius / px + 3.0, 4.0}, QString::number(static_cast<int>(std::lround(c.chance * 100.0))) + "%");
    }
}

MapHit DeepMindGenesisLayer::pick(const QPointF& pixels, MapHost& host) const {
    MapHit best;
    best.reach = 24.0;
    best.priority = 4;
    for (const auto& c : clusters()) {
        const auto p = host.view().toPixels(c.lat, c.lon);
        const double d = std::hypot(p.x() - pixels.x(), p.y() - pixels.y());
        if (d >= best.distance) {
            continue;
        }
        best.distance = d;
        const auto init = QDateTime::fromString(QString::fromStdString(cycle), "yyyyMMddHH");
        const auto valid = QDateTime{init.date(), init.time(), Qt::UTC}.addSecs(static_cast<qint64>(std::lround(c.hourMedian)) * 3600);
        QString text = "A storm that does not exist yet (DeepMind 1000-member ensemble, " + QString::fromStdString(cycle.size() == 10 ? cycle.substr(8, 2) : string{}) + "z run)\\n" +
            QString::number(c.points.size()) + " of " + QString::number(members) + " members (" + QString::number(c.chance * 100.0, 'f', c.chance < 0.1 ? 1 : 0) + "%) form it\\n" +
            "typically " + valid.toString("yyyy-MM-dd HH") + "Z (+" + QString::number(std::lround(c.hourMedian)) + " h) near " + QString::number(std::abs(c.lat), 'f', 1) + (c.lat < 0 ? "S " : "N ") +
            QString::number(std::abs(c.lon), 'f', 1) + (c.lon < 0 ? "W" : "E");
        if (c.pressureMedian > -9000.0) text += "\nwhen it first appears: " + QString::number(std::lround(c.pressureMedian)) + " mb" + (c.windMedian > -9000.0 ? ", " + QString::number(std::lround(c.windMedian)) + " kt" : QString{});
        text += "\nexperimental data, not for real world use";
        best.text = text;
    }
    return best;
}

vector<MapLegendRow> DeepMindGenesisLayer::legend() const {
    MapLegendRow credit;
    credit.title = "(c) 2024-6 Google LLC, DeepMind Weather Lab (experimental; not for real world use):";
    credit.entries = {{MapLegendEntry::Circle, colorFor(0.02), "1-5% of members form a storm"}, {MapLegendEntry::Circle, colorFor(0.07), "5-10%"}, {MapLegendEntry::Circle, colorFor(0.15), "10-25%"},
                      {MapLegendEntry::Circle, colorFor(0.35), "25-50%"}, {MapLegendEntry::Circle, colorFor(0.6), "over 50%"}};
    return {credit};
}

QWidget * DeepMindGenesisLayer::options(QWidget * parent, const std::function<void()>& changed) {
    auto * widget = new QWidget{parent};
    auto * layout = new QVBoxLayout{widget};
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(new QLabel{"Show storms that at least this share of the members form:", widget});
    auto * chance = UiStandards::combo(widget);
    chance->addItems({"1%", "2%", "5%", "10%", "25%"});
    chance->setCurrentIndex(chanceChoice);
    QObject::connect(chance, QOverload<int>::of(&QComboBox::currentIndexChanged), [this, changed] (int index) { chanceChoice = index; changed(); });
    layout->addWidget(chance);
    layout->addWidget(new QLabel{"Forming within:", widget});
    auto * window = UiStandards::combo(widget);
    window->addItems({"All of the forecast (about 13 days)", "7 days", "5 days", "3 days"});
    window->setCurrentIndex(windowChoice);
    QObject::connect(window, QOverload<int>::of(&QComboBox::currentIndexChanged), [this, changed] (int index) { windowChoice = index; changed(); });
    layout->addWidget(window);
    auto * only = UiStandards::button(widget, "Show only this layer");
    QObject::connect(only, &QPushButton::clicked, [this] {
        if (hostPointer != nullptr) {
            hostPointer->onlyLayer(id());
        }
    });
    layout->addWidget(only);
    return widget;
}
