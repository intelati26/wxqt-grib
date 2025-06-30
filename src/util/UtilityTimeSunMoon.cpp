// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "UtilityTimeSunMoon.h"
#include "external/SunSet.h"
#include "objects/ObjectDateTime.h"
#include "util/To.h"
#include "util/UtilityMath.h"

string UtilityTimeSunMoon::getSunTimes(const LatLon& latLon) {
    SunSet sun;
    const auto offset = static_cast<int>(ObjectDateTime::offsetFromUtcInSeconds() / 3600);
    sun.setPosition(latLon.lat(), latLon.lon(), offset);
    sun.setCurrentDate(ObjectDateTime::getYear(), ObjectDateTime::getMonth(), ObjectDateTime::getDay());
    // If you have daylight savings time, make sure you set the timezone appropriately as well
    sun.setTZOffset(offset);
    return "Sunrise: " + ObjectDateTime::timeOfDayFromMinutes(sun.calcSunrise()) + " Sunset: "  + ObjectDateTime::timeOfDayFromMinutes(sun.calcSunset());
}

string UtilityTimeSunMoon::getMoonTimes(const LatLon& latLon) {
    return "";
}

vector<double> UtilityTimeSunMoon::getSunriseSunsetFromObs(const Site& obs) {
    SunSet sun;
    const auto offset = static_cast<int>(ObjectDateTime::offsetFromUtcInSeconds() / 3600);
    sun.setPosition(obs.latLon.lat(), obs.latLon.lon(), offset);
    sun.setCurrentDate(ObjectDateTime::getYear(), ObjectDateTime::getMonth(), ObjectDateTime::getDay());
    // If you have daylight savings time, make sure you set the timezone appropriately as well
    sun.setTZOffset(offset);
    return {sun.calcSunrise(), sun.calcSunset()};
}
