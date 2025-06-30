// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef UTILITYMODELSPCHRRRINTERFACE_H
#define UTILITYMODELSPCHRRRINTERFACE_H

#include <string>
#include <vector>

using std::string;
using std::vector;

class UtilityModelSpcHrrrInterface {
public:
    static const vector<string> sectors;
    static const vector<string> sectorCodes;
    static const vector<string> params;
    static const vector<string> labels;
};

#endif  // UTILITYMODELSPCHRRRINTERFACE_H
