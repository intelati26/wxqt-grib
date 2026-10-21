// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "mapkit/SatellitePassLayer.h"
#include <array>
#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QTimeZone>
#include <QVBoxLayout>
#include "radar/MapView.h"
#include "util/Utility.h"

namespace {
    QColor colorOf(SatellitePasses::Kind kind) {
        return kind == SatellitePasses::Kind::Imagery ? QColor{60, 140, 235} : kind == SatellitePasses::Kind::Microwave ? QColor{170, 90, 225} : kind == SatellitePasses::Kind::Wind ? QColor{50, 190, 100} : QColor{240, 150, 40};
    }
}

string SatellitePassLayer::summary() const {
    if (loading && !data) return "working out the passes...";
    if (!data) return {};
    return std::to_string(data->bands.size()) + (onlyOver ? " passes over the storms and places in the next " : " swaths in the next ") + std::to_string(hours) + " hours" + (data->problem.empty() ? string{} : " (" + data->problem + ")");
}

void SatellitePassLayer::refresh(MapHost& host) {
    if (loading) {
        return;
    }
    loading = true;
    hours = std::clamp(Utility::readPrefInt("MM_SATPASS_HOURS", 6), 1, 24);
    bool wanted[4];
    for (int i = 0; i < 4; i++) {
        wanted[i] = kinds[i];
    }
    const int ahead = hours;
    const bool only = onlyOver;
    auto fresh = std::make_shared<Data>();
    host.background(
        [fresh, ahead, wanted, only] {
            const long long from = QDateTime::currentSecsSinceEpoch();
            fresh->places = SatellitePasses::places();
            auto storms = SatellitePasses::stormPlaces();
            fresh->places.insert(fresh->places.end(), storms.begin(), storms.end());
            // one band of an instrument: the swath along the ground track between two times
            const auto makeBand = [] (const SatelliteOrbit::Propagator& propagator, const SatellitePasses::Satellite& sat, const SatellitePasses::Instrument& instrument, long long start, long long end, bool timeLabels, const QString& labelAtMiddle) {
                Band band;
                band.satellite = sat.name;
                band.instrument = instrument.label;
                band.kind = instrument.kind;
                const auto track = SatelliteOrbit::track(propagator, start, static_cast<double>(end - start), 30.0);
                struct Edge {
                    double from, to;
                };
                std::vector<Edge> strips;   // the outer swath, or the two with a gap under the satellite; a radar looking right has the right side only
                if (instrument.innerKm > 0.0) {
                    if (instrument.side != 1) strips.push_back({-instrument.halfWidthKm, -instrument.innerKm});
                    strips.push_back({instrument.innerKm, instrument.halfWidthKm});
                } else {
                    strips.push_back({instrument.side == 1 ? 0.0 : -instrument.halfWidthKm, instrument.halfWidthKm});
                }
                for (const auto& strip : strips) {
                    double previous[2][2]{};
                    bool have = false;
                    double lastLon = 0.0;
                    for (const auto& point : track) {
                        double aLat, aLon, bLat, bLon;
                        SatelliteOrbit::swathEdge(propagator, point.time, strip.from, aLat, aLon);
                        SatelliteOrbit::swathEdge(propagator, point.time, strip.to, bLat, bLon);
                        if (have && std::fabs(aLon - previous[0][1]) < 100.0 && std::fabs(bLon - previous[1][1]) < 100.0 && std::fabs(point.lon - lastLon) < 100.0) {
                            band.quads.push_back({QPointF{previous[0][1], previous[0][0]}, QPointF{previous[1][1], previous[1][0]}, QPointF{bLon, bLat}, QPointF{aLon, aLat}});
                        }
                        previous[0][0] = aLat; previous[0][1] = aLon; previous[1][0] = bLat; previous[1][1] = bLon;
                        have = true;
                        lastLon = point.lon;
                    }
                }
                for (size_t i = 0; i < track.size(); i++) {
                    band.track.push_back({track[i].lon, track[i].lat});
                    if (timeLabels && track[i].time % 1800 < 30) {
                        band.labels.emplace_back(QPointF{track[i].lon, track[i].lat}, QString::fromStdString(sat.name) + " " + QDateTime::fromSecsSinceEpoch(track[i].time, QTimeZone::utc()).toString("HH:mm") + "Z");
                    }
                }
                if (!labelAtMiddle.isEmpty() && !track.empty()) {
                    const auto& m = track[track.size() / 2];
                    band.labels.emplace_back(QPointF{m.lon, m.lat}, labelAtMiddle);
                }
                return band;
            };
            for (const auto& sat : SatellitePasses::satellites()) {
                string problem;
                const auto tle = SatellitePasses::elements(sat.catalog, problem);
                if (!tle.valid) {
                    fresh->problem += (fresh->problem.empty() ? "" : "; ") + sat.name + " " + problem;
                    continue;
                }
                const SatelliteOrbit::Propagator propagator{tle};
                for (const auto& instrument : sat.instruments) {
                    if (!wanted[static_cast<int>(instrument.kind)]) {
                        continue;
                    }
                    if (!only) {
                        fresh->bands.push_back(makeBand(propagator, sat, instrument, from, from + ahead * 3600, true, {}));
                        continue;
                    }
                    // the passes over the storms and the places: the footprint 10 minutes either side of the closest approach, named with the time and the places it covers
                    std::map<long long, QStringList> byTime;
                    for (const auto& place : fresh->places) {
                        for (const auto& pass : SatelliteOrbit::passes(propagator, place.lat, place.lon, from, ahead, {instrument.label, instrument.halfWidthKm, instrument.innerKm})) {
                            if (instrument.side == 1 && pass.distanceKm < 0.0) {
                                continue;
                            }
                            byTime[pass.time / 120 * 120] << QString::fromStdString(place.name);
                        }
                    }
                    for (const auto& [time, names] : byTime) {
                        QStringList unique = names;
                        unique.removeDuplicates();
                        auto band = makeBand(propagator, sat, instrument, time - 600, time + 600, false, {});
                        band.when = time;
                        band.text = QString::fromStdString(sat.name + " " + instrument.label) + " " + QDateTime::fromSecsSinceEpoch(time, QTimeZone::utc()).toString("ddd HH:mm") + "Z   " + unique.join(", ");
                        fresh->bands.push_back(std::move(band));
                    }
                }
            }
        },
        [this, &host, fresh] {
            loading = false;
            data = fresh;
            host.redraw();
        });
}

void SatellitePassLayer::paint(QPainter& painter, MapHost& host) {
    if (!data) {
        return;
    }
    const auto t = host.view().transform();
    const double px = host.view().unitsPerPixel();
    painter.save();
    // the swaths, one colour for a kind, the quads of an instrument joined so that where they meet they do not double the colour
    for (const auto& band : data->bands) {
        QPainterPath path;
        path.setFillRule(Qt::WindingFill);
        for (const auto& q : band.quads) {
            QPolygonF polygon;
            for (const auto& p : q) {
                polygon << t(p.y(), p.x());
            }
            path.addPolygon(polygon);
            path.closeSubpath();
        }
        auto color = colorOf(band.kind);
        color.setAlpha(onlyOver ? 42 : 30);
        painter.setPen(QPen{QColor{color.red(), color.green(), color.blue(), 200}, 1.0 * px});
        painter.setBrush(color);
        painter.drawPath(path.simplified());
    }
    // the ground tracks and the times
    QFont font{painter.font()};
    font.setPixelSize(static_cast<int>(10 * px * 1.0));
    font.setBold(true);
    painter.setFont(font);
    std::set<string> tracked;
    for (const auto& band : data->bands) {
        if (!tracked.insert(band.satellite).second) {
            continue;
        }
        QPainterPath line;
        QPointF last;
        bool open = false;
        for (const auto& p : band.track) {
            const auto at = t(p.y(), p.x());
            if (open && std::fabs(p.x() - last.x()) < 100.0) {
                line.lineTo(at);
            } else {
                line.moveTo(at);
            }
            last = p;
            open = true;
        }
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen{QColor{20, 20, 40, 150}, 1.0 * px, Qt::DashLine});
        painter.drawPath(line);
        for (const auto& [where, text] : band.labels) {
            const auto at = t(where.y(), where.x());
            painter.setPen(QPen{QColor{20, 20, 40}, 1.0 * px});
            painter.setBrush(QColor{255, 255, 255, 200});
            painter.drawEllipse(at, 2.5 * px, 2.5 * px);
            const QRectF box{at.x() + 5.0 * px, at.y() - 7.0 * px, (text.size() * 6.2 + 6.0) * px, 14.0 * px};
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor{255, 255, 255, 190});
            painter.drawRoundedRect(box, 3.0 * px, 3.0 * px);
            painter.setPen(QColor{20, 20, 40});
            painter.drawText(box, Qt::AlignCenter, text);
        }
    }
    if (onlyOver) {   // the list of the passes in the corner, soonest first
        std::vector<std::pair<long long, std::pair<SatellitePasses::Kind, QString>>> list;
        for (const auto& band : data->bands) {
            if (band.when > 0) {
                list.push_back({band.when, {band.kind, band.text}});
            }
        }
        std::sort(list.begin(), list.end(), [] (const auto& a, const auto& b) { return a.first < b.first; });
        if (!list.empty()) {
            QFont listFont{painter.font()};
            listFont.setPixelSize(static_cast<int>(11 * px));
            painter.setFont(listFont);
            const int shown = static_cast<int>(std::min<size_t>(list.size(), 14));
            double width = 0.0;
            for (int i = 0; i < shown; i++) {
                width = std::max(width, painter.fontMetrics().horizontalAdvance(list[static_cast<size_t>(i)].second.second) + 26.0 * px);
            }
            const QRectF box{-500.0 + 8.0 * px, -250.0 + 8.0 * px, width + 12.0 * px, shown * 15.0 * px + 10.0 * px + (list.size() > 14 ? 15.0 * px : 0.0)};
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor{255, 255, 255, 215});
            painter.drawRoundedRect(box, 4.0 * px, 4.0 * px);
            for (int i = 0; i < shown; i++) {
                const double y = box.top() + 5.0 * px + i * 15.0 * px;
                painter.setBrush(colorOf(list[static_cast<size_t>(i)].second.first));
                painter.drawRect(QRectF{box.left() + 6.0 * px, y + 3.0 * px, 9.0 * px, 9.0 * px});
                painter.setPen(QColor{20, 20, 40});
                painter.drawText(QPointF{box.left() + 20.0 * px, y + 12.0 * px}, list[static_cast<size_t>(i)].second.second);
                painter.setPen(Qt::NoPen);
            }
            if (list.size() > 14) {
                painter.setPen(QColor{80, 80, 90});
                painter.drawText(QPointF{box.left() + 20.0 * px, box.bottom() - 5.0 * px}, QString{"... and %1 more (the schedule window has them all)"}.arg(list.size() - 14));
            }
        }
    }
    // the places of interest
    for (const auto& place : data->places) {
        const auto at = t(place.lat, place.lon);
        painter.setPen(QPen{QColor{20, 20, 30}, 1.2 * px});
        painter.setBrush(QColor{255, 220, 60});
        const double r = 6.0 * px;
        painter.drawPolygon(QPolygonF{{at + QPointF{0, -r}, at + QPointF{r, 0}, at + QPointF{0, r}, at + QPointF{-r, 0}}});
        painter.setPen(QColor{255, 255, 255});
        painter.drawText(at + QPointF{9.0 * px, 4.0 * px}, QString::fromStdString(place.name));
    }
    painter.restore();
}

MapHit SatellitePassLayer::pick(const QPointF& pixels, MapHost& host) const {
    MapHit hit;
    hit.reach = 12.0;
    hit.priority = 5;
    if (!data) {
        return hit;
    }
    auto& view = host.view();
    for (const auto& band : data->bands) {
        if (band.when > 0) {   // a pass: its text where the pointer is over its footprint's middle
            const auto& m = band.track[band.track.size() / 2];
            const auto at = view.toPixels(m.y(), m.x());
            const double distance = std::hypot(at.x() - pixels.x(), at.y() - pixels.y());
            if (distance < hit.distance + 40.0) {
                hit.reach = 60.0;
                hit.distance = distance;
                hit.text = band.text;
            }
        }
        for (const auto& [where, text] : band.labels) {
            const auto at = view.toPixels(where.y(), where.x());
            const double distance = std::hypot(at.x() - pixels.x(), at.y() - pixels.y());
            if (distance < hit.distance) {
                hit.distance = distance;
                hit.text = text + "  (" + QString::fromStdString(band.instrument) + ")";
            }
        }
    }
    return hit;
}

vector<MapLegendRow> SatellitePassLayer::legend() const {
    MapLegendRow row;
    row.title = "Satellite swaths:";
    for (const auto kind : {SatellitePasses::Kind::Imagery, SatellitePasses::Kind::Microwave, SatellitePasses::Kind::Wind, SatellitePasses::Kind::Radar}) {
        row.entries.push_back({MapLegendEntry::Square, colorOf(kind), QString::fromStdString(SatellitePasses::kindName(kind))});
    }
    row.entries.push_back({MapLegendEntry::Diamond, QColor{255, 220, 60}, "place of interest"});
    return {row};
}

QWidget * SatellitePassLayer::options(QWidget * parent, const std::function<void()>& changed) {
    auto * widget = new QWidget{parent};
    auto * layout = new QVBoxLayout{widget};
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(new QLabel{"The next:", widget});
    auto * hoursBox = new QComboBox{widget};
    int current = 1;
    int index = 0;
    for (const int h : {3, 6, 12, 24}) {
        hoursBox->addItem(QString::number(h) + " hours", h);
        if (h == hours) current = index;
        index++;
    }
    hoursBox->setCurrentIndex(current);
    layout->addWidget(hoursBox);
    QObject::connect(hoursBox, &QComboBox::currentIndexChanged, [hoursBox, changed] (int) {
        Utility::writePrefInt("MM_SATPASS_HOURS", hoursBox->currentData().toInt());
        changed();
    });
    auto * only = new QCheckBox{"Only the passes over the storms and places of interest", widget};
    only->setChecked(onlyOver);
    QObject::connect(only, &QCheckBox::toggled, [this, changed] (bool on) { onlyOver = on; changed(); });
    layout->addWidget(only);
    for (const auto kind : {SatellitePasses::Kind::Imagery, SatellitePasses::Kind::Microwave, SatellitePasses::Kind::Wind, SatellitePasses::Kind::Radar}) {
        auto * check = new QCheckBox{QString::fromStdString(SatellitePasses::kindName(kind)), widget};
        check->setChecked(kinds[static_cast<int>(kind)]);
        const int at = static_cast<int>(kind);
        QObject::connect(check, &QCheckBox::toggled, [this, at, changed] (bool on) { kinds[at] = on; changed(); });
        layout->addWidget(check);
    }
    return widget;
}
