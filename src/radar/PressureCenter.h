// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef PRESSURECENTER_H
#define PRESSURECENTER_H

#include <string>
#include "radar/PressureCenterTypeEnum.h"

using std::string;

class PressureCenter {
public:
    PressureCenter(PressureCenterTypeEnum, const string&, double, double);
    PressureCenterTypeEnum centerType;
    string pressureInMb;
    double lat;
    double lon;
};

#endif  // PRESSURECENTER_H
