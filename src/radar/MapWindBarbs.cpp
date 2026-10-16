// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include <mutex>
#include "radar/MapWindBarbs.h"
#include "external/ExternalGeodeticCalculator.h"
#include "external/ExternalGlobalCoordinates.h"
#include "objects/WString.h"
#include "radar/MapLineCommon.h"
#include "radar/Projection.h"
#include "ui/WindBarb.h"
#include "util/To.h"
#include "util/UtilityList.h"

vector<double> MapWindBarbs::decodeAndPlot(const ProjectionNumbers& projectionNumbers, bool isGust, FileStorage& fileStorage) {
    vector<double> stormList;
    vector<string> arrWb;
    {
        const std::lock_guard<std::mutex> guard{*fileStorage.lock};   // a worker may be replacing the lists
        arrWb = (!isGust) ? fileStorage.obsArrWb : fileStorage.obsArrWbGust;
    }
    const auto staffMeters = 6.0 * 1852.0;   // the staff of a barb on the map: 6 nautical miles
    for (const auto& windBarb : arrWb) {
        const auto metarArr = WString::split(windBarb, ":");
        if (metarArr.size() <= 3) {
            continue;
        }
        const auto locX = To::Double(metarArr[0]);
        const auto locY = To::Double(metarArr[1]);
        const auto angle = To::Int(metarArr[2]);
        const auto knots = To::Int(metarArr[3]);
        if (knots <= 4) {
            continue;
        }
        // the program's one barb (ui/WindBarb): its lines and pennants, in staff lengths from the station, turned into distances and bearings on the earth
        const auto barb = WindBarb::shape(angle, knots, 1.0, locX < 0.0);
        const ExternalGlobalCoordinates start{locX, locY};
        const auto place = [&] (const QPointF& offset) {
            const auto meters = std::hypot(offset.x(), offset.y()) * staffMeters;
            const auto bearing = std::atan2(offset.x(), -offset.y()) * 180.0 / M_PI;
            return Projection::computeMercatorNumbersFromEc(meters < 1.0 ? start : ExternalGeodeticCalculator::calculateEndingGlobalCoordinates(start, bearing, meters), projectionNumbers);
        };
        const auto addLine = [&] (const QPointF& a, const QPointF& b) {
            addAll(stormList, place(a));
            addAll(stormList, place(b));
        };
        for (const auto& [a, b] : barb.lines) {
            addLine(a, b);
        }
        for (const auto& flag : barb.pennants) {
            for (int i = 0; i < flag.size(); i++) {
                addLine(flag[i], flag[(i + 1) % flag.size()]);
            }
        }
    }
    return stormList;
}
