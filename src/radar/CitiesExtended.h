// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef CITIESEXTENDED_H
#define CITIESEXTENDED_H

#include <vector>
#include "radar/CityExt.h"

using std::vector;

class CitiesExtended {
public:
    static void create();
    static vector<CityExt> cities;
};

#endif  // CITIESEXTENDED_H
