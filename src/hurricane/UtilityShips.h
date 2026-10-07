// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef UTILITYSHIPS_H
#define UTILITYSHIPS_H

#include <map>
#include <string>
#include <vector>

using std::string;
using std::vector;

// NHC's SHIPS text product (ftp.nhc.noaa.gov/atcf/stext/<yymmddhh><basin><number><yy>_ships.txt): the Statistical Hurricane Intensity Prediction
// Scheme's intensity forecasts (with and without land, and LGEM), the environment it was fed (shear, sea surface temperature, potential intensity,
// humidity, ocean heat content ...) at each forecast hour, and the rapid-intensification (RI) probabilities of SHIPS-RII and its relatives.
// The names are those printed in the file; "N/A", "LOST" and "DIS" (no value at that hour) are read as missing. Pure text work.
class UtilityShips {
public:
    static constexpr double missing = -9999.0;
    struct RiProbability {          // one line of "SHIPS Prob RI for 30kt/ 24hr RI threshold= 13% is 1.9 times climatological mean ( 6.8%)"
        int knots{0};
        int hours{0};
        double percent{missing};
        double climatology{missing};
        double times{missing};
    };
    struct Ships {
        string name;                // ISAIAS
        string id;                  // AL092026
        string cycle;               // yyyymmddhh
        vector<int> hours;          // the columns: 0, 6, 12 ...
        std::map<string, vector<double>> rows;   // "V (KT) LAND", "SHEAR (KT)", "SST (C)", "POT. INT. (KT)", ... one value per hour
        vector<string> stormType;   // TROP, EXTP ... per hour
        double preliminaryRi{missing};           // "PRELIM RI PROB (DV .GE. 35 KT IN 36 HR)", percent
        vector<RiProbability> riLines;
        vector<string> riThresholds;             // "20/12", "25/24" ... the columns of the matrix
        vector<std::pair<string, vector<double>>> riMatrix;   // SHIPS-RII, Logistic, Bayesian, Consensus, DTOPS, SDCON: percent per threshold
        bool ok{false};
        const vector<double> * row(const string& label) const {
            const auto found = rows.find(label);
            return found == rows.end() ? nullptr : &found->second;
        }
    };
    static Ships parse(const string& text);
    static bool has(double value) { return value > missing + 1.0; }
    // the file name of the newest cycle in a listing of stext/, for "al092026": e.g. 26100706AL0926_ships.txt; empty when there is none
    static string newestFile(const string& listing, const string& stormId);
};

#endif  // UTILITYSHIPS_H
