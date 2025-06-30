// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef WPCFRONTS_H
#define WPCFRONTS_H

#include <string>
#include <vector>
#include "objects/DownloadTimer.h"
#include "radar/Fronts.h"
#include "radar/PressureCenter.h"

using std::string;
using std::vector;

class WpcFronts {
public:
    static void get();
    static vector<PressureCenter> pressureCenters;
    static vector<Fronts> fronts;
    static const string separator;

private:
    static void addColdFrontTriangles(Fronts&, const vector<string>&);
    static void addWarmFrontSemicircles(Fronts&, const vector<string>&);
    static void addFrontDataStationaryWarm(Fronts&, const vector<string>&);
    static void addFrontDataTrof(Fronts&, const vector<string>&);
    static void addFrontData(Fronts&, const vector<string>&);
    static vector<double> parseLatLon(const string&);
    static DownloadTimer timer;
};

#endif  // WPCFRONTS_H
