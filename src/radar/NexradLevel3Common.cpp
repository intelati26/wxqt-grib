// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "NexradLevel3Common.h"
#include "external/ExternalGeodeticCalculator.h"
#include "radar/Projection.h"

vector<double> NexradLevel3Common::drawLineFromEc(
    const ExternalGlobalCoordinates& startEc,
    const ProjectionNumbers& projectionNumbers,
    double startBearing,
    double distance
) {
    const auto start{ExternalGlobalCoordinates{startEc.getLatitude(), startEc.getLongitude()}};
    const auto startCoords{Projection::computeMercatorNumbersFromEc(startEc, projectionNumbers)};
    const auto ec{ExternalGeodeticCalculator::calculateEndingGlobalCoordinates(start, startBearing, distance)};
    const auto coordinates{Projection::computeMercatorNumbersFromEc(ec, projectionNumbers)};
    return {startCoords[0], startCoords[1], coordinates[0], coordinates[1]};
}

vector<double> NexradLevel3Common::drawLine(
    const vector<double>& startPoint,
    const ProjectionNumbers& projectionNumbers,
    const ExternalGlobalCoordinates& start,
    double startBearing,
    double distance
) {
    const auto ec{ExternalGeodeticCalculator::calculateEndingGlobalCoordinates(start, startBearing, distance)};
    const auto coordinates{Projection::computeMercatorNumbersFromEc(ec, projectionNumbers)};
    auto listToReturn{startPoint};
    listToReturn.push_back(coordinates[0]);
    listToReturn.push_back(coordinates[1]);
    return listToReturn;
}
