// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef RADARSITES_H
#define RADARSITES_H

#include <string>
#include <unordered_map>
#include <vector>
#include "objects/LatLon.h"
#include "objects/Sites.h"

using std::string;
using std::unordered_map;
using std::vector;

class RadarSites {
public:
    static void initialize();
    static vector<string> radars();
    static LatLon getLatLon(const string&);
    static vector<string> nexradRadarCodes();
    static vector<string> tdwrRadarCodes();
    static vector<string> nexradRadars();
    static vector<string> tdwrRadars();
    static string getName(const string&);
    static int findRadarIndex(const string&);
    static vector<Site> getNearest(const LatLon&, int, bool = true);
    static string getNearestCode(const LatLon&, bool = true);
    static Sites * sites;

private:
    static const unordered_map<string, string> name;
    static const unordered_map<string, string> lat;
    static const unordered_map<string, string> lon;
};

#endif  // RADARSITES_H
