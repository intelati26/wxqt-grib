// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef HAZARDS_H
#define HAZARDS_H

#include <string>
#include "objects/LatLon.h"

using std::string;

class Hazards {
public:
    void process(const LatLon&);
    string data;
};

#endif  // HAZARDS_H
