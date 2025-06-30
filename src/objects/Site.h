// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef SITE_H
#define SITE_H

#include <string>
#include "objects/LatLon.h"

using std::string;

class Site {
public:
    Site(const string&, const string&, const string&, const string&, bool);
    static Site fromLatLon(const string&, const string&, LatLon, double = 0.0);
    string codeName;
    string fullName;
    string codeAndName;
    LatLon latLon;
    int distance;
};

#endif  // SITE_H
