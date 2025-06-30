// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "radar/NexradLevel3WindBarbs.h"
#include "external/ExternalGeodeticCalculator.h"
#include "external/ExternalGlobalCoordinates.h"
#include "objects/WString.h"
#include "radar/NexradLevel3Common.h"
#include "radar/Projection.h"
#include "util/To.h"
#include "util/UtilityList.h"

vector<double> NexradLevel3WindBarbs::decodeAndPlot(const ProjectionNumbers& projectionNumbers, bool isGust, FileStorage& fileStorage) {
    vector<double> stormList;
    const auto arrWb = (!isGust) ? fileStorage.obsArrWb : fileStorage.obsArrWbGust;
    const auto degreeShift = 180.0;
    const auto arrowLength = 2.5;
    const auto arrowSpacing = 3.0;
    const auto barbLengthScaleFactor = 0.4;
    const auto arrowBend = 60.0;
    const auto nmScaleFactor = -1852.0;
    const auto barbLength = 15.0;
    const auto barbOffset = 0.0;
    for (const auto& windBarb : arrWb) {
        const auto metarArr = WString::split(windBarb, ":");
        auto angle = 0;
        auto length = 0;
        auto locXDbl = 0.0;
        auto locYDbl = 0.0;
        if (metarArr.size() > 3) {
            locXDbl = To::Double(metarArr[0]);
            locYDbl = To::Double(metarArr[1]);
            angle = To::Int(metarArr[2]);
            length = To::Int(metarArr[3]);
        }
        if (length > 4) {
            const auto degree = 0.0;
            const auto nm = 0.0;
            const auto degree2 = angle;
            const auto startLength = nm * nmScaleFactor;
            auto above50 = false;
            auto start = ExternalGlobalCoordinates{locXDbl, locYDbl};
            auto ec = ExternalGeodeticCalculator::calculateEndingGlobalCoordinates(start, degree, nm * nmScaleFactor * barbLengthScaleFactor);
            addAll(stormList, Projection::computeMercatorNumbersFromEc(ec, projectionNumbers));
            start = ExternalGlobalCoordinates{ec.getLatitude(), ec.getLongitude()};
            ec = ExternalGeodeticCalculator::calculateEndingGlobalCoordinates(start, degree2 + degreeShift, barbLength * nmScaleFactor * barbLengthScaleFactor);
            const auto end = ExternalGlobalCoordinates{ec.getLatitude(), ec.getLongitude()};
            addAll(stormList, Projection::computeMercatorNumbersFromEc(ec, projectionNumbers));
            auto barbCount = static_cast<int>(length / 10);
            auto halfBarb = false;
            auto oneHalfBarb = false;
            if (((length - barbCount * 10) > 4 && length > 10) || length < 10) {
                halfBarb = true;
            }
            if (length < 10) {
                oneHalfBarb = true;
            }
            if (length > 49) {
                above50 = true;
                barbCount -= 4;
            } else {
                above50 = false;
            }
            auto index = 0;
            if (above50) {
                // initial angled line;
                ec = ExternalGeodeticCalculator::calculateEndingGlobalCoordinates(
                    end,
                    degree2,
                    barbOffset + startLength + index * arrowSpacing * nmScaleFactor * barbLengthScaleFactor);
                addAll(stormList, NexradLevel3Common::drawLineFromEc(ec, projectionNumbers, degree2 - arrowBend * 2.0, startLength + arrowLength * nmScaleFactor));
                // perpendicular line from main barb;
                ec = ExternalGeodeticCalculator::calculateEndingGlobalCoordinates(
                    end,
                    degree2,
                    barbOffset + startLength + -1.0 * arrowSpacing * nmScaleFactor * barbLengthScaleFactor);
                addAll(stormList, NexradLevel3Common::drawLineFromEc(ec, projectionNumbers, degree2 - 90.0, startLength + 0.80 * arrowLength * nmScaleFactor));
                // connecting line parallel to main barb;
                ec = ExternalGeodeticCalculator::calculateEndingGlobalCoordinates(
                    end,
                    degree2,
                    barbOffset + startLength + index * arrowSpacing * nmScaleFactor * barbLengthScaleFactor);
                addAll(stormList, NexradLevel3Common::drawLineFromEc(ec, projectionNumbers, degree2 - 180.0, startLength + 0.5 * arrowLength * nmScaleFactor));
                index += 1;
            }
            for ([[maybe_unused]] auto i : range2(index, barbCount)) {
                ec = ExternalGeodeticCalculator::calculateEndingGlobalCoordinates(
                    end,
                    degree2,
                    barbOffset + startLength + index * arrowSpacing * nmScaleFactor * barbLengthScaleFactor);
                addAll(stormList, NexradLevel3Common::drawLineFromEc(ec, projectionNumbers, degree2 - arrowBend * 2.0, startLength + arrowLength * nmScaleFactor));
                index += 1;
            }
            auto halfBarbOffsetFudge = 0.0;
            if (oneHalfBarb) {
                halfBarbOffsetFudge = nmScaleFactor * 1.0;
            }
            if (halfBarb) {
                ec = ExternalGeodeticCalculator::calculateEndingGlobalCoordinates(
                    end,
                    degree2,
                    barbOffset + halfBarbOffsetFudge + startLength + index * arrowSpacing * nmScaleFactor * barbLengthScaleFactor);
                addAll(stormList, NexradLevel3Common::drawLineFromEc(ec, projectionNumbers, degree2 - arrowBend * 2.0, startLength + arrowLength / 2.0 * nmScaleFactor));
            }
        }
    }
    return stormList;
}
