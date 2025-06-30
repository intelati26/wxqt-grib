// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef UTILITYGOES_H
#define UTILITYGOES_H

#include <string>
#include <unordered_map>
#include <vector>
#include "objects/LatLon.h"

using std::string;
using std::unordered_map;
using std::vector;

class UtilityGoes {
public:
    static string getNearest(const LatLon&);
    static string getImageFileName(const string&);
    static string getImageGoesFloater(const string&, const string&);
    static string getImage(const string&, const string&);
    static vector<string> getAnimation(const string&, const string&, size_t);
    static vector<string> getAnimationGoesFloater(const string&, const string&, size_t);
    static const unordered_map<string, LatLon> sectorToLatLon;
    static const vector<string> sectorsInGoes17;
    static const unordered_map<string, string> sizeMap;
    static const vector<string> sectors;
    static const vector<string> labels;
    static const vector<string> productCodes;
};

#endif  // UTILITYGOES_H
