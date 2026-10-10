// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef UTILITYECMWFTRACKS_H
#define UTILITYECMWFTRACKS_H

#include <string>
#include <vector>

using std::string;
using std::vector;

// Reads ECMWF's open-data tropical cyclone track files (data.ecmwf.int/forecasts/<date>/<hh>z/<model>/0p25/<stream>/..-tf.bufr, BUFR edition 4,
// the compressed WMO template 3 16 082 "tropical cyclone ensemble forecast", master table 35): one message per storm, one subset per ensemble
// member (plus the unperturbed runs). Only that template is understood; anything else is reported as an error rather than guessed at.
// The element widths, scales and references are those of WMO BUFR Table B (version 35) for the descriptors the template uses.
// Pure byte work, no network. The data is CC BY 4.0, see licenses/ecmwf-notice.md.
class UtilityEcmwfTracks {
public:
    static constexpr double missing = -9999.0;
    struct Step {
        int hour{0};                  // forecast hour
        double lat{missing};          // the position of the minimum pressure (the track)
        double lon{missing};          // east positive
        double pressure{missing};     // mb at that position
        double windLat{missing};      // where the strongest 10 m wind is
        double windLon{missing};
        double wind{missing};         // kt, maximum 10 m wind
    };
    struct Member {
        int number{0};
        int type{0};                  // 0 = unperturbed high resolution, 1 = unperturbed low resolution (control), 2 / 3 = perturbed down / up, 4 = perturbed
        vector<Step> steps;           // forecast hour 0 first
    };
    struct Storm {
        string id;                    // "09L" (number and basin letter, as NHC's ids)
        string name;
        string cycle;                 // yyyymmddhh of the run
        vector<Member> members;
    };
    static bool parse(const string& bytes, vector<Storm>& storms, string& error);
    static bool has(double value) { return value > missing + 1.0; }
};

#endif  // UTILITYECMWFTRACKS_H
