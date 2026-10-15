// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef UTILITYSARWINDS_H
#define UTILITYSARWINDS_H

#include <string>
#include <vector>

using std::string;
using std::vector;

// NOAA STAR's synthetic aperture radar (SAR) tropical cyclone winds (www.star.nesdis.noaa.gov/socd/mecb/sar/sarwinds_tropical.php): the schedule of the passes still to come (Sentinel-1,
// RADARSAT Constellation) and, for a storm, the scenes made of it with their pictures and the winds measured in them. Pure parsing here, no network.
class UtilitySarWinds {
public:
    struct Pass {            // a row of the "Upcoming Acquisitions Schedule"
        string name;         // SIMON
        string id;           // EP202026
        string time;         // 2026-10-14 12:55 (UTC)
        string sensor;       // RCM-2, S1C
    };
    struct Schedule {
        vector<Pass> passes;
        string updated;      // "2026-10-14 13:00:17 UTC"
    };
    struct Scene {
        string sensor;       // "S1D - VH"
        string time;         // "2026-10-13  1:05:00 UTC"
        string wind;         // the wind speed picture
        string nrcs;         // the normalized radar cross section
        string footprint;    // the map of where the scene is
        string contours;     // wind speed contours
        string radialQuads;  // the radial wind profile by quadrant
        string radialAll;    // and of all of them
        string info;         // the text file of the scene
        vector<string> measurements;   // "Quadrant 1 NE VMax (kts): 89.16", "RMax (nmi): 5.00 - 7.00" ...
        double lat{-999.0}, lon{-999.0};   // the storm centre found in the scene
    };
    static const string base;                                             // https://www.star.nesdis.noaa.gov/socd/mecb/sar/
    static string stormUrl(int year, const string& id, const string& name);   // the page of a storm: EP202026, SIMON
    static Schedule parseSchedule(const string& html);
    static vector<Scene> parseStorm(const string& html);                   // newest first, as the page has them
    static string storm(const string& id, const string& name, int year, vector<Scene>& out);   // fetches; the error text, or empty
    static string fetchSchedule(Schedule& out);
};

#endif  // UTILITYSARWINDS_H
