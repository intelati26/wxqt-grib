// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef CITYEXT_H
#define CITYEXT_H

#include <string>

using std::string;

class CityExt {
public:
    CityExt(const string&, double, double);
    string name;
    double latitude;
    double longitude;
};

#endif  // CITYEXT_H
