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

    // numbers of a track, for ranking and listing storms
    static double ace(const Track&);          // the winds of 34 kt or more at 00, 06, 12 and 18 UTC while a tropical or subtropical storm or a hurricane, squared, / 10,000 (the same count as the season tables)
    static double lengthKm(const Track&);     // along the points, great circles between them
    static double durationDays(const Track&); // from the first record to the last
    // how a list of storms is ordered: each puts its own number first (the biggest first) and falls back on the peak wind, then the ACE, then the newer year
    enum class Sort { Strongest, LowestPressure, Ace, Longest, LongestLived, Newest, Oldest };
    static vector<string> sortNames();
    static double sortKey(const Track&, Sort);     // bigger is listed first
    static string sortNote(const Track&, Sort);    // what the sort looked at, for the row ("ACE 41.3", "4,120 km"); "" when the row shows it already
    static bool parseCoordinate(const string& field, double& degrees);   // "39.4N" "74.4W" -> 39.4, -74.4
};

#endif  // UTILITYHURDAT_H
