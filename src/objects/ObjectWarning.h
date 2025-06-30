// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef OBJECTWARNING_H
#define OBJECTWARNING_H

#include <string>
#include <vector>
#include "objects/LatLon.h"

using std::string;
using std::vector;

class ObjectWarning {
public:
    ObjectWarning(const string&, const string&, const string&, const string&, const string&, const string&, const string&, const string&, const string&);
    static vector<ObjectWarning> parseJson(const string&);
    string getClosestRadar() const;
    string getUrl() const;
    vector<LatLon> getPolygonAsLatLons(int) const;
    string url;
    string title;
    string area;
    string effective;
    string expires;
    string event;
    string sender;
    string polygon;
    string vtec;
    bool isCurrent;
};

#endif  // OBJECTWARNING_H
