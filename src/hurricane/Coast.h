// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef COAST_H
#define COAST_H

#include <utility>
#include <vector>

// The coastlines and borders of the Atlantic basin (resourceCreation/res/nhc_basins.bin, Natural Earth): lines of (longitude, latitude) points
class Coast {
public:
    static const std::vector<std::vector<std::pair<float, float>>>& lines();
    // the whole world (resourceCreation/res/world_coast.bin, Natural Earth 1:50m, simplified to 0.03 degrees): the master map and the global charts
    static const std::vector<std::vector<std::pair<float, float>>>& worldLines();
};

#endif  // COAST_H
