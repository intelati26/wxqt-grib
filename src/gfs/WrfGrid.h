// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef WRFGRID_H
#define WRFGRID_H

#include <vector>
#include "gfs/GfsGrid.h"

// A model grid that is not latitude / longitude (the Lambert conformal grid of a WRF run, the rotated latitude / longitude grid of the NAM's descendants): all that is known is the latitude and the
// longitude of each of its points (WRF's XLAT and XLONG). Whatever the projection, the fields are put on a regular latitude / longitude grid by finding, for each cell of it, where it falls among the
// points (the nearest point, then a step along the grid's own directions there) and taking the bilinear value.
class WrfGrid {
public:
    // latitude[j * columns + i] and longitude[...] of the points, row j = 0 first; false (with the reason) for a grid too small, or one with no points in the box
    bool build(const std::vector<float>& latitude, const std::vector<float>& longitude, int columns, int rows, std::string& error);
    // a field on the points (same order) as a regular grid: the first row is the northern one (as the charts have it); NaN where the cell is outside the points
    GfsGrid::Grid apply(const std::vector<float>& values) const;
    int columns() const { return outColumns; }
    int rows() const { return outRows; }

private:
    struct Cell {
        int i{-1}, j{-1};     // the point at the lower left of the four around the cell (i < 0: outside)
        float wx{0}, wy{0};   // how far from it, as a share of the way to the next in each direction
    };
    int sourceColumns{0}, sourceRows{0};
    int outColumns{0}, outRows{0};
    double west{0}, north{0}, step{0.05};
    std::vector<Cell> cells;
};

#endif  // WRFGRID_H
