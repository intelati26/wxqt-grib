// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "mapkit/ForecastPointLayer.h"
#include <algorithm>
#include <cmath>
#include <QFontMetricsF>
#include "misc/ForecastPointViewer.h"
#include "settings/Location.h"

using UtilityForecastPoint::has;

void ForecastPointLayer::refresh(MapHost& host) {
    if (loading) {
        return;
    }
    loading = true;
    auto fresh = std::make_shared<std::vector<Entry>>();
    const auto names = Location::listOfNames();
    const auto where = Location::getListLatLons();
    for (size_t i = 0; i < where.size(); i++) {
        fresh->push_back({i < names.size() ? names[i] : std::string{}, where[i].lat(), where[i].lon(), nullptr});
    }
    host.background([fresh] {
        for (auto& entry : *fresh) {
            entry.data = std::make_shared<UtilityForecastPoint::Data>(UtilityForecastPoint::fetch(entry.lat, entry.lon));
        }
    }, [this, &host, fresh] {
        loading = false;
        points = fresh;
        host.redraw();
    });
}

void ForecastPointLayer::paint(QPainter& painter, MapHost& host) {
    if (!points) {
        return;
    }
    auto& view = host.view();
    const auto t = view.transform();
    const double perPixel = view.unitsPerPixel();
    QFont font{painter.font()};
    font.setPixelSize(std::max(6, static_cast<int>(std::lround(12.0 * perPixel))));
    font.setBold(true);
    painter.setFont(font);
    const QFontMetricsF metrics{font};
    for (const auto& entry : *points) {
        const QPointF at = t(entry.lat, entry.lon);
        if (at.x() < -500.0 || at.x() > 500.0 || at.y() < -250.0 || at.y() > 750.0) {
            continue;
        }
        const double r = 5.0 * perPixel;
        painter.setPen(QPen{QColor{255, 255, 255}, 1.6 * perPixel});
        painter.setBrush(QColor{30, 110, 220});
        painter.drawEllipse(at, r, r);
        QString text = QString::fromStdString(entry.name);
        if (entry.data && entry.data->ok && !entry.data->days.empty()) {
            const auto& day = entry.data->days.front();
            text += "  " + (has(day.maxTemp) ? QString::number(static_cast<int>(std::lround(day.maxTemp))) : QString{"-"}) + "/" + (has(day.minTemp) ? QString::number(static_cast<int>(std::lround(day.minTemp))) : QString{"-"}) +
                (has(day.maxPop) && day.maxPop >= 5.0 ? "  " + QString::number(static_cast<int>(std::lround(day.maxPop))) + "%" : QString{});
        }
        const QRectF box{at.x() + r + 2.0 * perPixel, at.y() - metrics.height() / 2.0, metrics.horizontalAdvance(text) + 6.0 * perPixel, metrics.height()};
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor{20, 20, 28, 190});
        painter.drawRoundedRect(box, 3.0 * perPixel, 3.0 * perPixel);
        painter.setPen(QColor{240, 240, 240});
        painter.drawText(box.adjusted(3.0 * perPixel, 0, 0, 0), Qt::AlignVCenter | Qt::AlignLeft, text);
    }
}

MapHit ForecastPointLayer::pick(const QPointF& pixels, MapHost& host) const {
    MapHit hit;
    hit.reach = 16.0;
    hit.priority = 30;
    if (!points) {
        return hit;
    }
    for (const auto& entry : *points) {
        const auto at = host.view().toPixels(entry.lat, entry.lon);
        const double distance = std::hypot(at.x() - pixels.x(), at.y() - pixels.y());
        if (distance < hit.distance && entry.data && entry.data->ok) {
            hit.distance = distance;
            QString text = QString::fromStdString(entry.name) + "  (NWS " + entry.data->office + ")";
            for (size_t d = 0; d < std::min<size_t>(3, entry.data->days.size()); d++) {
                const auto& day = entry.data->days[d];
                text += "\\n" + day.date.toString("ddd M/d") + ":  " + (has(day.maxTemp) ? QString::number(static_cast<int>(std::lround(day.maxTemp))) : QString{"-"}) + " / " + (has(day.minTemp) ? QString::number(static_cast<int>(std::lround(day.minTemp))) : QString{"-"}) +
                    " F,  wind " + (has(day.maxWind) ? QString::number(static_cast<int>(std::lround(day.maxWind))) : QString{"-"}) + " mph,  rain " + (has(day.maxPop) ? QString::number(static_cast<int>(std::lround(day.maxPop))) : QString{"-"}) + "%";
            }
            hit.text = text + "\\nClick for the full page";
            const auto data = entry.data;
            hit.open = [data] (Window * parent) { new ForecastPointViewer{parent, data}; };
        }
    }
    return hit;
}

vector<MapLegendRow> ForecastPointLayer::legend() const {
    return {MapLegendRow{"Forecast points:", {{MapLegendEntry::Circle, QColor{30, 110, 220}, "your locations: high / low, chance of rain"}}}};
}

string ForecastPointLayer::summary() const {
    return points ? std::to_string(points->size()) + " saved locations" : string{"loading"};
}
