// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "NexradLevel3Tvs.h"
#include <vector>
#include "external/ExternalGeodeticCalculator.h"
#include "objects/WString.h"
#include "radar/NexradLevel3TextProduct.h"
#include "util/UtilityList.h"
#include "util/UtilityString.h"
#include "util/To.h"

void NexradLevel3Tvs::decode(const ProjectionNumbers& projectionNumbers, FileStorage& fileStorage) {
    const auto rawData = NexradLevel3TextProduct::download("TVS", projectionNumbers.getRadarSite());
    vector<double> stormList;
    const auto tvs{UtilityString::parseColumn(rawData, "P  TVS(.{20})")};
    for (auto index : range(tvs.size())) {
        const auto stringData{UtilityString::parse(tvs[index], ".{9}(.{7})")};
        const auto items{WString::split(stringData, "/")};
        const auto degStr{WString::replace(items[0], " ", "")};
        const auto nmStr{WString::replace(items[1], " ", "")};
        const auto degree{To::Int(degStr)};
        const auto nm{To::Int(nmStr)};
        const auto start{ExternalGlobalCoordinates::withPn(projectionNumbers, true)};
        const auto ec{ExternalGeodeticCalculator::calculateEndingGlobalCoordinates(start, degree, nm * 1852.0)};
        stormList.push_back(ec.getLatitude());
        stormList.push_back(ec.getLongitude() * -1.0);
    }
    fileStorage.tvsData.clear();
    addAll(fileStorage.tvsData, stormList);
}
