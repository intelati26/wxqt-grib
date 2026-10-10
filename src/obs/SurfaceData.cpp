// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "obs/SurfaceData.h"
#include <cmath>
#include <ctime>
#include <map>
#include <mutex>
#include <set>
#include "obs/UtilityMadis.h"
#include "obs/UtilityMetarCache.h"
#include "util/UtilityGzip.h"
#include "util/UtilityIO.h"

bool SurfaceData::loadMetars(vector<SurfaceStation>& stations, string& error) {
    static std::mutex mutex;
    static vector<SurfaceStation> cache;
    static std::time_t cachedAt = 0;
    {
        std::lock_guard lock{mutex};
        if (!cache.empty() && std::time(nullptr) - cachedAt < 120) {
            stations = cache;
            return true;
        }
    }
    string csv;
    const auto packed = UtilityIO::downloadAsByteArray("https://aviationweather.gov/data/cache/metars.cache.csv.gz").toStdString();
    if (!UtilityGzip::gunzip(packed, csv)) {
        error = "Could not read the Aviation Weather Center's METAR file.";
        return false;
    }
    stations = UtilityMetarCache::parse(csv);
    if (stations.empty()) {
        error = "The Aviation Weather Center's METAR file had no reports.";
        return false;
    }
    // the site names, once a day is enough but it is only 350 kB
    static std::map<string, UtilityMetarCache::Name> names;
    {
        std::lock_guard lock{mutex};
        if (names.empty()) {
            string json;
            if (UtilityGzip::gunzip(UtilityIO::downloadAsByteArray("https://aviationweather.gov/data/cache/stations.cache.json.gz").toStdString(), json)) {
                names = UtilityMetarCache::parseNames(json);
            }
        }
        for (auto& s : stations) {
            const auto found = names.find(s.id);
            if (found != names.end()) {
                s.name = found->second.name;
                s.state = found->second.state;
            }
        }
        cache = stations;
        cachedAt = std::time(nullptr);
    }
    return true;
}

bool SurfaceData::loadMesonet(vector<SurfaceStation>& stations, string& error, const vector<SurfaceStation>& airports) {
    static std::mutex mutex;
    static vector<SurfaceStation> cache;
    static std::time_t cachedAt = 0;
    {
        std::lock_guard lock{mutex};
        if (!cache.empty() && std::time(nullptr) - cachedAt < 900) {
            stations = cache;
            return true;
        }
    }
    const auto files = UtilityMadis::listFiles(UtilityIO::downloadAsByteArray(mesonetUrl()).toStdString());
    if (files.empty()) {
        error = "MADIS did not list any mesonet files.";
        return false;
    }
    // the older file first, so a newer report of a station wins
    std::map<string, SurfaceStation> merged;
    int read = 0;
    for (size_t i = files.size() >= 2 ? files.size() - 2 : 0; i < files.size(); i++) {
        const auto packed = UtilityIO::downloadAsByteArray(mesonetUrl() + files[i]).toStdString();
        if (packed.size() < 100) {
            continue;
        }
        UtilityMadis::MesonetReader reader;
        UtilityGzip::gunzipStream(packed, 4u << 20, [&reader] (const char * data, size_t size) { return reader.push(data, size); });
        if (reader.failed()) {
            continue;
        }
        read++;
        for (auto& s : reader.stations()) {
            const auto key = s.network + "/" + s.id + "/" + std::to_string(std::lround(s.lat * 1000.0)) + "/" + std::to_string(std::lround(s.lon * 1000.0));
            auto found = merged.find(key);
            if (found == merged.end() || found->second.seconds < s.seconds) {
                merged[key] = std::move(s);
            }
        }
    }
    if (read == 0 || merged.empty()) {
        error = "Could not read the MADIS mesonet files.";
        return false;
    }
    // an airport already reports through its METAR: leave out the mesonet copy of it (the same id, or within about 1.5 km)
    std::set<string> airportIds;
    std::set<std::pair<int, int>> airportCells;   // 0.02 degree cells
    for (const auto& a : airports) {
        airportIds.insert(a.id);
        airportCells.insert({static_cast<int>(std::floor(a.lat / 0.02)), static_cast<int>(std::floor(a.lon / 0.02))});
    }
    stations.clear();
    for (auto& [key, s] : merged) {
        const int cy = static_cast<int>(std::floor(s.lat / 0.02));
        const int cx = static_cast<int>(std::floor(s.lon / 0.02));
        if (airportIds.count(s.id) != 0 || (s.network == "NonFedAWOS" && airportCells.count({cy, cx}) != 0)) {
            continue;
        }
        stations.push_back(std::move(s));
    }
    std::lock_guard lock{mutex};
    cache = stations;
    cachedAt = std::time(nullptr);
    return true;
}
