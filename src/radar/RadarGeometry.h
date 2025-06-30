// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef RADARGEOMETRY_H
#define RADARGEOMETRY_H

#include <unordered_map>
#include "radar/RadarGeometryTypeEnum.h"
#include "radar/RadarGeomInfo.h"

using std::unordered_map;

class RadarGeometry {
public:
    static void initStatic();
    static unordered_map<RadarGeometryTypeEnum, RadarGeomInfo> dataByType;
};

#endif  // RADARGEOMETRY_H
