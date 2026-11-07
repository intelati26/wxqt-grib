// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef OMGRID_H
#define OMGRID_H

#include <vector>
#include "gfs/GfsGrid.h"

namespace OmGrid {
    // ECMWF's octahedral reduced Gaussian grid O<n> (O320 is the EC46's, O1280 the IFS's): 2n rings of latitude from the north, the first with 20 points and each one after 4 more (to the equator, then
    // back), the points of a ring evenly spaced from longitude 0. The file holds them ring after ring.
    size_t gaussianPoints(int n);
    // The values of such a grid, bilinear (along each of the two rings round a point, then between the rings) onto a regular `step` degree latitude / longitude grid of the whole globe
    // (the box is cut to `south`..`north` and `west`..`east`, which have to be multiples of the step; longitudes -180..180 style when west < 0).
    bool gaussianToRegular(const std::vector<float>& values, int n, double step, double west, double south, double east, double north, GfsGrid::Grid& out, std::string& error);
}

#endif  // OMGRID_H
