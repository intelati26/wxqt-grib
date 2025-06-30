// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef UTILITYRADARMOSAIC_H
#define UTILITYRADARMOSAIC_H

#include <string>
#include <unordered_map>
#include <vector>
#include "objects/LatLon.h"

using std::string;
using std::unordered_map;
using std::vector;

class UtilityRadarMosaic {
public:
    static string getNearest(const LatLon&);
    static string get(const string&);
    static vector<string> getAnimation(const string&, const string&, int);
    static const string baseUrl;
    static const vector<string> sectors;
    static const unordered_map<string, LatLon> cityToLatLon;
};

#endif  // UTILITYRADARMOSAIC_H
