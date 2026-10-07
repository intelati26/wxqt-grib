// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef UTILITYSEASON_H
#define UTILITYSEASON_H

#include <string>
#include <vector>
#include "hurricane/UtilityAtcf.h"

using std::string;
using std::vector;

// Atlantic seasons from the HURDAT2 best-track database (NHC, 1851 on; the format is NHC's "hurdat2-format-atlantic") and, for the season under way,
// from the ATCF best-track files. Per storm: the first and last times, the peak wind, the lowest pressure and its ACE (accumulated cyclone energy:
// the sum of the squares of the maximum sustained wind in knots at 00, 06, 12 and 18 UTC while the system is a tropical or subtropical storm or a
// hurricane at 34 kt or more, divided by 10,000). A season counts the named storms (tropical or subtropical storm strength reached), the hurricanes
// (64 kt) and the major hurricanes (96 kt, category 3 and up). Pure text and number work, no network.
class UtilitySeason {
public:
    struct Storm {
        string id;               // AL092011
        string name;             // IRENE, UNNAMED
        int year{0};
        string first;            // yyyymmddhh of the first record
        string last;             // ... of the last
        int peakWind{0};         // kt
        int minPressure{0};      // mb, 0 when never given
        double ace{0.0};
        bool stormStrength{false};   // reached tropical or subtropical storm strength (counts as a named storm)
    };
    struct Season {
        int year{0};
        int cyclones{0};         // every system in the database, including depressions that never became storms
        int named{0};
        int hurricanes{0};
        int major{0};
        double ace{0.0};
    };
    // ACE of one record: wind squared / 10^4 when it is a synoptic hour and the status counts
    static double recordAce(int hourUtc, const string& status, int windKt);
    static vector<Storm> parseHurdat2(const string& text);
    static Storm fromBestTrack(const vector<UtilityAtcf::Fix>& best, const string& id);   // an ATCF best track in the same terms
    static vector<Season> seasons(const vector<Storm>&);                                // by year, ascending
    static string csv(const vector<Storm>&);                                           // the compact cache form
    static vector<Storm> fromCsv(const string&);
    static string categoryName(int peakWind);                                          // "Tropical depression", "Category 3" ...
    template <class T>
    static double mean(const vector<Season>& all, int firstYear, int lastYear, T Season::*field) {   // 0 when no season falls in the range
        double sum = 0.0;
        int n = 0;
        for (const auto& s : all) {
            if (s.year >= firstYear && s.year <= lastYear) {
                sum += static_cast<double>(s.*field);
                n++;
            }
        }
        return n == 0 ? 0.0 : sum / n;
    }
    // "hurdat2-1851-2025-092326.txt" (Atlantic, prefix "hurdat2-1851") or "hurdat2-nepac-1949-2025-092926.txt" (prefix "hurdat2-nepac-1949") from the directory listing
    static string newestHurdatFile(const string& listing, const string& prefix = "hurdat2-1851");
};

#endif  // UTILITYSEASON_H
