// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef UTILITYRTMA_H
#define UTILITYRTMA_H

#include <string>
#include <unordered_map>
#include <vector>
#include "objects/LatLon.h"

using std::string;
using std::vector;
using std::unordered_map;

class UtilityRtma {
public:
    static string getNearest(const LatLon&);
    static string get(const string&);
    static vector<string> getAnimation(const string&, const string&, int);
    static const string baseUrl;
    static vector<string> getTimes();
    static string getUrl(int, int, string);
    static string getUrlForHomeScreen(string);
    static const vector<string> codes;
    static const vector<string> labels;
    static const vector<string> sectors;
    static const unordered_map<string, LatLon> sectorToLatLon;
};

#endif  // UTILITYRTMA_H
