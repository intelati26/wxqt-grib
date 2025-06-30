// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "NexradLevel3StormInfo.h"
#include <string>
#include "external/ExternalGeodeticCalculator.h"
#include "objects/WString.h"
#include "radar/NexradLevel3Common.h"
#include "radar/NexradLevel3TextProduct.h"
#include "radar/Projection.h"
#include "util/To.h"
#include "util/UtilityList.h"
#include "util/UtilityString.h"

using std::string;

void NexradLevel3StormInfo::decode(const ProjectionNumbers& projectionNumbers, FileStorage& fileStorage) {
    const auto rawData = NexradLevel3TextProduct::download("STI", projectionNumbers.getRadarSite());
    vector<double> stormList;
    if (rawData.size() > 10) {
        const auto position = UtilityString::parseColumn(rawData, "AZ/RAN(.*?)V");
        const auto motion = UtilityString::parseColumn(rawData, "MVT(.*?)V");
        const auto posnStr = WString::replace(WString::join(position, "") , "/", " ");
        auto motionStr = WString::replace(WString::join(motion, "") , "/", " ");
        motionStr = WString::replace(motionStr, "NEW", "  0  0  ");
        const string reg = "(\\d+) ";
        const auto posnNumbers = UtilityString::parseColumn(posnStr, reg);
        const auto motNumbers = UtilityString::parseColumn(motionStr, reg);
        const auto sti15IncrLen = 0.40;
        if ((posnNumbers.size() == motNumbers.size()) && posnNumbers.size() > 1) {
            const auto degreeShift = 180;
            const auto arrowLength = 2.0;
            const auto arrowBend = 20.0;
            for (auto index : range3(0, posnNumbers.size() - 2, 2)) {
                const auto degree = To::Int(posnNumbers[index]);
                const auto nm = To::Int(posnNumbers[index + 1]);
                const auto degree2 = To::Double(motNumbers[index]);
                const auto nm2 = To::Int(motNumbers[index + 1]);
                auto start = ExternalGlobalCoordinates::withPn(projectionNumbers, true);
                auto ec = ExternalGeodeticCalculator::calculateEndingGlobalCoordinates(start, degree, nm * 1852.0);
                const auto coord = Projection::computeMercatorNumbersFromEc(ec, projectionNumbers);
                stormList.push_back(coord[0]);
                stormList.push_back(coord[1]);
                start = ExternalGlobalCoordinates{ec.getLatitude(), ec.getLongitude()};
                ec = ExternalGeodeticCalculator::calculateEndingGlobalCoordinates(start, degree2 + degreeShift, nm2 * 1852.0);
                const auto tmpCoords = Projection::computeMercatorNumbersFromEc(ec, projectionNumbers);
                stormList.push_back(tmpCoords[0]);
                stormList.push_back(tmpCoords[1]);
                vector<ExternalGlobalCoordinates> ecArr;
                vector<LatLon> latLons;
                for (auto z : {0, 1, 2, 3}) {
                    ecArr.push_back(ExternalGeodeticCalculator::calculateEndingGlobalCoordinates(start, degree2 + degreeShift, nm2 * 1852.0 * z * 0.25));
                    latLons.push_back(LatLon::fromList(Projection::computeMercatorNumbersFromEc(ecArr[z], projectionNumbers)));
                }
                const auto endPoint = tmpCoords;
                if (nm2 > 0) {
                    start = ExternalGlobalCoordinates{ec.getLatitude(), ec.getLongitude()};
                    for (auto b : {degree2 + arrowBend, degree2 - arrowBend}) {
                        addAll(stormList, NexradLevel3Common::drawLine(endPoint, projectionNumbers, start, b, arrowLength * 1852.0));
                    }
                    // 15,30,45 min ticks;
                    const auto stormTrackTickMarkAngleOff90 = 30.0;
                    for (auto index2 : {0, 1, 2, 3}) {
                        const vector<double> angles{
                            degree2 - (90.0 + stormTrackTickMarkAngleOff90),
                            degree2 + (90.0 - stormTrackTickMarkAngleOff90),
                            degree2 - (90.0 - stormTrackTickMarkAngleOff90),
                            degree2 + (90.0 + stormTrackTickMarkAngleOff90)
                        };
                        for (auto d : angles) {
                            addAll(stormList, drawTickMarks(latLons[index2], projectionNumbers, ecArr[index2], d, arrowLength * 1852.0 * sti15IncrLen));
                        }
                    }
                }
            }
        }
    }
    fileStorage.stiData.clear();
    addAll(fileStorage.stiData, stormList);
}

vector<double> NexradLevel3StormInfo::drawTickMarks(
    const LatLon& startPoint,
    const ProjectionNumbers& projectionNumbers,
    const ExternalGlobalCoordinates& ecArr,
    double startBearing,
    double distance
) {
    vector<double> items{startPoint.lat(), startPoint.lon()};
    const auto start = ExternalGlobalCoordinates{ecArr.getLatitude(), ecArr.getLongitude()};
    const auto ec = ExternalGeodeticCalculator::calculateEndingGlobalCoordinates(start, startBearing, distance);
    addAll(items, Projection::computeMercatorNumbersFromEc(ec, projectionNumbers));
    return items;
}

// TODO FIXME wxgtkmm below,  use this
// vector<double> NexradLevel3StormInfo::drawTickMarks(const LatLon& startPoint, const ProjectionNumbers& pn, const ExternalGlobalCoordinates& ecArr, double startBearing, double distance) {
//     const auto start = ExternalGlobalCoordinates::withEc(ecArr, false);
//     const auto ec = ExternalGeodeticCalculator::calculateEndingGlobalCoordinates(start, startBearing, distance);
//     const auto coordinates = NexradLevel3Common::computeMercatorNumbersFromEc(ec, pn);
//     return {startPoint.lat(), startPoint.lon(), coordinates[0], coordinates[1]};
// }
