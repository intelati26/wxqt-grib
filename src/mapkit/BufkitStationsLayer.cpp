// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "mapkit/BufkitStationsLayer.h"
#include <algorithm>
#include <cmath>
#include <set>
#include <QFontMetricsF>
#include "models/SoundingViewer.h"
#include "radar/MapView.h"
#include "sounding/UtilityBufkit.h"

namespace {
    constexpr double spacing = 34.0;   // pixels between two buttons
    constexpr size_t most = 220;
}

// the stations in view that get a button: offices first, then the rest, none within `spacing` pixels of another
std::vector<BufkitStationsLayer::Shown> BufkitStationsLayer::choose(MapHost& host) const {
    std::vector<Shown> out;
    auto& view = host.view();
    const auto * widget = view.map();
    if (widget == nullptr) {
        return out;
    }
    const auto& offices = UtilityBufkit::officeStations();
    std::vector<const UtilityBufkit::Station *> order;
    for (const auto& station : UtilityBufkit::stations()) {
        order.push_back(&station);
    }
    std::stable_sort(order.begin(), order.end(), [&offices] (const auto * a, const auto * b) { return offices.count(a->id) > offices.count(b->id); });
    std::set<std::pair<int, int>> taken;
    const double w = widget->width(), h = widget->height();
    for (const auto * station : order) {
        const auto p = view.toPixels(station->latitude, station->longitude);
        if (p.x() < 0 || p.y() < 0 || p.x() > w || p.y() > h) {
            continue;
        }
        const int cx = static_cast<int>(p.x() / spacing), cy = static_cast<int>(p.y() / spacing);
        bool near = false;
        for (int dx = -1; dx <= 1 && !near; dx++) {
            for (int dy = -1; dy <= 1 && !near; dy++) {
                near = taken.count({cx + dx, cy + dy}) > 0;
            }
        }
        if (near) {
            continue;
        }
        taken.insert({cx, cy});
        out.push_back({station, view.transform()(station->latitude, station->longitude), p, station->id});
        if (out.size() >= most) {
            break;
        }
    }
    return out;
}

void BufkitStationsLayer::paint(QPainter& painter, MapHost& host) {
    const double perPixel = host.view().unitsPerPixel();
    QFont font{painter.font()};
    font.setPixelSize(std::max(6, static_cast<int>(std::lround(10.0 * perPixel))));
    font.setBold(true);
    painter.setFont(font);
    const QFontMetricsF metrics{font};
    for (const auto& s : choose(host)) {
        const double r = 4.5 * perPixel;
        painter.setPen(QPen{QColor{255, 255, 255}, 1.4 * perPixel});
        painter.setBrush(QColor{200, 70, 30});
        painter.drawEllipse(s.units, r, r);
        const QString text = QString::fromStdString(s.id);
        const QRectF box{s.units.x() + r + 1.5 * perPixel, s.units.y() - metrics.height() / 2.0, metrics.horizontalAdvance(text) + 4.0 * perPixel, metrics.height()};
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor{20, 20, 28, 170});
        painter.drawRoundedRect(box, 2.0 * perPixel, 2.0 * perPixel);
        painter.setPen(QColor{240, 240, 240});
        painter.drawText(box.adjusted(2.0 * perPixel, 0, 0, 0), Qt::AlignVCenter | Qt::AlignLeft, text);
    }
}

MapHit BufkitStationsLayer::pick(const QPointF& pixels, MapHost& host) const {
    MapHit hit;
    hit.reach = 12.0;
    hit.priority = 20;
    for (const auto& s : choose(host)) {
        const double distance = std::hypot(s.pixels.x() - pixels.x(), s.pixels.y() - pixels.y());
        if (distance < hit.distance) {
            hit.distance = distance;
            hit.text = QString::fromStdString(s.id + "\nClick for the RRFS sounding");
            const string id = s.id;
            hit.open = [id] (Window * parent) { new SoundingViewer{parent, "RRFS", id, true}; };
        }
    }
    return hit;
}

string BufkitStationsLayer::summary() const {
    return std::to_string(UtilityBufkit::stations().size()) + " stations";
}
