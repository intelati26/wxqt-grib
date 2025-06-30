// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef UTILITY_H
#define UTILITY_H

#include <string>
#include <vector>
#include "objects/LatLon.h"

using std::string;
using std::vector;

class Utility {
public:
    static string readPref(const string&, const string&);
    static int readPrefInt(const string&, int);
    static void writePref(const string&, const string&);
    static void writePrefInt(const string&, int);
    static vector<string> prefGetAllKeys();
    static string safeGet(const vector<string>&, size_t);
};

#endif  // UTILITY_H
