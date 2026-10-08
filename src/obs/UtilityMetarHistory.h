// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef UTILITYMETARHISTORY_H
#define UTILITYMETARHISTORY_H

#include <string>
#include <vector>
#include "obs/SurfaceStation.h"

using std::string;
using std::vector;

// One airport's reports of the last day from the Aviation Weather Center's data API (aviationweather.gov/api/data/metar?ids=KOUN&format=json&hours=24).
class UtilityMetarHistory {
public:
    struct Ob {
        long seconds{0};
        double temperature{SurfaceStation::missing};   // C
        double dewPoint{SurfaceStation::missing};      // C
        double windDirection{SurfaceStation::missing};
        double windSpeed{SurfaceStation::missing};     // kt
        double windGust{SurfaceStation::missing};      // kt
        double altimeter{SurfaceStation::missing};     // inHg
        string raw;
    };
    static string url(const string& id, int hours = 24);
    // oldest first
    static vector<Ob> parse(const string& json);
};

#endif  // UTILITYMETARHISTORY_H
