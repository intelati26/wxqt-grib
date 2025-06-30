// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "ObjectMetar.h"
#include <iostream>
#include "common/GlobalVariables.h"
#include "objects/ObjectDateTime.h"
#include "objects/WString.h"
#include "radar/Metar.h"
#include "util/To.h"
#include "util/UtilityIO.h"
#include "util/UtilityMath.h"
#include "util/UtilityMetarConditions.h"
#include "util/UtilityString.h"

ObjectMetar::ObjectMetar(const LatLon& location, int index)
    : obsClosest{Metar::findClosestObservation(location, index)}
{}

void ObjectMetar::process() {
    metarData = UtilityIO::getHtmlWithRetry(GlobalVariables::tgftpSitePrefix + "data/observations/metar/decoded/" + obsClosest.codeName + ".TXT", 200);
    temperature = UtilityString::parse(metarData, "Temperature: (.*?) F");
    dewPoint = UtilityString::parse(metarData, "Dew Point: (.*?) F");
    windDirection = UtilityString::parse(metarData, "Wind: from the (.*?) \\(.*? degrees\\) at .*? MPH ");
    windSpeed = UtilityString::parse(metarData, "Wind: from the .*? \\(.*? degrees\\) at (.*?) MPH ");
    windGust = UtilityString::parse(metarData, "Wind: from the .*? \\(.*? degrees\\) at .*? MPH \\(.*? KT\\) gusting to (.*?) MPH");
    seaLevelPressure = UtilityString::parse(metarData, "Pressure \\(altimeter\\): .*? in. Hg \\((.*?) hPa\\)");
    visibility = UtilityString::parse(metarData, "Visibility: (.*?) mile");
    relativeHumidity = UtilityString::parse(metarData, "Relative Humidity: (.*?)%");
    windChill = UtilityString::parse(metarData, "Windchill: (.*?) F");
    heatIndex = UtilityMath::heatIndex(temperature, relativeHumidity);
    metarSkyCondition = UtilityString::parse(metarData, "Sky conditions: (.*?)" + GlobalVariables::newline);
    metarWeatherCondition = UtilityString::parse(metarData, "Weather: (.*?)" + GlobalVariables::newline);
    metarSkyCondition = UtilityString::title(metarSkyCondition);
    metarWeatherCondition = UtilityString::title(metarWeatherCondition);
    if (metarWeatherCondition.empty() || WString::contains(metarWeatherCondition, "Inches Of Snow On Ground")) {
        condition = metarSkyCondition;
    } else {
        condition = metarWeatherCondition;
    }
    condition = WString::replace(condition, "; Lightning Observed", "");
    condition = WString::replace(condition, "; Cumulonimbus Clouds, Lightning Observed", "");
    if (condition == "Mist") {
        condition = "Fog/Mist";
    }
    icon = decodeIconFromMetar(condition, obsClosest);
    condition = WString::replace(condition, ";", " and");
    metarDataList = WString::split(metarData, GlobalVariables::newline);
    if (metarDataList.size() > 2) {
        const auto localStatus = WString::split(metarDataList[1], "/");
        if (localStatus.size() > 1) {
            const auto tmp = WString::replace(localStatus[0], " UTC", "");
            conditionsTimeString = ObjectDateTime::convertFromUTCForMetar(tmp) + " " + obsClosest.codeName;
            timeStringUtc = WString::strip(localStatus[1]);
        }
    }
    seaLevelPressure = changePressureUnits(seaLevelPressure);
    temperature = changeDegreeUnits(temperature);
    dewPoint = changeDegreeUnits(dewPoint);
    windChill = changeDegreeUnits(windChill);
    heatIndex = changeDegreeUnits(heatIndex);
    if (windSpeed.empty()) {
        windSpeed = "0";
    }
    if (condition.empty()) {
        condition = "NA";
    }
}

string ObjectMetar::decodeIconFromMetar(const string& condition, const Site& obs) {
    const string timeOfDay{ObjectDateTime::isDaytime(obs) ? "day" : "night"};
    const auto conditionModified{WString::split(condition, ";")[0]};
    string shortCondition;
    if (UtilityMetarConditions::iconFromCondition.contains(conditionModified)) {
        shortCondition = UtilityMetarConditions::iconFromCondition.at(conditionModified);
    } else {
        std::cout << "Condition not found ObjectMetar: " << conditionModified << std::endl;
    }
    shortCondition = translateCondition(shortCondition);
    return GlobalVariables::nwsApiUrl + "icons/land/" + timeOfDay + "/" + shortCondition + "?size=medium";
}

string ObjectMetar::translateCondition(const string& condition) {
    auto newCondition = WString::replace(condition, "minus_", "");
    newCondition = WString::replace(newCondition, "fg", "fog");
    newCondition = WString::replace(newCondition, "fzrain", "rainfzra");
    if (!WString::contains(condition, "ts") && !WString::contains(condition, "fz")) {
        newCondition = WString::replace(newCondition, "ra", "rain");
    }
    return newCondition;
}

string ObjectMetar::changeDegreeUnits(const string& value) {
    if (!value.empty()) {
        const auto tempD = To::Double(value);
        return UtilityMath::roundDTostring(tempD);
    } else {
        return "NA";
    }
}

string ObjectMetar::changePressureUnits(const string& value) {
    return value + " mb";
}
