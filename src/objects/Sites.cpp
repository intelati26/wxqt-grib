// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "Sites.h"
#include <algorithm>
#include <set>
#include "util/UtilitySet.h"

using std::set;

Sites::Sites(const unordered_map<string, string>& nameDict, const unordered_map<string, string>& latDict, const unordered_map<string, string>& lonDict, bool lonReversed)
    : nameDict{nameDict}
    , latDict{latDict}
    , lonDict{lonDict}
{
    checkValidityMaps();
    for (const auto& m : nameDict) {
        sites.push_back(Site(m.first, m.second, latDict.at(m.first), lonDict.at(m.first), lonReversed));
        byCode[m.first] = std::make_unique<Site>(m.first, m.second, latDict.at(m.first), lonDict.at(m.first), lonReversed);
    }
    sort(
        sites.begin(),
        sites.end(),
        [] (const Site &a, const Site &b) { return a.fullName < b.fullName; });
    for (const auto& site : sites) {
        codeList.push_back(site.codeName);
        nameList.push_back(site.codeName + ": " +site.fullName);
    }
}

namespace {
    // (distance, index into `sites`) for every site, nearest first. The list of sites is shared by every thread (the radar's
    // observation job walks it while the UI asks for the nearest site), so it is never sorted or written after construction.
    vector<std::pair<int, size_t>> byDistance(const vector<Site>& sites, const LatLon& latLon) {
        vector<std::pair<int, size_t>> distances;
        distances.reserve(sites.size());
        for (size_t i = 0; i < sites.size(); i += 1) {
            distances.emplace_back(static_cast<int>(LatLon::distance(latLon, sites[i].latLon)), i);
        }
        std::sort(distances.begin(), distances.end());
        return distances;
    }
}

string Sites::getNearest(const LatLon& latLon) {
    return sites[byDistance(sites, latLon)[0].second].codeName;
}

Site Sites::getNearestSite(const LatLon& latLon, int order) {
    const auto distances = byDistance(sites, latLon);
    const auto position = std::min(static_cast<size_t>(std::max(0, order)), distances.size() - 1);
    Site site = sites[distances[position].second];
    site.distance = distances[position].first;
    return site;
}

vector<string> Sites::getNearestList(const LatLon& latLon, int count) {
    const auto distances = byDistance(sites, latLon);
    vector<string> codeList;
    for (size_t i = 0; i < distances.size() && i < static_cast<size_t>(std::max(0, count)); i += 1) {
        codeList.push_back(sites[distances[i].second].codeName);
    }
    return codeList;
}

int Sites::getNearestInMiles(const LatLon& latLon) {
    return byDistance(sites, latLon)[0].first;
}

void Sites::checkValidityMaps() {
    set<string> k1;
    for (auto kv : nameDict) {
        k1.insert(kv.first);
    }
    set<string> k2;
    for (auto kv : latDict) {
        k2.insert(kv.first);
    }
    set<string> k3;
    for (auto kv : lonDict) {
        k3.insert(kv.first);
    }
    UtilitySet::checkEquality(k1, k2, "name", "lat");
    UtilitySet::checkEquality(k1, k3, "name", "lon");
}
