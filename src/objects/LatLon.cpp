// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "LatLon.h"
#include <cmath>
#include <iostream>
#include <iomanip>
#include <sstream>
#include "common/GlobalVariables.h"
#include "external/ExternalPoint.h"
#include "objects/WString.h"
#include "util/To.h"
#include "util/UtilityIO.h"
#include "util/UtilityList.h"
#include "util/UtilityMath.h"
#include "util/UtilityString.h"

LatLon::LatLon()
    : latString{To::string(0.0)}
    , lonString{To::string(0.0)}
    , latNum{0.0}
    , lonNum{0.0}
{}

LatLon::LatLon(double x, double y)
    : latString{To::string(x)}
    , lonString{To::string(y)}
    , latNum{x}
    , lonNum{y}
{}

LatLon::LatLon(const string& x, const string& y)
    : latString{x}
    , lonString{y}
    , latNum{To::Double(x)}
    , lonNum{To::Double(y)}
{}

LatLon LatLon::fromList(const vector<double>& coords) {
    return {coords[0], coords[1]};
}

double LatLon::lat() const {
    return latNum;
}

double LatLon::lon() const {
    return lonNum;
}

LatLon LatLon::reverseLon() const {
    return {lat(), -1.0 * lon()};
}

double LatLon::latInRadians() const {
    return UtilityMath::deg2rad(lat());
}

double LatLon::lonInRadians() const {
    return UtilityMath::deg2rad(lon());
}

string LatLon::latStr() const {
    return latString;
}

string LatLon::lonStr() const {
    return lonString;
}

string LatLon::latForNws() const {
    std::stringstream stream;
    stream << std::fixed << std::setprecision(4) << latNum;
    return stream.str();
}

string LatLon::lonForNws() const {
    std::stringstream stream;
    stream << std::fixed << std::setprecision(4) << lonNum;
    return stream.str();
}

void LatLon::setLon(double newLon) {
    lonNum = newLon;
    lonString = To::string(lonNum);
}

LatLon LatLon::fromWatchData(const string& temp) {
    auto latString = UtilityString::substring(temp, 0, 4);
    auto lonString = UtilityString::substring(temp, 4, 8);
    latString = UtilityString::addPeriodBeforeLastTwoChars(latString);
    lonString = UtilityString::addPeriodBeforeLastTwoChars(lonString);
    auto tmpDbl = To::Double(lonString);
    if (tmpDbl < 40.00) {
        tmpDbl += 100;
        lonString = To::string(tmpDbl);
    }
    const auto latNum = To::Double(latString);
    const auto lonNum = To::Double(lonString);
    return {latNum, lonNum};
}

string LatLon::printPretty() const {
    return latForNws() + ", " + lonForNws() + " ";
}

// used in SwoDayOne and PolygonWatch
string LatLon::printSpaceSeparated() const {
    return latString + " " + lonString + " ";
}

ExternalPoint LatLon::asPoint() const {
    return {lat(), lon()};
}

double LatLon::distance(const LatLon& location1, const LatLon& location2) { // , DistanceUnit unit
    const auto theta = location1.lonNum - location2.lonNum;
    auto dist = sin(UtilityMath::deg2rad(location1.latNum)) * sin(UtilityMath::deg2rad(location2.latNum)) +
            cos(UtilityMath::deg2rad(location1.latNum)) * cos(UtilityMath::deg2rad(location2.latNum)) * cos(UtilityMath::deg2rad(theta));
    dist = acos(dist);
    dist = UtilityMath::rad2deg(dist);
    dist = dist * 60.0 * 1.1515;
    return dist;
}

int LatLon::calculateBearing(const LatLon& start, const LatLon& end) {
    auto deltaLon = end.lonInRadians() - start.lonInRadians();
    auto x = cos(end.latInRadians()) * sin(deltaLon);
    auto y = cos(start.latInRadians()) * sin(end.latInRadians()) - sin(start.latInRadians()) * cos(end.latInRadians()) * cos(deltaLon);
    auto b = atan2(x, y);
    auto bearing = static_cast<int>(UtilityMath::rad2deg(b)) % 360;
    if (bearing < 0) {
        bearing += 360;
    }
    return bearing;
}

string LatLon::calculateDirection(const LatLon& start, const LatLon& end) {
    return UtilityMath::bearingToDirection(calculateBearing(start, end));
}

// take a space separated list of numbers and return a list of LatLon, list is of the format
// lon0 lat0 lon1 lat1 for watch
// for Watch need to multiply Y by -1.0
vector<LatLon> LatLon::parseStringToLatLons(const string& stringOfNumbers, int multiplier, bool isWarning) {
    const auto listOfNumbers = WString::split(stringOfNumbers, " ");
    vector<double> x;
    vector<double> y;
    for (auto i : range(listOfNumbers.size())) {
        if (isWarning) {
            if (i % 2 == 0) {
                y.push_back(To::Double(listOfNumbers[i]) * multiplier);
            } else {
                x.push_back(To::Double(listOfNumbers[i]));
            }
        } else {
            if (i % 2 == 0) {
                x.push_back(To::Double(listOfNumbers[i]));
            } else {
                y.push_back(To::Double(listOfNumbers[i]) * multiplier);
            }
        }
    }
    vector<LatLon> latLons;
    if (x.size() > 3 && y.size() > 3 && x.size() == y.size()) {
        for (auto index : range(x.size())) {
            latLons.emplace_back(x[index], y[index]);
        }
    }
    return latLons;
}
