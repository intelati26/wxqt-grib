// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef UTILITYWEATHERLAB_H
#define UTILITYWEATHERLAB_H

#include <string>
#include <string_view>
#include <vector>
#include "hurricane/UtilityEcmwfTracks.h"

// Google DeepMind's Weather Lab cyclone ensembles (deepmind.google.com/science/weatherlab): the FNV3 model (50 members) and the experimental WeatherNext 3 (WNV3, 64 members), as one
// CSV per run with a row per member, storm and 6 hourly step: position, minimum pressure, maximum sustained wind and the wind radii. The columns are found by their names (the file has
// grown columns before). The members are returned in the shape of the ECMWF ensembles so the map, the statistics and the strike counts take them as they are.
// Pure text work, no network. The data is under Google's terms of use, which ask for a credit wherever it is shown: see EnsembleStyle::deepMindCredit.
class UtilityWeatherLab {
public:
    // one storm per track id; storm.id is NHC's short id ("09L"), storm.name the ATCF id as the file has it ("AL092026"), storm.cycle yyyymmddhh; members are numbered by the file's sample column and are all type 4 (perturbed)
    static bool parse(const std::string& csv, std::vector<UtilityEcmwfTracks::Storm>& storms, std::string& error);
    // The large (1000 member) ensemble's cyclogenesis file: besides the storms that exist, tracks numbered "7", "9" ... are storms that some members form later. For each such
    // cluster, the first point of each member that forms it (hour, position, pressure, wind). The file is big (about 37 MB): it is read in place, line by line.
    struct GenesisPoint {
        int hour{0};
        double lat{UtilityEcmwfTracks::missing};
        double lon{UtilityEcmwfTracks::missing};
        double pressure{UtilityEcmwfTracks::missing};
        double wind{UtilityEcmwfTracks::missing};
    };
    struct Genesis {
        std::string track;                 // "9"
        std::vector<GenesisPoint> points;  // one per member that forms it
    };
    static bool parseGenesis(std::string_view csv, std::vector<Genesis>& clusters, int& members, std::string& cycle, std::string& error);   // members: the size of the ensemble
    // the large ensemble's file for a run (model folder FNV3_LARGE_ENSEMBLE), cycle yyyymmddhh
    static std::string genesisUrl(const std::string& cycle);
    // "AL092026" or "al092026" -> "09L"; an id of another basin or a short one is returned as it came
    static std::string shortId(const std::string& atcfId);
    // The models offered. folder is the name in the download address (and the start of the file name); label is what the screens call it. WeatherNext 2 (the page's "r2" tab) has no
    // entry yet: its folder name is not known (the download page is behind a Google sign-in), and adding it is one line here.
    struct Model {
        const char * folder;
        const char * label;
    };
    static const std::vector<Model>& models();
    // the file for a run: model folder "FNV3" or "WNV3", cycle yyyymmddhh ("2026100812")
    static std::string url(const std::string& model, const std::string& cycle);
};

#endif  // UTILITYWEATHERLAB_H
