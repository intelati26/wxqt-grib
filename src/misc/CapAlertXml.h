// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef CAPALERTXML_H
#define CAPALERTXML_H

#include <string>
#include <vector>

using std::string;
using std::vector;

class CapAlertXml {
public:
    explicit CapAlertXml(const string&);
    string getClosestRadar() const;
    string url;
    string title;
    string summary;
    string instructions;
    string area;
    string effective;
    string expires;
    string event;
    string polygon;
    string text;
    vector<string> points;
};

#endif  // CAPALERTXML_H
