// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "mapkit/SpcOutlookLayer.h"
#include <algorithm>
#include <cmath>
#include <set>
#include <QComboBox>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QPainterPath>
#include <QPolygonF>
#include <QTimeZone>
#include <QVBoxLayout>
#include "radar/MapView.h"
#include "ui/UiStandards.h"
#include "util/Utility.h"
#include "util/UtilityIO.h"

namespace {
    const char * const productNames[4] = {"Categorical risk", "Tornado probability", "Damaging wind probability", "Hail probability"};
    const char * const fileNames[4] = {"cat", "torn", "wind", "hail"};

    QString urlFor(int day, int product) {
        const QString spc = "https://www.spc.noaa.gov/products/";
        if (day >= 4) {
            return spc + "exper/day4-8/day" + QString::number(day) + "prob.nolyr.geojson";
        }
        if (day == 3) {   // (the third day has the categorical and the total severe probability only)
            return spc + "outlook/day3otlk_" + (product == 0 ? "cat" : "prob") + ".nolyr.geojson";
        }
        return spc + "outlook/day" + QString::number(day) + "otlk_" + fileNames[product] + ".nolyr.geojson";
    }

    QString productOf(int day, int product) {
        if (day >= 4) {
            return "Severe probability (15% and 30%)";
        }
        if (day == 3 && product != 0) {
            return "Total severe probability";
        }
        return productNames[product];
    }

    // the rings of a GeoJSON geometry (a polygon, a multipolygon, or a collection of them): every outer ring and hole, (lon, lat)
    void rings(const QJsonObject& geometry, std::vector<std::vector<QPointF>>& out) {
        const auto type = geometry.value("type").toString();
        const auto ring = [&out] (const QJsonArray& points) {
            std::vector<QPointF> r;
            for (const auto& point : points) {
                const auto pair = point.toArray();
                if (pair.size() >= 2) {
                    r.emplace_back(pair.at(0).toDouble(), pair.at(1).toDouble());
                }
            }
            if (r.size() >= 3) {
                out.push_back(std::move(r));
            }
        };
        if (type == "GeometryCollection") {
            for (const auto& g : geometry.value("geometries").toArray()) {
                rings(g.toObject(), out);
            }
        } else if (type == "Polygon") {
            for (const auto& r : geometry.value("coordinates").toArray()) {
                ring(r.toArray());
            }
        } else if (type == "MultiPolygon") {
            for (const auto& polygon : geometry.value("coordinates").toArray()) {
                for (const auto& r : polygon.toArray()) {
                    ring(r.toArray());
                }
            }
        }
    }

    long secondsOf(const QString& iso) {
        const auto t = QDateTime::fromString(iso, Qt::ISODate);
        return t.isValid() ? static_cast<long>(t.toSecsSinceEpoch()) : 0;
    }

    QString zulu(long seconds) {
        return QDateTime::fromSecsSinceEpoch(seconds, QTimeZone::utc()).toString("ddd d HH:mm") + "Z";
    }
}

SpcOutlookLayer::SpcOutlookLayer() : product{std::clamp(Utility::readPrefInt("SPC_OUTLOOK_PRODUCT", 0), 0, 3)} {}

void SpcOutlookLayer::refresh(MapHost& host) {
    if (loading) {
        again = true;
        return;
    }
    loading = true;
    const int choice = product;
    auto out = std::make_shared<std::vector<Day>>(8);
    auto start = std::make_shared<long>(0);
    auto problem = std::make_shared<string>();
    host.background(
        [out, start, problem, choice] {
            int read = 0;
            for (int d = 1; d <= 8; d++) {
                auto& day = (*out)[static_cast<size_t>(d - 1)];
                day.number = d;
                day.product = productOf(d, choice);
                const auto doc = QJsonDocument::fromJson(UtilityIO::downloadAsByteArray(urlFor(d, choice).toStdString()));
                if (!doc.isObject()) {
                    continue;
                }
                day.read = true;
                read++;
                for (const auto& value : doc.object().value("features").toArray()) {
                    const auto feature = value.toObject();
                    const auto p = feature.value("properties").toObject();
                    if (const long expire = secondsOf(p.value("EXPIRE_ISO").toString()); expire > 0 && *start == 0 && d <= 3) {
                        *start = expire - 86400L * d;   // (the period of day d ends at `expire`: day 1 starts a day before that of day 1)
                    }
                    if (day.issued.isEmpty() && !p.value("ISSUE_ISO").toString().isEmpty()) {
                        day.issued = QDateTime::fromString(p.value("ISSUE_ISO").toString(), Qt::ISODate).toUTC().toString("yyyy-MM-dd HH:mm") + "Z";
                        day.forecaster = p.value("FORECASTER").toString();
                    }
                    Zone zone;
                    zone.fill = QColor{p.value("fill").toString()};
                    if (!zone.fill.isValid()) {
                        continue;   // ("less than 5%": no area)
                    }
                    zone.stroke = QColor{p.value("stroke").toString()};
                    if (!zone.stroke.isValid()) {
                        zone.stroke = zone.fill.darker(140);
                    }
                    zone.label = p.value("LABEL").toString();
                    zone.name = p.value("LABEL2").toString().isEmpty() ? zone.label : p.value("LABEL2").toString();
                    rings(feature.value("geometry").toObject(), zone.rings);
                    if (!zone.rings.empty()) {
                        day.zones.push_back(std::move(zone));
                    }
                }
            }
            if (read == 0) {
                *problem = "the outlooks could not be read";
            }
        },
        [this, out, start, problem, &host, alive = alive] {
            if (!*alive) {
                return;
            }
            loading = false;
            error = *problem;
            if (problem->empty()) {
                days = std::move(*out);
                if (*start > 0) {
                    base = *start;
                } else {   // (no file said: the 12Z that began the period holding now)
                    const auto now = QDateTime::currentDateTimeUtc();
                    auto today = QDateTime{now.date(), QTime{12, 0}, QTimeZone::utc()};
                    if (now < today) {
                        today = today.addDays(-1);
                    }
                    base = static_cast<long>(today.toSecsSinceEpoch());
                }
                host.timesChanged();
            }
            host.redraw();
            if (again) {
                again = false;
                refresh(host);
            }
        });
}

int SpcOutlookLayer::shownDay() const {
    if (days.empty() || base == 0) {
        return -1;
    }
    if (shown == 0) {
        return 0;
    }
    if (shown < base) {
        return -1;
    }
    const long index = (shown - base) / 86400L;
    return index > 7 ? -1 : static_cast<int>(index);
}

vector<long> SpcOutlookLayer::times() const {
    vector<long> out;
    if (!days.empty() && base > 0) {
        for (int i = 0; i < 8; i++) {
            out.push_back(dayStart(i));
        }
    }
    return out;
}

void SpcOutlookLayer::showTime(long seconds, MapHost& host) {
    shown = seconds;
    host.redraw();
}

string SpcOutlookLayer::timeText() const {
    const int d = shownDay();
    if (d < 0) {
        return {};
    }
    return "Day " + std::to_string(d + 1) + ", valid " + zulu(dayStart(d)).toStdString() + " to " + zulu(dayStart(d + 1)).toStdString() + (days[static_cast<size_t>(d)].issued.isEmpty() ? string{} : ", issued " + days[static_cast<size_t>(d)].issued.toStdString());
}

string SpcOutlookLayer::summary() const {
    if (loading && days.empty()) {
        return "reading the outlooks...";
    }
    if (!error.empty()) {
        return error;
    }
    const int d = shownDay();
    if (d < 0) {
        return days.empty() ? string{} : "no outlook for this time: days 1 to 8 are valid from " + zulu(dayStart(0)).toStdString() + " to " + zulu(dayStart(8)).toStdString();
    }
    const auto& day = days[static_cast<size_t>(d)];
    if (!day.read) {
        return "day " + std::to_string(d + 1) + ": not available";
    }
    return "day " + std::to_string(d + 1) + ": " + day.product.toStdString() + ", " + (day.zones.empty() ? string{"no areas"} : std::to_string(day.zones.size()) + (day.zones.size() == 1 ? " area" : " areas"));
}

void SpcOutlookLayer::paint(QPainter& painter, MapHost& host) {
    const int d = shownDay();
    if (d < 0 || !days[static_cast<size_t>(d)].read) {
        return;
    }
    const auto t = host.view().transform();
    const double px = host.view().unitsPerPixel();
    for (const auto& zone : days[static_cast<size_t>(d)].zones) {
        QPainterPath path;
        path.setFillRule(Qt::OddEvenFill);   // a hole is a ring inside a ring
        for (const auto& r : zone.rings) {
            QPolygonF polygon;
            for (const auto& p : r) {
                polygon << t(p.y(), p.x());
            }
            path.addPolygon(polygon);
            path.closeSubpath();
        }
        auto fill = zone.fill;
        fill.setAlpha(150);
        painter.setBrush(fill);
        painter.setPen(QPen{zone.stroke, 1.8 * px});
        painter.drawPath(path);
    }
}

MapHit SpcOutlookLayer::pick(const QPointF& pixels, MapHost& host) const {
    MapHit hit;
    const int d = shownDay();
    if (d < 0 || !days[static_cast<size_t>(d)].read) {
        return hit;
    }
    const auto& day = days[static_cast<size_t>(d)];
    for (auto it = day.zones.rbegin(); it != day.zones.rend(); ++it) {   // (the highest risk is the last)
        QPainterPath path;
        path.setFillRule(Qt::OddEvenFill);
        for (const auto& r : it->rings) {
            QPolygonF polygon;
            for (const auto& p : r) {
                polygon << host.view().toPixels(p.y(), p.x());
            }
            path.addPolygon(polygon);
            path.closeSubpath();
        }
        if (path.contains(pixels)) {
            hit.distance = 0.0;
            hit.priority = 4;
            hit.text = "SPC day " + QString::number(d + 1) + " outlook: " + day.product + "\n" + it->name + (it->label.isEmpty() || it->label == it->name ? QString{} : "  (" + it->label + ")") + "\nvalid " +
                       QString::fromStdString(zulu(dayStart(d)).toStdString()) + " to " + QString::fromStdString(zulu(dayStart(d + 1)).toStdString()) + (day.issued.isEmpty() ? QString{} : "\nissued " + day.issued) +
                       (day.forecaster.isEmpty() ? QString{} : "  by " + day.forecaster);
            return hit;
        }
    }
    return hit;
}

vector<MapLegendRow> SpcOutlookLayer::legend() const {
    const int d = shownDay();
    if (d < 0 || !days[static_cast<size_t>(d)].read) {
        return {};
    }
    const auto& day = days[static_cast<size_t>(d)];
    MapLegendRow row;
    row.title = "SPC day " + QString::number(d + 1) + ", " + day.product + (day.zones.empty() ? ": no areas" : ":");
    std::set<QString> seen;
    for (const auto& zone : day.zones) {
        if (seen.insert(zone.name).second) {
            row.entries.push_back({MapLegendEntry::Square, zone.fill, zone.name});
        }
    }
    return {row};
}

QWidget * SpcOutlookLayer::options(QWidget * parent, const std::function<void()>& changed) {
    auto * widget = new QWidget{parent};
    auto * layout = new QVBoxLayout{widget};
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(new QLabel{"Days 1 and 2 show:", widget});
    auto * choice = UiStandards::combo(widget);
    for (const auto name : productNames) {
        choice->addItem(name);
    }
    choice->setCurrentIndex(product);
    layout->addWidget(choice);
    auto * note = new QLabel{"Day 3 has the categorical risk and the total severe probability; days 4 to 8 the 15% and 30% areas. The time bar steps through the days.", widget};
    note->setWordWrap(true);
    UiStandards::muted(note);
    layout->addWidget(note);
    QObject::connect(choice, QOverload<int>::of(&QComboBox::currentIndexChanged), widget, [this, changed] (int index) {
        product = std::clamp(index, 0, 3);
        Utility::writePrefInt("SPC_OUTLOOK_PRODUCT", product);
        changed();
    });
    return widget;
}
