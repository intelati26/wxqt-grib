// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef UTILITYHDOB_H
#define UTILITYHDOB_H

#include <string>
#include <vector>

using std::string;
using std::vector;

// High Density Observation (HDOB) bulletins from the hurricane reconnaissance aircraft (WMO URNT15 / URPN15 / URPA15). The line layout is
// NHC's "High Density Observations (HDOB) Bulletins" specification (nhc.noaa.gov/pdf/HDOB-specification.pdf, Tables G-3 to G-5):
//   hhmmss LLLLH NNNNNH PPPP GGGGG XXXX sTTT sddd wwwSSS MMM KKK ppp FF
// Pure text work, no network.
class UtilityHdob {
public:
    static constexpr double missing = -9999.0;
    struct Ob {
        long seconds{0};            // UTC seconds since 1970-01-01
        double lat{0.0};
        double lon{0.0};            // east positive
        double staticPressure{missing};   // mb, at the aircraft
        double height{missing};     // geopotential height, m
        double surfacePressure{missing};  // mb, extrapolated to the surface (only when flying at 550 mb or lower altitude)
        double dValue{missing};     // m, the D-value instead (flying above the 550 mb level)
        double temperature{missing};   // C
        double dewPoint{missing};      // C
        double windDirection{missing}; // degrees
        double windSpeed{missing};     // kt, 30 s average at flight level
        double peakWind{missing};      // kt, maximum 10 s average at flight level
        double sfmrWind{missing};      // kt, maximum 10 s surface wind from the SFMR
        double rainRate{missing};      // mm / hr, from the SFMR
        int flags{0};                  // the two quality-control columns, as the two digits
    };
    struct Message {
        string mission;     // e.g. "AF302 1712A KATRINA"
        int number{0};
        string date;        // yyyymmdd of the first line
        vector<Ob> obs;
    };
    static vector<Message> parse(const string& text);
    static bool has(double value) { return value > missing + 1.0; }
    static string timeText(long seconds);   // "07 Oct 01:22Z"
};

#endif  // UTILITYHDOB_H
