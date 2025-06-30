// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef COUNTYLABELS_H
#define COUNTYLABELS_H

#include <string>
#include <vector>
#include "objects/LatLon.h"

using std::string;
using std::vector;

class CountyLabels {
public:
    static void create();
    static vector<string> names;
    static vector<LatLon> location;
};

#endif  // COUNTYLABELS_H
