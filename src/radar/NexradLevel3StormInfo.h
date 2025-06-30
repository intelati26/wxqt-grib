// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef NEXRADLEVEL3STORMINFO_H
#define NEXRADLEVEL3STORMINFO_H

#include <vector>
#include "external/ExternalGlobalCoordinates.h"
#include "objects/FileStorage.h"
#include "objects/LatLon.h"
#include "radar/ProjectionNumbers.h"

using std::vector;

class NexradLevel3StormInfo {
public:
    static void decode(const ProjectionNumbers&, FileStorage&);

private:
    static vector<double> drawTickMarks(const LatLon&, const ProjectionNumbers&, const ExternalGlobalCoordinates&, double, double);
};

#endif  // NEXRADLEVEL3STORMINFO_H
