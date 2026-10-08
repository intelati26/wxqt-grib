// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef UTILITYWINDPROBABILITY_H
#define UTILITYWINDPROBABILITY_H

#include <string>
#include <vector>

using std::string;
using std::vector;

// NHC's wind speed probabilities as a map (nhc.noaa.gov/gis/forecast/archive/wsp_120hr5km_latest.zip): for 34, 50 and 64 kt, the chance over the next
// five days that sustained winds of at least that speed occur at a point, in bands (<5 %, 5-10 %, 10-20 % ... >90 %) drawn as polygons that cover every
// active storm of the cycle together. Looking a point up gives its band. Pure geometry on the shapefile reader's output.
class UtilityWindProbability {
public:
    using Ring = vector<std::pair<double, double>>;     // (lon, lat)
    struct Band {
        int low{0};             // percent; 0 for "<5 %"
        int high{5};            // percent; 100 for ">90 %"
        vector<Ring> rings;
    };
    struct Map {
        vector<Band> bands[3];   // 34, 50, 64 kt, the lowest band first
        string cycle;            // "2026100806" from the file names
        bool ok{false};
    };
    struct Chance {
        int low{0};              // the band the point is in; both 0 with no polygon over it
        int high{0};
        bool covered{false};     // false: outside every band, so under 5 % or no storm to speak of
        string text() const;     // "20-30%", "<5%", "none"
    };
    static Map parse(const string& zip);
    static Chance at(const Map&, int knotsIndex, double lat, double lon);   // knotsIndex 0 / 1 / 2 for 34 / 50 / 64 kt
    static bool inside(const Ring&, double lat, double lon);
    // "5-10%" -> 5 .. 10, "<5%" -> 0 .. 5, ">90%" -> 90 .. 100; false when it is none of those
    static bool parseBand(const string& label, int& low, int& high);
};

#endif  // UTILITYWINDPROBABILITY_H
