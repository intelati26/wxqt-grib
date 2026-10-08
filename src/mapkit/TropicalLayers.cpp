// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "mapkit/TropicalLayers.h"
#include <algorithm>
#include <cmath>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QPainterPath>
#include <QVBoxLayout>
#include "hurricane/HurricaneViewer.h"
#include "hurricane/UtilityAtcf.h"
#include "hurricane/UtilityNhcGis.h"
#include "hurricane/UtilityTropicalAlerts.h"
#include "hurricane/UtilityWindProbability.h"

namespace {
    QColor categoryColor(int wind) {
        static const QColor colors[] = {QColor{94, 186, 255}, QColor{0, 235, 230}, QColor{255, 255, 204}, QColor{255, 231, 117},
                                        QColor{255, 193, 64}, QColor{255, 143, 32}, QColor{255, 96, 96}};
        return colors[std::clamp(wind >= 0 ? UtilityAtcf::categoryOf(wind) : 0, 0, 6)];
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
                    painter.drawText(p + QPointF{7.0 * px, -5.0 * px}, QString::number(f.tau / 24) + " d");
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
        painter.drawText(now + QPointF{11.0 * px, 4.0 * px}, QString::fromStdString(HurricaneData::idLabel(s.entry.id) + (s.entry.name.empty() ? "" : " " + s.entry.name)));
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
        painter.drawText(t(area.centerLat, area.centerLon) + QPointF{-30 * px, 4 * px}, QString::number(area.prob2) + "% / " + QString::number(area.prob7) + "%");
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
    auto * combo = new QComboBox{widget};
    combo->addItems({"34 kt (tropical storm)", "50 kt", "64 kt (hurricane)"});
    combo->setCurrentIndex(threshold);
    QObject::connect(combo, &QComboBox::currentIndexChanged, [this, changed] (int index) { threshold = index; changed(); });
    layout->addWidget(combo);
    return widget;
}
