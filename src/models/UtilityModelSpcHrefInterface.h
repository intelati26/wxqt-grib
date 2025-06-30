// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef UTILITYMODELSPCHREFINTERFACE_H
#define UTILITYMODELSPCHREFINTERFACE_H

#include <string>
#include <vector>

using std::string;
using std::vector;

class UtilityModelSpcHrefInterface {
public:
    static const vector<string> sectorsLong;
    static const vector<string> sectors;
    static const vector<string> params;
    static const vector<string> labels;
};

#endif  // UTILITYMODELSPCHREFINTERFACE_H
