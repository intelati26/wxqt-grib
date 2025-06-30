// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef UTILITYMODELESRLINTERFACE_H
#define UTILITYMODELESRLINTERFACE_H

#include <string>
#include <vector>

using std::string;
using std::vector;

class UtilityModelEsrlInterface {
public:
    static const vector<string> models;
    static const vector<string> sectorsHrrr;
    static const vector<string> sectorsHrrrAk;
    static const vector<string> sectorsHrrrSmoke;
    static const vector<string> modelHrrrParams;
    static const vector<string> modelHrrrLabels;
    static const vector<string> modelHrrrSmokeParams;
    static const vector<string> modelHrrrSmokeLabels;
    static const vector<string> sectorsRap;
    static const vector<string> modelRapParams;
    static const vector<string> modelRapLabels;
};

#endif  // UTILITYMODELESRLINTERFACE_H
