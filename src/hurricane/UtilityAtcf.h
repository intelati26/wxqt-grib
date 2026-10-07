// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef UTILITYATCF_H
#define UTILITYATCF_H

#include <array>
#include <map>
#include <string>
#include <vector>

using std::string;
using std::vector;

// Readers for NHC's ATCF text files (ftp.nhc.noaa.gov/atcf/): btk/ (best track so far), fst/ (the official forecast) and aid_public/
// (the model guidance, gunzipped). A row is comma separated: basin, storm number, time yyyymmddhh, technique number, technique,
// forecast hour, latitude (tenths of a degree, N / S), longitude (tenths, E / W), maximum wind (kt), pressure (mb), status ...
// The column meanings are those of the NRL ATCF database document that NHC's atcf/docs readme points to. Pure text work, no network.
class UtilityAtcf {
public:
    struct Fix {
        string time;       // the analysis / cycle time, yyyymmddhh
        int tau{0};        // forecast hour (0 for the best track)
        double lat{0.0};
        double lon{0.0};   // east positive
        int wind{-1};      // kt, -1 when not given
        int pressure{-1};  // mb, -1 when not given
        string status;     // TD, TS, HU, ...
        string name;       // best track rows carry the storm name
        std::array<std::array<int, 4>, 3> radii{};   // wind radii in nm for 34 / 50 / 64 kt, by quadrant NE, SE, SW, NW (0 = none)
    };
    struct Track {         // one technique's forecast from one cycle
        string tech;
        string cycle;      // yyyymmddhh
        vector<Fix> fixes; // by forecast hour
    };

    static vector<Fix> parseRows(const string& text, const string& onlyTech = "");   // every usable row, in file order
    // best track: one fix per time (the file repeats a time for each wind-radii threshold)
    static vector<Fix> bestTrack(const string& btkText);
    // the newest cycle of each technique, as tracks (forecast hour 0 onward, one fix per hour); at least two fixes
    static vector<Track> latestTracks(const string& text);
    // aid_public holds the technique name in column 5; fst files hold OFCL rows
    static std::map<string, string> parseTechList(const string& nhcTechlist);   // code -> long name

    enum class Group { Official, Consensus, Global, Hurricane, Ensemble, Simple, Other };
    static Group groupOf(const string& tech);
    static string groupName(Group);
    // NHC's forecast cone: the radius (nm) of the circle at a forecast hour (2026 Atlantic: two thirds of the 2021-2025 official track errors),
    // linear between the published hours (12, 24, 36, 48, 60, 72, 96, 120) and from 0 at hour 0; beyond 120 h the 120 h value
    static double coneRadiusNm(int hour);
    static int categoryOf(int windKt);                // 0 = TD or weaker, 1 = TS, 2..6 = hurricane category 1..5
    static string categoryName(int category);
    static double hoursBetween(const string& timeA, const string& timeB);   // yyyymmddhh strings, B - A
    static string formatTime(const string& yyyymmddhh);                    // "Oct 07 00Z"
};

#endif  // UTILITYATCF_H
