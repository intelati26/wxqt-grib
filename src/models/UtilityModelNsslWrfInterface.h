// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef UTILITYMODELNSSLWRFINTERFACE_H
#define UTILITYMODELNSSLWRFINTERFACE_H

#include <string>
#include <vector>

using std::string;
using std::vector;

class UtilityModelNsslWrfInterface {
public:
    static const vector<string> models;
    static const vector<string> paramsNsslWrf;
    static const vector<string> labelsNsslWrf;
    static const vector<string> paramsNsslFv3;
    static const vector<string> labelsNsslFv3;
    static const vector<string> paramsNsslHrrrv3;
    static const vector<string> labelsNsslHrrrv3;
    static const vector<string> sectorsLong;
    static const vector<string> sectors;
};

#endif  // UTILITYMODELNSSLWRFINTERFACE_H
