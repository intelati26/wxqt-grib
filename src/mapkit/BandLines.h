// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef BANDLINES_H
#define BANDLINES_H

#include <utility>
#include <vector>
#include <QPainter>
#include <QString>
#include "gfs/GfsGrid.h"
#include "mapkit/MapLayer.h"

// The edges of the colour bands of a filled map as thin lines with their values on them, so that the map reads without the legend. make() finds the lines (where the grid crosses each level),
// paint() draws them under the layer's own picture, with a number on the long ones where there is room.
namespace BandLines {
    struct Contour {
        QString label;
        std::vector<GfsGrid::Line> lines;
    };
    // levels: the value of each edge (in the grid's units) with the text to put on it
    std::vector<Contour> make(const GfsGrid::Grid& grid, const std::vector<std::pair<double, QString>>& levels);
    // `line`: the color of the lines (the numbers are always on white); `heavy` thicker lines
    void paint(QPainter& painter, MapHost& host, const std::vector<Contour>& contours, const QColor& line = QColor{30, 30, 40, 150}, double heavy = 1.0);
}

#endif  // BANDLINES_H
