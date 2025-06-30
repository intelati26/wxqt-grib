// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "Warnings.h"
#include "external/ExternalPolygon.h"
#include "objects/ObjectWarning.h"
#include "objects/PolygonWarning.h"
#include "radar/Watch.h"
#include "util/UtilityList.h"

vector<double> Warnings::add(const ProjectionNumbers& projectionNumbers, PolygonType type) {
    const auto html = PolygonWarning::byType[type]->getData();
    const auto warnings = ObjectWarning::parseJson(html);
    vector<double> warningList;
    for (const auto& w : warnings) {
        if (type == Sps || type == Smw || w.isCurrent) {
            const auto latLons = w.getPolygonAsLatLons(-1);
            addAll(warningList, Watch::latLonListToListOfDoubles(latLons, projectionNumbers));
        }
    }
    return warningList;
}

int Warnings::getCount(PolygonType type) {
    const auto html = PolygonWarning::byType[type]->getData();
    const auto warningList = ObjectWarning::parseJson(html);
    auto i = 0;
    for (const auto& s : warningList) {
        if (s.isCurrent) {
            i += 1;
        }
    }
    return i;
}

string Warnings::show(const LatLon& latLon) {
    string warningChunk;
    for (const auto& data : PolygonWarning::polygonList) {
        const auto it = PolygonWarning::byType[data].get();
        if (it->isEnabled) {
            warningChunk += it->getData();
        }
    }
    const auto warnings = ObjectWarning::parseJson(warningChunk);
    string urlToOpen;
    auto notFound = true;
    for (const auto& w : warnings) {
        const auto latLons = w.getPolygonAsLatLons(1);
        if (!latLons.empty()) {
            const auto contains = ExternalPolygon::polygonContainsPoint(latLon, latLons);
            if (contains && notFound) {
                urlToOpen = w.getUrl();
                notFound = false;
            }
        }
    }
    return urlToOpen;
}
