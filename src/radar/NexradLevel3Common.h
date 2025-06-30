// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef NEXRADLEVEL3COMMON_H
#define NEXRADLEVEL3COMMON_H

#include <vector>
#include "external/ExternalGlobalCoordinates.h"
#include "radar/ProjectionNumbers.h"

using std::vector;

class NexradLevel3Common {
public:
    static vector<double> drawLineFromEc(const ExternalGlobalCoordinates&, const ProjectionNumbers&, double, double);
    static vector<double> drawLine(const vector<double>&, const ProjectionNumbers&, const ExternalGlobalCoordinates&, double, double);
};

#endif  // NEXRADLEVEL3COMMON_H
