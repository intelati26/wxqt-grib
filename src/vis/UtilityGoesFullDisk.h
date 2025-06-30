// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef UTILITYGOESFULLDISK_H
#define UTILITYGOESFULLDISK_H

#include <string>
#include <unordered_map>
#include <vector>
#include "objects/LatLon.h"

using std::string;
using std::unordered_map;
using std::vector;

class UtilityGoesFullDisk {
public:
    static vector<string> getAnimation(const string&, const string&, size_t);
    static bool canAnimate(const string&);
    static const vector<string> labels;
    static const vector<string> urls;
};

#endif  // UTILITYGOESFULLDISK_H
