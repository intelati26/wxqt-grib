// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef MAPLEGEND_H
#define MAPLEGEND_H

#include <vector>
#include <QColor>
#include <QPainter>
#include <QString>

// The legend of a map screen, drawn along the foot of the map in window units (1000 wide from -500, 1000 tall from -250): rows of coloured marks with
// their names. The rows are packed into the width: a short row shares a line with the one before it, a long row wraps onto more lines, and the dark
// strip behind them is only as tall as they need, so the legend takes the room it needs and no more.
struct MapLegendEntry {
    enum Shape { Circle, Square, Diamond, Line } shape{Circle};
    QColor color;
    QString label;
};

struct MapLegendRow {
    QString title;                         // "Buoys, wind:"
    std::vector<MapLegendEntry> entries;
};

namespace MapLegend {
    // `perPixel`: window units per screen pixel; the font is the painter's. Returns the top of the legend (window units), so a caller can keep clear of it.
    double draw(QPainter& painter, const std::vector<MapLegendRow>& rows, double perPixel);
}

#endif  // MAPLEGEND_H
