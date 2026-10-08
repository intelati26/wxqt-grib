// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "ui/WindBarb.h"
#include <cmath>
#include <numbers>
#include <QPolygonF>

void WindBarb::draw(QPainter& painter, const QPointF& at, double fromDegrees, double knots, double length, bool south) {
    const double rad = fromDegrees * std::numbers::pi / 180.0;
    const QPointF staff{std::sin(rad), -std::cos(rad)};
    const QPointF perp = south ? QPointF{staff.y(), -staff.x()} : QPointF{-staff.y(), staff.x()};
    if (knots < 2.5) {
        painter.drawEllipse(at, length * 0.12, length * 0.12);
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
        flag << a << a + perp * feather - staff * (step * 0.4) << a - staff * (step * 1.4);
        painter.drawPolygon(flag);
        along -= step * 1.6;
        remaining -= 50;
    }
    while (remaining >= 10) {
        const QPointF a = at + staff * along;
        painter.drawLine(a, a + perp * feather - staff * (step * 0.6));
        along -= step;
        remaining -= 10;
    }
    if (remaining >= 5) {
        if (along >= length - 1e-6) {
            along -= step;   // a lone half barb sits one step in from the end
        }
        const QPointF a = at + staff * along;
        painter.drawLine(a, a + perp * feather * 0.5 - staff * (step * 0.3));
    }
}
