// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef DROUGHTOUTLOOKHISTORY_H
#define DROUGHTOUTLOOKHISTORY_H

#include <array>
#include <string>
#include <vector>
#include <QDate>
#include "drought/UtilityDrought.h"

// How much of an area lies in each category of the Climate Prediction Center's 6-10 and 8-14 day outlooks, outlook by outlook (the day each was issued): the CPC posts every day's outlook as a KMZ
// of probability areas (ftp.cpc.ncep.noaa.gov/GIS/us_tempprcpfcst/610temp_<yyyymmdd>.kmz, back to 2012). Each is read once and kept as a 0.1 degree grid of its category (a few KB, in the drought
// cache), from which the share of any area is counted, weighted by the cosine of latitude.
namespace DroughtOutlookHistory {
    struct Kind {
        std::string key;      // "610temp" (the file's prefix)
        std::string name;     // "6-10 day temperature"
        bool temperature{true};
    };
    const std::vector<Kind>& kinds();

    // The categories, as the cells hold them: 0 none, 1 to 7 above normal with a probability of 33 to 39, 40 to 49, 50 to 59, 60 to 69, 70 to 79, 80 to 89, 90 and over, 8 near normal, 9 to 15 below normal
    // (33 ... 90 and over). Drawn from the bottom (the most below) to the top (the most above): stackOrder.
    constexpr int categories = 16;
    int bin(const std::string& category, int probability);                // 0 for what is not a category
    std::string label(int bin, bool temperature);                         // "60% to 70% Above", "Near Normal"
    const std::array<int, 15>& stackOrder();                              // bins from the bottom of a bar to the top

    constexpr double west = -125.0, south = 24.0, east = -66.5, north = 50.0, step = 0.1;   // the contiguous states
    // The outlook issued on a day as a grid: from the cache, else fetched (the KMZ, a few hundred KB, not stored) and made into the grid kept for good. Blocking: off the interface thread.
    // false with the reason (no outlook that day: the newest is not out until the afternoon).
    bool day(const Kind& kind, const QDate& issued, UtilityDrought::Raster& out, std::string& error);
    // A grid for the masks: the geometry of every day's grid, no categories.
    UtilityDrought::Raster geometry();
    // percent of the masked cells (weighted by the cosine of latitude) in each category (index 0: none, a cell the outlook does not reach)
    std::array<double, categories> shares(const UtilityDrought::Raster& grid, const std::vector<uint8_t>& mask);
}

#endif  // DROUGHTOUTLOOKHISTORY_H
