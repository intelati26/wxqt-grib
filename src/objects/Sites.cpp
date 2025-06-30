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

string Sites::getNearest(const LatLon& latLon) {
    for (auto& site : sites) {
        site.distance = static_cast<int>(LatLon::distance(latLon, site.latLon));
    }
    std::sort(
        sites.begin(),
        sites.end(),
        [] (const Site &a, const Site &b) { return a.distance < b.distance; });
    return sites[0].codeName;
}

Site Sites::getNearestSite(const LatLon& latLon, int order) {
    for (auto& site : sites) {
        site.distance = static_cast<int>(LatLon::distance(latLon, site.latLon));
    }
    std::sort(
        sites.begin(),
        sites.end(),
        [] (const Site &a, const Site &b) { return a.distance < b.distance; });
    return sites[order];
}

vector<string> Sites::getNearestList(const LatLon& latLon, int count) {
    for (auto& site : sites) {
        site.distance = static_cast<int>(LatLon::distance(latLon, site.latLon));
    }
    std::sort(
        sites.begin(),
        sites.end(),
        [] (const Site &a, const Site &b) { return a.distance < b.distance; });
    auto sitesList = std::vector<Site>{sites.begin(), sites.begin() + count};
    vector<string> codeList;
    for (const auto& site : sitesList) {
        codeList.push_back(site.codeName);
    }
    return codeList;
}

int Sites::getNearestInMiles(const LatLon& latLon) {
    for (auto& site : sites) {
        site.distance = static_cast<int>(LatLon::distance(latLon, site.latLon));
    }
    std::sort(
        sites.begin(),
        sites.end(),
        [] (const Site &a, const Site &b) { return a.distance < b.distance; });
    return sites[0].distance;
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
