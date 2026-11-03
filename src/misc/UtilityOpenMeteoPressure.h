// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef UTILITYOPENMETEOPRESSURE_H
#define UTILITYOPENMETEOPRESSURE_H

#include <string>
#include <vector>

using std::string;
using std::vector;

// The forecast sea level pressure at a point from Open-Meteo (https://open-meteo.com, free for non-commercial use, data CC BY 4.0): the NWS point forecast the home screen's
// hourly graph is made of has no pressure. One request gives the hourly values of the next days; the graph keeps every third hour.
namespace UtilityOpenMeteoPressure {
    struct Sample {
        long long utc;   // seconds since 1970
        double hPa;
    };
    // the answer for a point as text; empty when Open-Meteo did not answer. Blocking: not on the GUI thread
    string get(double lat, double lon);
    // the hourly samples in the answer, oldest first, each hour on the hour; `everyHours` 3: only the hours that are a multiple of three (UTC)
    vector<Sample> parse(const string& json, int everyHours = 3);
}

#endif  // UTILITYOPENMETEOPRESSURE_H
