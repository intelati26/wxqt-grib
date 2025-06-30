// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "Projection.h"
#include <numbers>
#include <cmath>

vector<double> Projection::computeMercatorNumbers(double lat, double lon, const ProjectionNumbers& projectionNumbers) {
    const auto pnX = projectionNumbers.getLatLon().lat();
    const auto pnY = projectionNumbers.getLatLon().lon();
    const auto test1 = 180.0 / std::numbers::pi * log(tan(std::numbers::pi / 4.0 + lat * (std::numbers::pi / 180.0) / 2.0));
    const auto test2 = 180.0 / std::numbers::pi * log(tan(std::numbers::pi / 4.0 + pnX * (std::numbers::pi / 180.0) / 2.0));
    const auto y = -1.0 * (test1 - test2) * projectionNumbers.getOneDegreeScaleFactor() + projectionNumbers.yCenter;
    const auto x = -1.0 * (lon - pnY) * projectionNumbers.getOneDegreeScaleFactor() + projectionNumbers.xCenter;
    return {x, y};
}

vector<double> Projection::computeMercatorNumbersFromEc(const ExternalGlobalCoordinates& ec, const ProjectionNumbers& projectionNumbers) {
    return computeMercatorNumbers(ec.getLatitude(), ec.getLongitude() * -1.0, projectionNumbers);
}

vector<double> Projection::computeMercatorNumbersFromLatLon(const LatLon& latLon, const ProjectionNumbers& projectionNumbers) {
    return computeMercatorNumbers(latLon.lat(), latLon.lon(), projectionNumbers);
}
