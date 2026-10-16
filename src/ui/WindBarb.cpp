// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "ui/WindBarb.h"
#include <cmath>
#include <numbers>
#include <QPolygonF>
#include <chrono>
#include "util/Utility.h"

namespace {
    // the style is a setting (WIND_BARB_ARROW2, off to begin with: the standard barb); read again at most once a second, however many barbs are drawn
    bool arrowStyle() {
        static bool arrow = false;
        static std::chrono::steady_clock::time_point read{};
        const auto now = std::chrono::steady_clock::now();
        if (read == std::chrono::steady_clock::time_point{} || now - read > std::chrono::seconds{1}) {
            arrow = Utility::readPref("WIND_BARB_ARROW2", "false") == "true";
            read = now;
        }
        return arrow;
    }
}

WindBarb::Shape WindBarb::shape(double fromDegrees, double knots, double length, bool south) {
    Shape out;
    const double rad = fromDegrees * std::numbers::pi / 180.0;
    const QPointF staff{std::sin(rad), -std::cos(rad)};
    const QPointF perp = south ? QPointF{staff.y(), -staff.x()} : QPointF{-staff.y(), staff.x()};
    if (knots < 2.5) {
        out.calmRadius = length * 0.12;
        return out;
    }
    const double step = length * 0.14;
    const double feather = length * 0.42;
    int remaining = static_cast<int>(std::lround(knots / 5.0)) * 5;
    if (arrowStyle()) {   // an arrow: the shaft runs from `at` the way the wind goes, the head at its far end, the feathers (the speed) at the tail, leaning back like fletching
        const QPointF heading = -staff;
        const QPointF tip = heading * length;
        out.lines.push_back({QPointF{}, tip});
        const double head = length * 0.26;
        out.lines.push_back({tip, tip - heading * head + perp * (head * 0.5)});
        out.lines.push_back({tip, tip - heading * head - perp * (head * 0.5)});
        double along = 0.0;
        while (remaining >= 50) {
            const QPointF a = heading * along;
            out.pennants.push_back(QPolygonF{} << a << a + perp * feather << a + heading * (step * 1.4));
            along += step * 1.6;
            remaining -= 50;
        }
        while (remaining >= 10) {
            const QPointF a = heading * along;
            out.lines.push_back({a, a + perp * feather - heading * (step * 0.9)});
            along += step;
            remaining -= 10;
        }
        if (remaining >= 5) {
            if (along <= 1e-6) {
                along += step;   // a lone half feather sits one step in from the tail
            }
            const QPointF a = heading * along;
            out.lines.push_back({a, a + perp * feather * 0.5 - heading * (step * 0.45)});
        }
        return out;
    }
    // the standard barb: the staff from the station toward where the wind comes from, the feathers at its free end leaning away from the station
    out.lines.push_back({QPointF{}, staff * length});
    double along = length;
    while (remaining >= 50) {
        const QPointF a = staff * along;
        out.pennants.push_back(QPolygonF{} << a << a + perp * feather << a - staff * (step * 1.4));
        along -= step * 1.6;
        remaining -= 50;
    }
    while (remaining >= 10) {
        const QPointF a = staff * along;
        out.lines.push_back({a, a + perp * feather + staff * (step * 0.9)});
        along -= step;
        remaining -= 10;
    }
    if (remaining >= 5) {
        if (along >= length - 1e-6) {
            along -= step;   // a lone half barb sits one step in from the end
        }
        const QPointF a = staff * along;
        out.lines.push_back({a, a + perp * feather * 0.5 + staff * (step * 0.45)});
    }
    return out;
}

void WindBarb::draw(QPainter& painter, const QPointF& at, double fromDegrees, double knots, double length, bool south) {
    const auto barb = shape(fromDegrees, knots, length, south);
    if (barb.calmRadius > 0.0) {
        painter.drawEllipse(at, barb.calmRadius, barb.calmRadius);
        return;
    }
    for (const auto& [a, b] : barb.lines) {
        painter.drawLine(at + a, at + b);
    }
    for (auto flag : barb.pennants) {
        flag.translate(at);
        painter.drawPolygon(flag);
    }
}
