// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef WARNINGS_H
#define WARNINGS_H

#include <string>
#include <vector>
#include "objects/LatLon.h"
#include "radar/PolygonType.h"
#include "radar/ProjectionNumbers.h"

using std::string;
using std::vector;

class Warnings {
public:
    static vector<double> add(const ProjectionNumbers&, PolygonType);
    static int getCount(PolygonType);
    static string show(const LatLon&);
};

#endif  // WARNINGS_H
