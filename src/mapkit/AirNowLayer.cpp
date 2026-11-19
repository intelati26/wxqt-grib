// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "mapkit/AirNowLayer.h"
#include <cmath>
#include <QPainter>
#include <QTimeZone>
#include "radar/MapLegend.h"
#include "radar/MapView.h"

namespace {
    const QColor colors[6] = {QColor{0, 228, 0}, QColor{255, 255, 0}, QColor{255, 126, 0}, QColor{255, 0, 0}, QColor{143, 63, 151}, QColor{126, 0, 35}};
    const char * names[6] = {"Good", "Moderate", "Unhealthy for sensitive groups", "Unhealthy", "Very unhealthy", "Hazardous"};
    long noonOf(const QDate& date) {
        return QDateTime{date, QTime{12, 0}, QTimeZone::utc()}.toSecsSinceEpoch();
    }
}

QDate AirNowLayer::shownDate() const {
    if (wanted.isValid() && days.count(wanted) > 0) {
        return wanted;
    }
    return days.empty() ? QDate{} : days.begin()->first;
}

vector<long> AirNowLayer::times() const {
    vector<long> out;
    for (const auto& [date, points] : days) {
        out.push_back(noonOf(date));
    }
    return out;
}

string AirNowLayer::timeText() const {
    const auto date = shownDate();
    return date.isValid() ? date.toString("ddd yyyy-MM-dd").toStdString() + "  AirNow" : "AirNow";
}

void AirNowLayer::showTime(long seconds, MapHost& host) {
    wanted = seconds == 0 ? QDate{} : QDateTime::fromSecsSinceEpoch(seconds, QTimeZone::utc()).date();
    host.redraw();
}

void AirNowLayer::setAsOf(const QDateTime& utc, MapHost& host) {   // the history page: that day (a past day reads its own kept file)
    wanted = utc.toUTC().date();
    days.clear();
    refresh(host);
}

string AirNowLayer::summary() const {
    const auto date = shownDate();
    if (!date.isValid()) {
        return loading ? "reading AirNow..." : "no AirNow data";
    }
    return std::to_string(days.at(date).size()) + " areas, " + date.toString("ddd MMM d").toStdString();
}

void AirNowLayer::refresh(MapHost& host) {
    if (loading) {
        return;
    }
    loading = true;
    const auto first = wanted.isValid() && wanted < QDate::currentDate() ? wanted : QDate::currentDate();
    const int count = wanted.isValid() && wanted < QDate::currentDate() ? 1 : 6;   // (a day that has passed is that day alone; else today and the five days ahead)
    auto result = std::make_shared<std::map<QDate, std::vector<AirNow::Point>>>();
    const auto alive = this->alive;
    host.background([result, first, count] {
        for (int i = 0; i < count; i++) {
            auto points = AirNow::points(first.addDays(i));
            if (!points.empty()) {
                (*result)[first.addDays(i)] = std::move(points);
            }
        }
    }, [this, result, alive, &host] {
        if (!*alive) {
            return;
        }
        loading = false;
        days = std::move(*result);
        host.timesChanged();
        host.redraw();
    });
}

void AirNowLayer::paint(QPainter& painter, MapHost& host) {
    const auto date = shownDate();
    if (!date.isValid()) {
        return;
    }
    const auto t = host.view().transform();
    const double px = host.view().unitsPerPixel();
    painter.setPen(QPen{QColor{20, 20, 20, 190}, 0.8 * px});
    for (int category = 0; category < 6; category++) {   // the worse on top
        painter.setBrush(QColor{colors[category].red(), colors[category].green(), colors[category].blue(), 215});
        const double radius = (3.6 + 0.5 * category) * px;
        for (const auto& p : days.at(date)) {
            if (p.category == category) {
                painter.drawEllipse(t(p.lat, p.lon), radius, radius);
            }
        }
    }
}

MapHit AirNowLayer::pick(const QPointF& pixels, MapHost& host) const {
    MapHit hit;
    hit.reach = 9.0;
    hit.priority = 14;
    const auto date = shownDate();
    if (!date.isValid()) {
        return hit;
    }
    for (const auto& p : days.at(date)) {
        const auto at = host.view().toPixels(p.lat, p.lon);
        const double d = std::hypot(at.x() - pixels.x(), at.y() - pixels.y());
        if (d < hit.distance) {
            hit.distance = d;
            hit.text = QString{"%1%2\n%3"}.arg(names[p.category], p.aqi >= 0.0 ? QString{" (index %1)"}.arg(std::lround(p.aqi)) : QString{}, date.toString("ddd MMM d"));
        }
    }
    return hit;
}

vector<MapLegendRow> AirNowLayer::legend() const {
    MapLegendRow row;
    row.title = "Air quality (EPA AirNow):";
    for (int k = 0; k < 6; k++) {
        row.entries.push_back({MapLegendEntry::Circle, colors[k], names[k]});
    }
    return {row};
}
