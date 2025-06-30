// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef STORMREPORT_H
#define STORMREPORT_H

#include <string>
#include "objects/LatLon.h"

using std::string;

class StormReport {
public:
    StormReport(const string&, const string&, const string&, const string&, const string&, const string&, const string&, const string&, const string&, const string&);
    string text;
    string lat;
    string lon;
    string time;
    string magnitude;
    string address;
    string city;
    string state;
    string damageReport;
    string damageHeader;
    LatLon latLon;
};

#endif  // STORMREPORT_H
