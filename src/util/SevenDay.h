// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef SEVENDAY_H
#define SEVENDAY_H

#include <string>
#include <vector>
#include "objects/LatLon.h"

using std::string;
using std::vector;

class SevenDay {
public:
    void process(const LatLon&);
    vector<string> icons;
    vector<string> detailedForecasts;

private:
    vector<string> shortForecasts;
};

#endif  // SEVENDAY_H
