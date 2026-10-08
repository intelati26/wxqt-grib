// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef UTILITYDROPSONDE_H
#define UTILITYDROPSONDE_H

#include <string>
#include <vector>

using std::string;
using std::vector;

// The reconnaissance dropsonde report (WMO UZNT13 / UZPN13, "REPNT3" / "REPPN3" in the NHC recon archive) in the WMO TEMP DROP code (FM 37), as the
// OFCM National Hurricane Operations Plan (Appendix G) and NOAA AOML's "NHOP sonde drop format" page describe it: part A (XXAA) with the surface and the
// mandatory pressure levels, part B (XXBB) with the significant temperature levels and (21212) the significant wind levels, 31313 the launch time,
// 61616 the mission, 62626 the remarks (release and splash positions, mean boundary layer wind). Pure text work, no network.
class UtilityDropsonde {
public:
    static constexpr double missing = -9999.0;
    struct Level {
        double pressure{missing};     // mb
        double height{missing};       // m (the mandatory levels only)
        double temperature{missing};  // C
        double dewPoint{missing};     // C
        double windDirection{missing};   // degrees
        double windSpeed{missing};       // kt
    };
    struct Drop {
        long seconds{0};              // UTC seconds since 1970-01-01 of the launch (31313), else of the report
        double lat{missing};          // part A position
        double lon{missing};          // east positive
        double releaseLat{missing};   // the release point from the remarks (REL), when given
        double releaseLon{missing};
        double splashLat{missing};    // the splash point (SPG)
        double splashLon{missing};
        double surfacePressure{missing};   // the 99PPP group of part A, mb
        string mission;               // 61616: "NOAA9 01BBA SURV OB 32"
        string remarks;               // 62626 as written
        double mblDirection{missing}; // "MBL WND 20013": the mean boundary layer wind
        double mblSpeed{missing};
        bool windInKnots{true};
        vector<Level> levels;         // highest pressure (surface) first, the three sections merged
        bool ok{false};
        const Level * surface() const { return levels.empty() ? nullptr : &levels.front(); }
    };
    // `fileStamp` is yyyymmddhhmm from the archive file name: it supplies the month and year
    static Drop parse(const string& text, const string& fileStamp);
    static bool has(double value) { return value > missing + 1.0; }
    // the lowest pressure of the report (the surface pressure, else the lowest level kept) and the strongest wind of any level; missing when it has none
    static double minimumPressure(const Drop&);
    static double maxWind(const Drop&);
};

#endif  // UTILITYDROPSONDE_H
