// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef UTILITYTIMESUNMOON_H
#define UTILITYTIMESUNMOON_H

#include <string>
#include <vector>
#include "objects/LatLon.h"
#include "objects/Site.h"

using std::string;
using std::vector;

class UtilityTimeSunMoon {
public:
    static string getSunTimes(const LatLon&);
    static string getMoonTimes(const LatLon&);
    static vector<double> getSunriseSunsetFromObs(const Site&);
};

#endif  // UTILITYTIMESUNMOON_H
