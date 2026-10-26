// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef GEOSOPENDAP_H
#define GEOSOPENDAP_H

#include <string>
#include "gfs/GfsData.h"

// A field of NASA's GEOS-FP read from the GrADS Data Server (OPeNDAP) instead of the whole file of the data portal: the run's own dataset ("tavg1_2d_slv_Nx.20261026_06"), one variable at one time,
// a few megabytes of the 48 MB file, put on the model's 0.25 degree grid (the native one is 0.3125 by 0.25). `file` is the collection the model file names and `variable` the name in it ("H500");
// false with the reason when the server has no such dataset, time or variable (the caller then reads the file).
namespace GeosOpendap {
    bool read(const GfsData::Run& run, int forecastHour, const std::string& file, const std::string& variable, GfsGrid::Grid& out, std::string& error);
    // the pieces of the reading that need no network (for the tests): the first time of a dataset's time axis from its DAS text, in minutes since the run's day 0 ("03:30z26oct2026" -> a date and time)
    bool parseStart(const std::string& das, long long& startSeconds, int& stepMinutes, int& count);
    // the values of a one-variable DODS reply: false when it is not one or is not `expected` values
    bool parseDods(const std::string& reply, size_t expected, std::vector<float>& values);
}

#endif  // GEOSOPENDAP_H
