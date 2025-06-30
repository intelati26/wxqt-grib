// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef PROJECTION_H
#define PROJECTION_H

#include <vector>
#include "external/ExternalGlobalCoordinates.h"
#include "objects/LatLon.h"
#include "radar/ProjectionNumbers.h"

using std::vector;

class Projection {
public:
    static vector<double> computeMercatorNumbers(double, double, const ProjectionNumbers&);
    static vector<double> computeMercatorNumbersFromEc(const ExternalGlobalCoordinates&, const ProjectionNumbers&);
    static vector<double> computeMercatorNumbersFromLatLon(const LatLon&, const ProjectionNumbers&);
};

#endif  // PROJECTION_H
