// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

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
#include "hurricane/UtilityWeatherLab.h"
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

// ---- DeepMind Weather Lab ----

int DeepMindLayer::limitHours() const {
    static const int hours[] = {10000, 120, 72, 48};
    return hours[std::clamp(hoursChoice, 0, 3)];
}

string DeepMindLayer::summary() const {
    if (loading && !runs) return "reading DeepMind's forecasts...";
    if (!runs) return error;
    string text;
    for (const auto& run : *runs) {
        text += (text.empty() ? "" : ", ") + run.label + " " + UtilityAtcf::formatTime(run.cycle) + ": " + std::to_string(run.storms.size()) + " storms";
    }
    return text.empty() ? error : text;
}

void DeepMindLayer::refresh(MapHost& host) {
    if (loading) {
        return;
    }
    loading = true;
    auto fresh = std::make_shared<vector<Run>>();
    auto message = std::make_shared<string>();
    host.background(
        [fresh, message] {
            for (const auto& model : UtilityWeatherLab::models()) {
                Run run;
                string error;
                if (!HurricaneData::loadWeatherLabModel(model.folder, run.storms, run.cycle, error)) {
                    *message = error;
                    continue;
                }
                run.label = model.label;
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
            }
        },
        [this, &host, fresh, message] {
            loading = false;
            runs = fresh;
            error = fresh->empty() ? (message->empty() ? "no DeepMind forecast was found" : *message) : string{};
            host.redraw();
        });
}

void DeepMindLayer::paint(QPainter& painter, MapHost& host) {
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
    // a line is broken where it jumps across the date line, and where a member has no position
    const auto drawTrack = [&] (const UtilityEcmwfTracks::Member& member, const QPen& pen) {
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);
        QPainterPath path;
        bool started = false;
        double lastLon = 0.0;
        for (const auto& s : member.steps) {
            if (s.hour > limit) {
                break;
            }
            if (!UtilityEcmwfTracks::has(s.lat) || !UtilityEcmwfTracks::has(s.lon)) {
                started = false;
                continue;
            }
            if (started && std::abs(s.lon - lastLon) > 180.0) {
                started = false;
            }
            const auto p = t(s.lat, s.lon);
            started ? path.lineTo(p) : path.moveTo(p);
            started = true;
            lastLon = s.lon;
        }
        painter.drawPath(path);
    };
    for (const auto& run : *runs) {
        const bool wanted = run.label == "DeepMind FNV3" ? showFnv : showWeatherNext;
        const auto * style = EnsembleStyle::of(run.label);
        if (!wanted || style == nullptr) {
            continue;
        }
        if (showMembers) {
            QColor member = style->member;
            member.setAlpha(60);   // fifty or sixty members from each storm: thin and faint
            for (const auto& storm : run.storms) {
                for (const auto& m : storm.members) {
                    drawTrack(m, QPen{member, 1.0 * px});
                }
            }
        }
    }
    for (const auto& run : *runs) {
        const bool wanted = run.label == "DeepMind FNV3" ? showFnv : showWeatherNext;
        const auto * style = EnsembleStyle::of(run.label);
        if (!wanted || style == nullptr || !showMean) {
            continue;
        }
        for (size_t i = 0; i < run.storms.size(); i++) {
            const auto& hours = run.means[i];
            if (hours.size() < 2) {
                continue;
            }
            QPainterPath path;
            bool started = false;
            double lastLon = 0.0;
            for (const auto& h : hours) {
                if (h.hour > limit) {
                    break;
                }
                if (started && std::abs(h.centerLon - lastLon) > 180.0) {
                    started = false;
                }
                const auto p = t(h.centerLat, h.centerLon);
                started ? path.lineTo(p) : path.moveTo(p);
                started = true;
                lastLon = h.centerLon;
            }
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen{QColor{0, 0, 0, 170}, 4.4 * px});
            painter.drawPath(path);
            painter.setPen(QPen{style->mean, 2.4 * px});
            painter.drawPath(path);
            for (const auto& h : hours) {   // a dot a day, and where the storm is now with its short id
                if (h.hour > limit) {
                    break;
                }
                const auto p = t(h.centerLat, h.centerLon);
                if (h.hour == 0) {
                    painter.setPen(QPen{QColor{0, 0, 0, 220}, 1.0 * px});
                    painter.setBrush(style->mean);
                    painter.drawEllipse(p, 5.0 * px, 5.0 * px);
                    painter.setPen(QColor{255, 255, 255});
                    painter.drawText(p + QPointF{7 * px, -6 * px}, QString::fromStdString(run.storms[i].id));
                } else if (h.hour % 24 == 0) {
                    painter.setPen(QPen{QColor{0, 0, 0, 200}, 0.9 * px});
                    painter.setBrush(style->mean);
                    painter.drawEllipse(p, 3.2 * px, 3.2 * px);
                }
            }
        }
    }
}

MapHit DeepMindLayer::pick(const QPointF& pixels, MapHost& host) const {
    MapHit best;
    best.reach = 14.0;
    best.priority = 5;
    if (!runs || !showMean) {
        return best;
    }
    const auto t = host.view().transform();
    const int limit = limitHours();
    for (const auto& run : *runs) {
        const bool wanted = run.label == "DeepMind FNV3" ? showFnv : showWeatherNext;
        if (!wanted) {
            continue;
        }
        for (size_t i = 0; i < run.storms.size(); i++) {
            for (const auto& h : run.means[i]) {
                if (h.hour > limit) {
                    break;
                }
                const auto p = host.view().toPixels(h.centerLat, h.centerLon);
                const double d = std::hypot(p.x() - pixels.x(), p.y() - pixels.y());
                if (d >= best.distance) {
                    continue;
                }
                best.distance = d;
                QString text = QString::fromStdString(run.label + " " + (run.cycle.size() == 10 ? run.cycle.substr(8, 2) + "z" : string{}) + "   storm " + run.storms[i].id + "\n+" + std::to_string(h.hour) + " h: ");
                text += QString::number(h.alive) + " of " + QString::number(h.total) + " members still a cyclone";
                if (h.windMedian > UtilityEnsembleStats::missing + 1.0) text += "\nwind (median) " + QString::number(std::lround(h.windMedian)) + " kt, range " + QString::number(std::lround(h.wind10)) + " to " + QString::number(std::lround(h.wind90)) + " kt";
                if (h.pressMedian > UtilityEnsembleStats::missing + 1.0) text += "\npressure (median) " + QString::number(std::lround(h.pressMedian)) + " mb";
                text += "\nexperimental data, not for real world use";
                best.text = text;
                const string track = run.storms[i].name;
                const string basin = track.size() >= 2 ? string{static_cast<char>(std::tolower(static_cast<unsigned char>(track[0]))), static_cast<char>(std::tolower(static_cast<unsigned char>(track[1])))} : string{};
                if (basin == "al" || basin == "ep" || basin == "cp") {
                    string lower = track;
                    std::transform(lower.begin(), lower.end(), lower.begin(), [] (unsigned char c) { return static_cast<char>(std::tolower(c)); });
                    best.open = [basin, lower] (Window * window) { new HurricaneViewer{window, basin, lower}; };
                }
            }
        }
    }
    return best;
}

vector<MapLegendRow> DeepMindLayer::legend() const {
    MapLegendRow row;
    row.title = "(c) 2024-6 Google LLC, DeepMind Weather Lab (experimental; not for real world use; terms: storage.googleapis.com/weathernext-public/terms-of-use.pdf):";
    for (const auto& model : UtilityWeatherLab::models()) {
        const bool wanted = string{model.label} == "DeepMind FNV3" ? showFnv : showWeatherNext;
        const auto * style = EnsembleStyle::of(model.label);
        if (!wanted || style == nullptr) {
            continue;
        }
        if (showMean) {
            row.entries.push_back({MapLegendEntry::Line, style->mean, QString::fromStdString(string{model.label} + " mean")});
        }
        if (showMembers) {
            row.entries.push_back({MapLegendEntry::Line, QColor{style->member.red(), style->member.green(), style->member.blue()}, QString::fromStdString(string{model.label} + " members")});
        }
    }
    return {row};
}

QWidget * DeepMindLayer::options(QWidget * parent, const std::function<void()>& changed) {
    auto * widget = new QWidget{parent};
    auto * layout = new QVBoxLayout{widget};
    layout->setContentsMargins(0, 0, 0, 0);
    const auto check = [&] (const char * label, bool& value) {
        auto * c = new QCheckBox{label, widget};
        c->setChecked(value);
        QObject::connect(c, &QCheckBox::toggled, [&value, changed] (bool on) { value = on; changed(); });
        layout->addWidget(c);
    };
    check("FNV3 (50 members)", showFnv);
    check("WeatherNext 3 (64 members, experimental)", showWeatherNext);
    check("Each member's track", showMembers);
    check("Ensemble mean", showMean);
    layout->addWidget(new QLabel{"Forecast shown:", widget});
    auto * combo = new QComboBox{widget};
    combo->addItems({"All of it (about 13 days)", "5 days", "3 days", "2 days"});
    combo->setCurrentIndex(hoursChoice);
    QObject::connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged), [this, changed] (int index) { hoursChoice = index; changed(); });
    layout->addWidget(combo);
    // the map with nothing else on it
    auto * only = new QPushButton{"Show only this layer", widget};
    only->setToolTip("Switch every other layer off, so the map shows DeepMind's forecasts alone");
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
        painter.drawText(p + QPointF{radius + 3 * px, 4 * px}, QString::number(static_cast<int>(std::lround(c.chance * 100.0))) + "%");
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
    auto * chance = new QComboBox{widget};
    chance->addItems({"1%", "2%", "5%", "10%", "25%"});
    chance->setCurrentIndex(chanceChoice);
    QObject::connect(chance, QOverload<int>::of(&QComboBox::currentIndexChanged), [this, changed] (int index) { chanceChoice = index; changed(); });
    layout->addWidget(chance);
    layout->addWidget(new QLabel{"Forming within:", widget});
    auto * window = new QComboBox{widget};
    window->addItems({"All of the forecast (about 13 days)", "7 days", "5 days", "3 days"});
    window->setCurrentIndex(windowChoice);
    QObject::connect(window, QOverload<int>::of(&QComboBox::currentIndexChanged), [this, changed] (int index) { windowChoice = index; changed(); });
    layout->addWidget(window);
    auto * only = new QPushButton{"Show only this layer", widget};
    QObject::connect(only, &QPushButton::clicked, [this] {
        if (hostPointer != nullptr) {
            hostPointer->onlyLayer(id());
        }
    });
    layout->addWidget(only);
    return widget;
}
