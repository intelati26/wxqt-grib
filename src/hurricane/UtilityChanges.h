// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef UTILITYCHANGES_H
#define UTILITYCHANGES_H

#include <string>
#include <vector>

using std::string;
using std::vector;

// What a storm looked like at one advisory, kept so the next advisory can be compared with it ("what changed"). Pure text and number work.
class UtilityChanges {
public:
    struct Snapshot {
        string advisory;            // "003"
        string classification;      // TD, TS, HU ...
        int wind{-1};               // kt
        int pressure{-1};           // mb
        double lat{0.0};
        double lon{0.0};            // east positive
        int moveDir{-1};            // degrees
        int moveSpeed{-1};          // kt
        int forecastPeak{-1};       // kt, the highest wind in NHC's forecast
        int forecastPeakHour{-1};
        double ri30{-1.0};          // SHIPS-RII chance of a 30 kt rise in 24 h, percent
        bool valid() const { return !advisory.empty(); }
    };
    static string serialize(const Snapshot&);
    static Snapshot parse(const string&);
    // plain sentences about what is different, newest first-thing-that-matters first; empty when nothing moved
    static vector<string> describe(const Snapshot& before, const Snapshot& after);
    static double distanceKm(double lat1, double lon1, double lat2, double lon2);
    static double bearing(double lat1, double lon1, double lat2, double lon2);   // degrees from north
};

#endif  // UTILITYCHANGES_H
