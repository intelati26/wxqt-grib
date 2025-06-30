// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef OBJECTMETAR_H
#define OBJECTMETAR_H

#include <string>
#include <vector>
#include "objects/LatLon.h"
#include "objects/Site.h"

using std::string;
using std::vector;

class ObjectMetar {
public:
    ObjectMetar(const LatLon&, int = 0);
    void process();
    static string decodeIconFromMetar(const string&, const Site&);
    static string translateCondition(const string&);
    static string changeDegreeUnits(const string&);
    static string changePressureUnits(const string&);
    Site obsClosest;
    string condition;
    string temperature;
    string dewPoint;
    string windDirection;
    string windSpeed;
    string windGust;
    string seaLevelPressure;
    string visibility;
    string relativeHumidity;
    string windChill;
    string heatIndex;
    string conditionsTimeString;
    string timeStringUtc;
    string icon;
    vector<string> metarDataList;
    string metarData;

private:
    string metarSkyCondition;
    string metarWeatherCondition;
};

#endif  // OBJECTMETAR_H
