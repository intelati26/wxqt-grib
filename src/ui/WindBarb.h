// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef WINDBARB_H
#define WINDBARB_H

#include <utility>
#include <vector>
#include <QPainter>
#include <QPolygonF>
#include <QPointF>

namespace WindBarb {
    // The barb as an arrow (the setting WIND_BARB_ARROW2, off to begin with): the shaft runs from `at` the way the wind goes, with the head at its far end and the feathers
    // (the speed) at the tail, leaning back. With the setting off, a standard wind barb: the staff of `length` runs from `at` toward the direction the wind comes from (`fromDegrees`), a pennant is 50 kt, a
    // long barb 10 kt, a short barb 5 kt (speeds rounded to 5 kt); a small circle for calm. The feathers are on the clockwise side north of the
    // equator and on the other side south of it. The pen and brush of the painter are used (the pennants are filled).
    void draw(QPainter& painter, const QPointF& at, double fromDegrees, double knots, double length, bool south = false);

    // The same barb as shapes, for the drawers that are not a QPainter (the radar map's line lists): the lines and the pennants relative to the station, x east and y south, in the units
    // of `length` (the staff). draw() paints exactly these, so every barb the program draws comes from this one routine.
    struct Shape {
        std::vector<std::pair<QPointF, QPointF>> lines;
        std::vector<QPolygonF> pennants;
        double calmRadius{0.0};            // above 0: a calm wind, a small circle of this radius
    };
    Shape shape(double fromDegrees, double knots, double length, bool south = false);
}

#endif  // WINDBARB_H
