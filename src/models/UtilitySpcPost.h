// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef UTILITYSPCPOST_H
#define UTILITYSPCPOST_H

#include <map>
#include <string>
#include <utility>
#include <vector>

using std::string;
using std::vector;

// SPC Post (NOMADS spc_post) product viewer support: HREFCT thunderstorm
// probability, calibrated lightning density, HREF/GEFS calibrated severe
// (tor/wind/hail) probability, and GEFS-MLP severe probability.
//
// Unlike the operational RRFS (UtilityGrib), spc_post is served as one small
// single-field GRIB2 file per product/forecast-hour, and NOMADS exposes a
// plain Apache directory listing at every level - so runs and forecast hours
// are discovered by scraping that listing (mirroring the reference
// spc_slideshow.py script) rather than probed/derived from the clock.
// GRIB2 decode, reprojection and colorization are delegated to the GDAL
// command line tools, matching UtilityGrib's approach.
class UtilitySpcPost {
public:
    struct Product {
        string key;          // stable id, used for cache filenames
        string subdir;       // spc_post subdirectory: thunder / severe / gefs_mlp / ltgdensity
        string fieldRegex;   // matches the filename between "tCCz." and ".fFFF"; a capturing
                              // group (severe only) denotes the GEFS environment-init hour
        string label;        // shown in the combo box
    };

    struct Domain {
        string label;
        double west;
        double south;
        double east;
        double north;
    };

    static const vector<Product> products;
    static const vector<Domain> domains;

    static vector<string> productLabels();
    static vector<string> domainLabels();
    static vector<string> backgroundLabels();   // "None" / "RRFS 2m Temperature" / "RRFS Reflectivity"

    // Boundary overlay bit flags for render()'s boundaryFlags param -
    // independent toggles, not a mutually-exclusive choice (a single combo
    // couldn't express "State Lines + NWS CWAs", the user's actual typical
    // SPC-site setup, without enumerating every combination). Composited in
    // a fixed order regardless of which combination is set: geographic
    // reference lines (lakes, Canada/Mexico, highways, counties) first,
    // then CWAs, then state lines last so they stay visually on top.
    static constexpr int BoundaryState = 1;
    static constexpr int BoundaryCwa = 2;
    static constexpr int BoundaryCounty = 4;
    static constexpr int BoundaryHighway = 8;
    static constexpr int BoundaryLake = 16;
    static constexpr int BoundaryCanada = 32;
    static constexpr int BoundaryMexico = 64;
    static constexpr int BoundaryCity = 128;   // labels only, drawn last (on top of every line layer)
    // {label, flag} pairs for building an independent-toggle UI (checkboxes)
    static vector<std::pair<string, int>> boundaryOptions();

    // {display label, run id ("" == latest)}, newest first, "Latest" at index 0.
    // Queries the NOMADS directory listing for the most recent runs that actually
    // have data for this product (bounded lookback / result count).
    static vector<std::pair<string, string>> runOptions(int productIndex);

    // Forecast hours (zero padded, e.g. "004") actually present for productIndex
    // at the given run ("" == latest), newest listing scraped from the server.
    static vector<string> forecastHours(int productIndex, const string& runId);

    static bool gdalAvailable();

    // the gdaldem color-relief table used to render 0-100% probability fields;
    // exposed so the viewer can draw a matching legend
    static string probabilityColorMap();

    // Runs the full pipeline (download -> gdalwarp -> gdaldem -> optional boundary
    // overlay -> PNG) and returns the path to the rendered PNG, or "" on failure.
    // status is set to a human readable line (run/valid time on success, the
    // reason on failure). backgroundIndex: 0 = none, 1 = RRFS 2m temperature,
    // 2 = RRFS composite reflectivity, composited underneath the product's own
    // (partly transparent) shading via UtilityGrib::renderBackground, aligned
    // to the nearest RRFS run/lead for this product's actual valid time.
    // samplePath is set to a cached ".grid" sidecar (gdal XYZ dump of the raw
    // 0-100% probability field, same extent as the PNG) for a hover read-out;
    // "" if unavailable.
    static string render(int productIndex, int domainIndex, int boundaryFlags, int backgroundIndex,
                         const string& forecastHour, const string& runId, string& status,
                         string& samplePath);

private:
    struct RunFiles {
        string dateStr;    // YYYYMMDD
        string cycle;      // HH
        vector<string> hours;                  // sorted "fhr3" strings
        std::map<string, string> filenames;    // fhr3 -> filename
    };

    static string gdalBinDir();
    static string cacheDir();
    static vector<string> listDir(const string& url);        // href-scrapes an Apache index, cached briefly
    static vector<string> listDateDirs();                    // "spc_post.YYYYMMDD/" entries, newest first
    static bool resolveRun(int productIndex, const string& runId, RunFiles& out);
};

#endif  // UTILITYSPCPOST_H
