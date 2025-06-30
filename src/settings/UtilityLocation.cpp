// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "UtilityLocation.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include "common/GlobalVariables.h"
#include "objects/Site.h"
#include "objects/WString.h"
#include "util/To.h"
#include "util/UtilityIO.h"

// latLon input is all positive numbers
LatLon UtilityLocation::getCenterOfPolygon(const vector<LatLon>& latLons) {
    auto x = 0.0;
    auto y = 0.0;
    for (const auto& latLon : latLons) {
        x += latLon.lat();
        y += latLon.lon();
    }
    const auto totalPoints = static_cast<double>(latLons.size());
    x /= totalPoints;
    y /= totalPoints;
    return {x, -1.0 * y};
}

string UtilityLocation::getNearest(const LatLon& latLon, const unordered_map<string, LatLon>& sectorToLatLon) {
    vector<Site> sites;
    for (const auto& m : sectorToLatLon) {
        sites.push_back(Site::fromLatLon(m.first, "", m.second, LatLon::distance(latLon, m.second)));
    }
    std::sort(
        sites.begin(),
        sites.end(),
        [] (const auto& s1, const auto& s2) { return s1.distance < s2.distance; });
    return sites[0].codeName;
}

string UtilityLocation::getNearestCity(const LatLon& latLon) {
    auto cityData = UtilityIO::rawFileToStringArray(GlobalVariables::resDir + "cityall.txt");
    unordered_map<string, LatLon> cityToLatlon;
    for (const auto& line : cityData) {
        auto items = WString::split(line, ",");
        if (items.size() > 3) {
            if (cityToLatlon.contains(items[0])) {
                std::cout << "UtilityLocation::getNearestCity duplicate: " << items[0] << std::endl;
            }
            if (To::Int(items[3]) > 1000) {
                cityToLatlon.insert({items[0], LatLon{items[1], items[2]}});
            }
        }
    }
    vector<Site> sites;
    for (const auto& m : cityToLatlon) {
        sites.push_back(Site::fromLatLon(m.first, "", m.second, LatLon::distance(latLon, m.second)));
    }
    std::sort(
        sites.begin(),
        sites.end(),
        [] (const auto& s1, const auto& s2) { return s1.distance < s2.distance; });

    const auto bearingToCity = LatLon::calculateDirection(latLon, cityToLatlon.at(sites[0].codeName));
    const auto distanceToCIty = static_cast<int>(std::round(LatLon::distance(latLon, cityToLatlon.at(sites[0].codeName))));
    return sites[0].codeName + " is " + To::string(distanceToCIty) + " miles to the " + bearingToCity;
}
