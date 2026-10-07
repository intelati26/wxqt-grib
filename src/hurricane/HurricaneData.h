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
#include "hurricane/UtilityEcmwfTracks.h"
#include "hurricane/UtilityPod.h"
#include "hurricane/UtilitySeason.h"
#include "hurricane/UtilityShips.h"
#include "hurricane/UtilityVdm.h"
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
    struct EnsembleSet {
        string label;                               // "AIFS ENS"
        string cycle;                               // yyyymmddhh of the run
        UtilityEcmwfTracks::Storm storm;            // every member of it
    };
    struct EnsembleData {
        vector<EnsembleSet> sets;                   // those of ECMWF's open data that have this storm
        string error;
    };
    struct ShipsData {
        UtilityShips::Ships ships;
        string error;
    };
    struct PodData {
        UtilityPod::Pod pod;
        string file;            // REPRPD.202610061722.txt
        string issued;          // yyyymmddhhmm from the file name
        string error;
    };
    struct VdmData {
        vector<UtilityVdm::Vdm> messages;     // oldest first, this storm's, the communications checks left out
        int filesRead{0};
        string error;
    };
    struct SeasonData {
        vector<UtilitySeason::Storm> history;      // HURDAT2, 1851 through the last finished season
        vector<UtilitySeason::Storm> current;      // this season so far, from the ATCF best tracks (numbers 1 to 89)
        vector<string> active;                     // ids of those whose file was touched in the last two days
        int currentYear{0};
        string hurdatFile;
        string error;
    };
    static bool loadStormList(vector<StormEntry>& entries, string& error);
    static void loadStorm(const string& id, StormData& data);
    static void loadRecon(ReconData& data, int bulletins);
    // ECMWF open data (CC BY 4.0): the member tracks of the IFS and AIFS ensembles and the unperturbed IFS / AIFS runs for this storm
    static void loadEnsembles(const string& nhcId, EnsembleData& data);
    // NOAA's GEFS members (ATCF AP01..AP30, control AC00) from the guidance already loaded, in the same shape as the ECMWF sets
    static bool gefsFromGuidance(const string& stormId, const vector<UtilityAtcf::Track>& guidance, EnsembleSet& set);
    static void loadShips(const string& nhcId, ShipsData& data);   // NHC's SHIPS text for the newest cycle
    static void loadPod(PodData& data);   // the newest Plan of the Day in the NHC recon archive
    static void loadVdm(const string& nhcId, VdmData& data);   // the recent vortex data messages for the storm (NHC recon archive, REPNT2)
    static void loadSeason(SeasonData& data);   // HURDAT2 (cached on disk) and this season's ATCF best tracks
    static string ecmwfId(const string& nhcId);     // "al092026" -> "09L"
    static string idLabel(const string& id);        // "al092026" -> "AL09"
};

#endif  // HURRICANEDATA_H
