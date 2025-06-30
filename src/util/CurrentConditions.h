// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef CURRENTCONDITIONS_H
#define CURRENTCONDITIONS_H

#include <string>
#include <vector>
#include "objects/LatLon.h"
#include "util/ObjectMetar.h"

using std::string;
using std::vector;

class CurrentConditions {
public:
    void process(const LatLon&, int index = 0);
    void formatCurrentConditions();
    string getObsFullName() const;
    void timeCheck();
    string temperature;
    string windChill;
    string heatIndex;
    string dewPoint;
    string relativeHumidity;
    string seaLevelPressure;
    string windDirection;
    string windSpeed;
    string windGust;
    string visibility;
    string condition;
    string conditionsTimeString;
    string obsStation;
    string data;
    string iconUrl;
    string topLine;
    string middleLine;
    string bottomLine;

private:
    string getString(const ObjectMetar&);
    LatLon latLon;
    string timeStringUtc;
};

#endif  // CURRENTCONDITIONS_H
