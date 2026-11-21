// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef DROUGHTVECTOR_H
#define DROUGHTVECTOR_H

#include <string>
#include "drought/UtilityDrought.h"

// A week's Drought Monitor map kept small: the shapes of the KMZ (140 to 550 KB a week, its coordinates in text, with Alaska, Hawaii and Puerto Rico) thinned (Douglas-Peucker), snapped to a
// grid of `quantum` degrees and written as differences in varints; the cache's zlib then makes it about 30 KB a week at 0.02 degree (50 MB for every week since 2000, against 500 MB of KMZs).
// The area of each category stays within 0.35 % of the exact one and a commodity's share (DroughtCrops) within a point except where a heavy county lies on an edge (docs/plans.md, section 32).
// 0.05 degree gives 23 KB but moves the share of a commodity of a few heavy counties by up to 6.7 points.
namespace DroughtVector {
    constexpr double tolerance = 0.02;   // degrees: the thinning
    constexpr double quantum = 0.01;     // degrees: the snap
    std::string encode(const UtilityDrought::Monitor& monitor);
    bool decode(const std::string& bytes, UtilityDrought::Monitor& out);
}

#endif  // DROUGHTVECTOR_H
