// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef CFS30LOADER_H
#define CFS30LOADER_H

#include <string>
#include "gfs/GfsGrid.h"

namespace Cfs30Loader {
    // The same for any stretch: the mean of the CFSv2's `days` days from tomorrow (7 to 90) against the CFS normal of those days, a field of the CFS's grid (degrees C, mm/day) for the CFS key
    // ("t2m", "precip"). The model's forecast "hour" is the stretch (168 a week, 720 the 30 days). Fields are read by byte range and kept, so asking again is cheap; false with the reason.
    bool period(int days, const std::string& key, GfsGrid::Grid& out, std::string& error);
    // What a stretch looked like when it was last worked out, at once: today's result, else the newest of the last five days. `ageDays` 0 for today's. false when there is none. (The fresh one is
    // `period`, which keeps its result for the day: the first of the day reads the fields, the rest and the next program start read a small file.)
    bool cached(int days, const std::string& key, GfsGrid::Grid& out, int& ageDays);

    // The CFSv2's next 30 days against its own normal as a Model Viewer model ("CFS30", the loader of the chart registry's source of that id): the member 1 of the four newest runs sampled at the four
    // synoptic times of every third day from tomorrow (160 fields), their mean minus the mean of the CFS normals (NOAA's reforecast climatology) of the same days. A field of the CFS's grid in the shown unit
    // (degrees C, mm/day); the want's variable is the CFS key ("t2m", "precip"). The fields are those of NOAA's open bucket (byte ranges, kept), no web service with a quota.
    void registerLoader();
}

#endif  // CFS30LOADER_H
