// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef UTILITYPOD_H
#define UTILITYPOD_H

#include <string>
#include <vector>

using std::string;
using std::vector;

// NHC's Tropical Cyclone Plan of the Day (TCPOD, WMO NOUS42 KNHC, "REPRPD" in the recon archive): the reconnaissance flights planned or requested for
// the next 24 hours. Each flight has the lettered lines of the product (A fix time(s), B mission, C departure, D forecast position of the feature,
// E time on station, F altitude, G type of mission, H WRA (Weather Reconnaissance Area) activation, I remarks) in up to two columns per row.
// Pure text work, no network.
class UtilityPod {
public:
    struct Flight {
        string ordinal;       // "ONE"
        string aircraft;      // "TEAL 71"
        string fixTimes;      // A
        string mission;       // B  "AFXXX 0209A CYCLONE"
        string departure;     // C
        string position;      // D  "22.1N 94.1W" or "NA"
        string onStation;     // E
        string altitude;      // F
        string type;          // G  "FIX", "TAIL DOPPLER RADAR & FIX", "SYNOPTIC SURVEILLANCE"
        string wra;           // H
        string remarks;       // I
        bool hasPosition{false};
        double lat{0.0};
        double lon{0.0};      // east positive
    };
    struct Requirement {      // one numbered item under a basin: a suspect area or a storm
        string title;         // "SUSPECT AREA AL92 (SOUTHWESTERN GULF OF AMERICA)"
        vector<Flight> flights;
    };
    struct Pod {
        string number;        // "26-128"
        string valid;         // "07/1100Z TO 08/1100Z OCTOBER 2026"
        string issued;        // "NOUS42 KNHC 061722": the day and time of issue are in the header
        vector<Requirement> atlantic;
        vector<Requirement> pacific;
        vector<string> notes;           // the outlook for the succeeding day and the remarks, as written
        bool noAtlantic{false};         // "NEGATIVE RECONNAISSANCE REQUIREMENTS"
        bool noPacific{false};
        bool ok{false};
    };
    static Pod parse(const string& text);
    // "22.1N 94.1W" -> lat, lon
    static bool parsePosition(const string& text, double& lat, double& lon);
};

#endif  // UTILITYPOD_H
