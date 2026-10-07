// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef HURRICANEDATA_H
#define HURRICANEDATA_H

#include <map>
#include <string>
#include <vector>
#include "hurricane/UtilityAtcf.h"
#include "hurricane/UtilityHdob.h"

using std::string;
using std::vector;

// What the Atlantic hurricane screen downloads: the storm list (NHC CurrentStorms.json and the ATCF btk/ directory), one storm's best
// track, official forecast and model guidance (ATCF btk/, fst/, aid_public/) and the recent reconnaissance bulletins (the NHC recon
// archive). These block, so call them off the GUI thread.
class HurricaneData {
public:
    struct StormEntry {
        string id;              // "al092026"
        string label;           // for the pick list
        bool active{false};     // in CurrentStorms.json, or an invest with a file touched in the last two days
        string name;            // "Nine"
        string classification;  // "TD", "TS", "HU" ...
        int wind{-1};
        int pressure{-1};
        double lat{0.0};
        double lon{0.0};
        int movementDir{-1};
        int movementSpeed{-1};
        string lastUpdate;
        string discussionUrl;
        string advisoryUrl;
        string graphicsUrl;
    };
    struct StormData {
        string id;
        string name;
        vector<UtilityAtcf::Fix> best;
        UtilityAtcf::Track official;                // the newest official forecast
        vector<UtilityAtcf::Track> guidance;        // every other technique, its newest cycle
        std::map<string, string> longNames;         // technique code -> name
        string error;
    };
    struct ReconData {
        vector<UtilityHdob::Message> messages;      // oldest first
        string error;
    };
    static bool loadStormList(vector<StormEntry>& entries, string& error);
    static void loadStorm(const string& id, StormData& data);
    static void loadRecon(ReconData& data, int bulletins);
    static string idLabel(const string& id);        // "al092026" -> "AL09"
};

#endif  // HURRICANEDATA_H
