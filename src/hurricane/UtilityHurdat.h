// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef UTILITYHURDAT_H
#define UTILITYHURDAT_H

#include <string>
#include <vector>

using std::string;
using std::vector;

// The tracks of NHC's HURDAT2 best-track database (the Atlantic from 1851, the northeast Pacific from 1949): every storm with its 6-hourly (and landfall /
// peak) positions, winds and pressures. The format is NHC's "hurdat2-format". Pure text work, no network.
class UtilityHurdat {
public:
    struct Point {
        string time;          // yyyymmddhh (the minutes of a landfall or peak record are dropped)
        string status;        // TD, TS, HU, EX, SD, SS, LO, WV, DB
        double lat{0.0};
        double lon{0.0};      // east positive
        int wind{0};          // kt, 0 when not given
        int pressure{0};      // mb, 0 when not given
    };
    struct Track {
        string id;            // AL092005
        string name;          // KATRINA, UNNAMED
        int year{0};
        vector<Point> points;
        int peakWind{0};
        int minPressure{0};
        bool stormStrength{false};   // reached tropical or subtropical storm strength
    };
    static vector<Track> parse(const string& text);
    static bool parseCoordinate(const string& field, double& degrees);   // "39.4N" "74.4W" -> 39.4, -74.4
};

#endif  // UTILITYHURDAT_H
