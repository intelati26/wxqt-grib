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
    // the style is a setting (WIND_BARB_ARROW, on to begin with); read again at most once a second, however many barbs are drawn
    bool arrowStyle() {
        static bool arrow = true;
        static std::chrono::steady_clock::time_point read{};
        const auto now = std::chrono::steady_clock::now();
        if (read == std::chrono::steady_clock::time_point{} || now - read > std::chrono::seconds{1}) {
            arrow = Utility::readPref("WIND_BARB_ARROW", "true") != "false";
            read = now;
        }
        return arrow;
    }
}

void WindBarb::draw(QPainter& painter, const QPointF& at, double fromDegrees, double knots, double length, bool south) {
    const double rad = fromDegrees * std::numbers::pi / 180.0;
    const QPointF staff{std::sin(rad), -std::cos(rad)};
    const QPointF perp = south ? QPointF{staff.y(), -staff.x()} : QPointF{-staff.y(), staff.x()};
    if (knots < 2.5) {
        painter.drawEllipse(at, length * 0.12, length * 0.12);
        return;
    }
    if (arrowStyle()) {   // an arrow: the shaft runs from `at` the way the wind goes, the head at its far end, the feathers (the speed) at the tail, leaning back like fletching
        const QPointF heading = -staff;
        const QPointF tip = at + heading * length;
        painter.drawLine(at, tip);
        const double head = length * 0.26;
        painter.drawLine(tip, tip - heading * head + perp * (head * 0.5));
        painter.drawLine(tip, tip - heading * head - perp * (head * 0.5));
        int remaining = static_cast<int>(std::lround(knots / 5.0)) * 5;
        double along = 0.0;
        const double step = length * 0.14;
        const double feather = length * 0.42;
        while (remaining >= 50) {
            const QPointF a = at + heading * along;
            QPolygonF flag;
            flag << a << a + perp * feather << a + heading * (step * 1.4);
            painter.drawPolygon(flag);
            along += step * 1.6;
            remaining -= 50;
        }
        while (remaining >= 10) {
            const QPointF a = at + heading * along;
            painter.drawLine(a, a + perp * feather - heading * (step * 0.9));
            along += step;
            remaining -= 10;
        }
        if (remaining >= 5) {
            if (along <= 1e-6) {
                along += step;   // a lone half feather sits one step in from the tail
            }
            const QPointF a = at + heading * along;
            painter.drawLine(a, a + perp * feather * 0.5 - heading * (step * 0.45));
        }
        return;
    }
    painter.drawLine(at, at + staff * length);
    int remaining = static_cast<int>(std::lround(knots / 5.0)) * 5;
    double along = length;
    const double step = length * 0.14;
    const double feather = length * 0.42;
    while (remaining >= 50) {
        const QPointF a = at + staff * along;
        QPolygonF flag;
        flag << a << a + perp * feather << a - staff * (step * 1.4);
        painter.drawPolygon(flag);
        along -= step * 1.6;
        remaining -= 50;
    }
    while (remaining >= 10) {
        const QPointF a = at + staff * along;
        painter.drawLine(a, a + perp * feather + staff * (step * 0.9));   // leaning to the free end, like the feathers of an arrow
        along -= step;
        remaining -= 10;
    }
    if (remaining >= 5) {
        if (along >= length - 1e-6) {
            along -= step;   // a lone half barb sits one step in from the end
        }
        const QPointF a = at + staff * along;
        painter.drawLine(a, a + perp * feather * 0.5 + staff * (step * 0.45));
    }
}
