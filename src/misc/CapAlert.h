// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef CAPALERT_H
#define CAPALERT_H

#include <string>

using std::string;

class CapAlert {
public:
    CapAlert();
    explicit CapAlert(const string&);
    string url;
    string title;
    string summary;
    string instructions;
    string area;
    string text;
    string vtec;
    string nwsHeadLine;
    string windThreat;
    string maxWindGust;
    string hailThreat;
    string maxHailSize;
    string tornadoThreat;
    string motion;
};

#endif  // CAPALERT_H
