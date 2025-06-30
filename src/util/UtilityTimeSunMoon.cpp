// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "UtilityTimeSunMoon.h"
#include "external/SunSet.h"
#include "externalMoon/SunMoonCalculator.h"
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
    const auto time{ObjectDateTime::currentTimeForMoon()};
    const auto obsLon = UtilityMath::deg2rad(latLon.lon());  // lon is negative to the west.
    const auto obsLat = UtilityMath::deg2rad(latLon.lat());
    auto smc{SunMoonCalculator{time[0], time[1], time[2], time[3], time[4], time[5], obsLon, obsLat}};
    smc.calcSunAndMoon();
    const auto moonRise = ObjectDateTime::fromMoonTimes(SunMoonCalculator::getDate(smc.moon.rise)).format(ObjectDateTime::formatHourMinute);
    const auto moonSet = ObjectDateTime::fromMoonTimes(SunMoonCalculator::getDate(smc.moon.set)).format(ObjectDateTime::formatHourMinute);
    const auto moonPhase = To::string(static_cast<int>(smc.moon.illuminationPhase));
    const auto moonAge = To::string(static_cast<int>(smc.moonAge));
    return "Moonrise: " + moonRise + " Moonset: " + moonSet + " " + moonPhase + "%" + " age: " + moonAge;
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
