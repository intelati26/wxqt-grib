// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "buoys/BuoyData.h"
#include <cmath>
#include <ctime>
#include <mutex>
#include <numbers>
#include "util/UtilityIO.h"

namespace {
    double mercatorOf(double lat) {
        return 180.0 / std::numbers::pi * std::log(std::tan(std::numbers::pi / 4.0 + lat * std::numbers::pi / 360.0));
    }
}

bool BuoyData::loadLatest(std::vector<Marker>& markers, std::string& error) {
    static std::mutex mutex;
    static std::vector<Marker> cache;
    static std::time_t cachedAt = 0;
    {
        std::lock_guard lock{mutex};
        if (!cache.empty() && std::time(nullptr) - cachedAt < 600) {
            markers = cache;
            return true;
        }
    }
    const auto latest = UtilityBuoys::parseObservations(UtilityIO::downloadAsByteArray("https://www.ndbc.noaa.gov/data/latest_obs/latest_obs.txt").toStdString());
    if (latest.empty()) {
        error = "Could not read the latest NDBC observations.";
        return false;
    }
    const auto stations = UtilityBuoys::parseStations(UtilityIO::downloadAsByteArray("https://www.ndbc.noaa.gov/activestations.xml").toStdString());
    markers.clear();
    for (const auto& obs : latest) {
        if (!UtilityBuoys::has(obs.lat) || !UtilityBuoys::has(obs.lon) || std::abs(obs.lat) > 89.0) {
            continue;
        }
        Marker m;
        m.obs = obs;
        const auto found = stations.find(obs.id);
        if (found != stations.end()) {
            m.station = found->second;
        } else {
            m.station.id = obs.id;
            m.station.lat = obs.lat;
            m.station.lon = obs.lon;
        }
        m.mercator = mercatorOf(obs.lat);
        markers.push_back(std::move(m));
    }
    std::lock_guard lock{mutex};
    cache = markers;
    cachedAt = std::time(nullptr);
    return true;
}

bool BuoyData::loadHistory(const std::string& id, std::vector<UtilityBuoys::Obs>& series, std::string& error) {
    series = UtilityBuoys::parseObservations(UtilityIO::downloadAsByteArray("https://www.ndbc.noaa.gov/data/realtime2/" + id + ".txt").toStdString());
    if (series.empty()) {
        error = "NDBC has no recent data for station " + id + ".";
        return false;
    }
    return true;
}
