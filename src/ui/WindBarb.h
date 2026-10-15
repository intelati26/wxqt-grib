// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef WINDBARB_H
#define WINDBARB_H

#include <QPainter>
#include <QPointF>

namespace WindBarb {
    // The barb as an arrow (the setting WIND_BARB_ARROW, on to begin with): the shaft runs from `at` the way the wind goes, with the head at its far end and the feathers
    // (the speed) at the tail, leaning back. With the setting off, a standard wind barb: the staff of `length` runs from `at` toward the direction the wind comes from (`fromDegrees`), a pennant is 50 kt, a
    // long barb 10 kt, a short barb 5 kt (speeds rounded to 5 kt); a small circle for calm. The feathers are on the clockwise side north of the
    // equator and on the other side south of it. The pen and brush of the painter are used (the pennants are filled).
    void draw(QPainter& painter, const QPointF& at, double fromDegrees, double knots, double length, bool south = false);
}

#endif  // WINDBARB_H
