// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef LATLON_H
#define LATLON_H

#include <string>
#include <vector>
#include "external/ExternalPoint.h"

using std::string;
using std::vector;

class LatLon {
public:
    LatLon();
    LatLon(double, double);
    LatLon(const string&, const string&);
    double lat() const;
    double lon() const;
    LatLon reverseLon() const;
    double latInRadians() const;
    double lonInRadians() const;
    string latStr() const;
    string lonStr() const;
    string latForNws() const;
    string lonForNws() const;
    void setLon(double);
    string printPretty() const;
    string printSpaceSeparated() const;
    ExternalPoint asPoint() const;
    static LatLon fromWatchData(const string&);
    static LatLon fromList(const vector<double>&);
    static double distance(const LatLon&, const LatLon&);
    static int calculateBearing(const LatLon&, const LatLon&);
    static string calculateDirection(const LatLon&, const LatLon&);
    static vector<LatLon> parseStringToLatLons(const string&, int, bool);

private:
    string latString;
    string lonString;
    double latNum;
    double lonNum;
};

#endif  // LATLON_H
