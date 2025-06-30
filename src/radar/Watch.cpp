// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "Watch.h"
#include "external/ExternalPolygon.h"
#include "objects/PolygonWatch.h"
#include "objects/WString.h"
#include "radar/Projection.h"
#include "util/UtilityList.h"

vector<double> Watch::add(const ProjectionNumbers& projectionNumbers, PolygonType type) {
    vector<double> warningList;
    const auto prefToken = PolygonWatch::byType[type]->latLonList.getValue();
    if (!prefToken.empty()) {
        const auto polygons = WString::split(prefToken, ":");
        for (const auto& polygon : polygons) {
            const auto latLons = LatLon::parseStringToLatLons(polygon, -1.0, false);
            const auto b = latLonListToListOfDoubles(latLons, projectionNumbers);
            addAll(warningList, b);
        }
    }
    return warningList;
}

string Watch::show(const LatLon& latLon, PolygonType type) {
    vector<string> numberList;
    string watchLatLon;
    if (type == PolygonType::Watch) {
        watchLatLon = PolygonWatch::watchLatlonCombined.getValue();
        numberList = WString::split(PolygonWatch::byType[PolygonType::Watch]->numberList.getValue(), ":");
    } else {
        numberList = WString::split(PolygonWatch::byType[type]->numberList.getValue(), ":");
        watchLatLon = PolygonWatch::byType[type]->latLonList.getValue();
    }
    const auto latLonsFromString = WString::split(watchLatLon, ":");
    auto notFound = true;
    string text;
    for (auto z : range(latLonsFromString.size())) {
        const auto latLons = LatLon::parseStringToLatLons(latLonsFromString[z], 1.0, false);
        if (latLons.size() > 3) {
            const auto contains = ExternalPolygon::polygonContainsPoint(latLon, latLons);
            if (contains && notFound) {
                text = numberList[z];
                notFound = false;
            }
        }
    }
    return text;
}

vector<double> Watch::latLonListToListOfDoubles(const vector<LatLon>& latLons, const ProjectionNumbers& projectionNumbers) {
    vector<double> warningList;
    if (!latLons.empty()) {
        const auto startCoordinates = Projection::computeMercatorNumbersFromLatLon(latLons[0], projectionNumbers);
        warningList.push_back(startCoordinates[0]);
        warningList.push_back(startCoordinates[1]);
        for (auto index : range3(1, latLons.size(), 1)) {
            const auto coordinates = Projection::computeMercatorNumbersFromLatLon(latLons[index], projectionNumbers);
            warningList.push_back(coordinates[0]);
            warningList.push_back(coordinates[1]);
            warningList.push_back(coordinates[0]);
            warningList.push_back(coordinates[1]);
        }
        warningList.push_back(startCoordinates[0]);
        warningList.push_back(startCoordinates[1]);
    }
    return warningList;
}
