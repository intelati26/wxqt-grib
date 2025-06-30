// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef OBJECTNHC_H
#define OBJECTNHC_H

#include <string>
#include <vector>
#include "nhc/NhcStormDetails.h"

using std::string;
using std::vector;

class ObjectNhc {
public:
    void getTextData();
    void showTextData();
    vector<NhcStormDetails> stormDataList;

private:
    vector<string> ids;
    vector<string> binNumbers;
    vector<string> names;
    vector<string> classifications;
    vector<string> intensities;
    vector<string> pressures;
    vector<string> latitudes;
    vector<string> longitudes;
    vector<string> movementDirs;
    vector<string> movementSpeeds;
    vector<string> lastUpdates;
    vector<string> statusList;
    vector<string> publicAdvisories;
    vector<string> publicAdvisoriesNumbers;
    vector<string> forecastAdvisories;
    vector<string> forecastDiscussions;
    vector<string> windSpeedProbabilities;
};

#endif  // OBJECTNHC_H
