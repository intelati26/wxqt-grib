// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef UTILITYMETARCACHE_H
#define UTILITYMETARCACHE_H

#include <map>
#include <string>
#include <vector>
#include "obs/SurfaceStation.h"

using std::string;
using std::vector;

// The Aviation Weather Center's cache of every current METAR (aviationweather.gov/data/cache/metars.cache.csv.gz, one report per station,
// refreshed every minute). Pure text work, no network.
class UtilityMetarCache {
public:
    // one CSV line (quoted fields allowed) split into its fields
    static vector<string> splitCsv(const string& line);
    // the whole file (the header line first); stations without a position are left out
    static vector<SurfaceStation> parse(const string& csv);
    struct Name {
        string name;
        string state;
    };
    // the site names (aviationweather.gov/data/cache/stations.cache.json.gz), by station id and by ICAO id
    static std::map<string, Name> parseNames(const string& json);
    // "2026-10-08T12:15:00.000Z" -> seconds since 1970 (UTC), 0 when it is not a time
    static long parseTime(const string& iso);
};

#endif  // UTILITYMETARCACHE_H
