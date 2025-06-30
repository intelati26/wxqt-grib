/*
 * Provides the ability to calculate the local time for sunrise,
 * sunset, and moonrise at any point in time at any location in the world
 *
 * Original work used with permission maintaining license
 * Copyright (GPL) 2004 Mike Chirico mchirico@comcast.net
 * Modifications copyright
 * Copyright (GPL) 2015 Peter Buelow
 *
 * This file is part of the Sunset library
 *
 * Sunset is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * Sunset is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Foobar.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "SunSet.h"
#include <numbers>

/**
 * \fn void SunSet::setPosition(double lat, double lon, int tz)
 * \param lat Double Latitude value
 * \param lon Double Longitude value
 * \param tz Integer Timezone offset
 * 
 * This will set the location the library uses for it's math. The
 * timezone is included in this as it's not valid to call
 * any of the calc functions until you have set a timezone.
 * It is possible to simply call setPosition one time, with a timezone
 * and not use the setTZOffset() function ever, if you never
 * change timezone values.
 * 
 * This is the old versoin of the setPosition using an integer
 * timezone, and will not be deprecated. However, it is preferred to
 * use the double version going forward.
 */
void SunSet::setPosition(double lat, double lon, int tz) {
    latitude = lat;
    longitude = lon;
    if (tz >= -12 && tz <= 14)
        tzOffset = tz;
    else
        tzOffset = 0.0;
}

double SunSet::degToRad(double angleDeg) {
    return std::numbers::pi * angleDeg / 180.0;
}

double SunSet::radToDeg(double angleRad) {
    return 180.0 * angleRad / std::numbers::pi;
}

double SunSet::calcMeanObliquityOfEcliptic(double t) {
    const auto seconds = 21.448 - t * (46.8150 + t * (0.00059 - t * (0.001813)));
    const auto e0 = 23.0 + (26.0 + (seconds/60.0))/60.0;
    return e0;              // in degrees
}

double SunSet::calcGeomMeanLongSun(double t) {
    if (std::isnan(t)) {
        return nan("");
    }
    auto L = 280.46646 + t * (36000.76983 + 0.0003032 * t);
    // 2021_03_19 commented out as value not used
    // std::fmod(L, 360.0);
    return L;              // in degrees
}

double SunSet::calcObliquityCorrection(double t) {
    const auto e0 = calcMeanObliquityOfEcliptic(t);
    const auto omega = 125.04 - 1934.136 * t;
    const auto e = e0 + 0.00256 * cos(degToRad(omega));
    return e;               // in degrees
}

double SunSet::calcEccentricityEarthOrbit(double t) {
    const auto e = 0.016708634 - t * (0.000042037 + 0.0000001267 * t);
    return e;               // unitless
}

double SunSet::calcGeomMeanAnomalySun(double t) {
    const auto M = 357.52911 + t * (35999.05029 - 0.0001537 * t);
    return M;               // in degrees
}

double SunSet::calcEquationOfTime(double t) {
    const auto epsilon = calcObliquityCorrection(t);
    const auto l0 = calcGeomMeanLongSun(t);
    const auto e = calcEccentricityEarthOrbit(t);
    const auto m = calcGeomMeanAnomalySun(t);
    auto y = tan(degToRad(epsilon)/2.0);

    y *= y;

    const auto sin2l0 = sin(2.0 * degToRad(l0));
    const auto sinm   = sin(degToRad(m));
    const auto cos2l0 = cos(2.0 * degToRad(l0));
    const auto sin4l0 = sin(4.0 * degToRad(l0));
    const auto sin2m  = sin(2.0 * degToRad(m));
    const auto Etime = y * sin2l0 - 2.0 * e * sinm + 4.0 * e * y * sinm * cos2l0 - 0.5 * y * y * sin4l0 - 1.25 * e * e * sin2m;
    return radToDeg(Etime)*4.0;  // in minutes of time
}

double SunSet::calcTimeJulianCent(double jd) {
    const auto T = (jd - 2451545.0) / 36525.0;
    return T;
}

double SunSet::calcSunTrueLong(double t) {
    const auto l0 = calcGeomMeanLongSun(t);
    const auto c = calcSunEqOfCenter(t);
    const auto O = l0 + c;
    return O;               // in degrees
}

double SunSet::calcSunApparentLong(double t) {
    const auto o = calcSunTrueLong(t);
    const auto omega = 125.04 - 1934.136 * t;
    const auto lambda = o - 0.00569 - 0.00478 * sin(degToRad(omega));
    return lambda;          // in degrees
}

double SunSet::calcSunDeclination(double t) {
    const auto e = calcObliquityCorrection(t);
    const auto lambda = calcSunApparentLong(t);
    const auto sint = sin(degToRad(e)) * sin(degToRad(lambda));
    const auto theta = radToDeg(asin(sint));
    return theta; // in degrees
}

double SunSet::calcHourAngleSunrise(double lat, double solarDec, double offset) {
    const auto latRad = degToRad(lat);
    const auto sdRad  = degToRad(solarDec);
    const auto HA = acos(cos(degToRad(offset)) / (cos(latRad) * cos(sdRad)) - tan(latRad) * tan(sdRad));
    return HA; // in radians
}

double SunSet::calcHourAngleSunset(double lat, double solarDec, double offset) {
    const auto latRad = degToRad(lat);
    const auto sdRad  = degToRad(solarDec);
    const auto HA = (acos(cos(degToRad(offset)) / (cos(latRad) * cos(sdRad)) - tan(latRad) * tan(sdRad)));
    return -HA; // in radians
}

/**
 * \fn double SunSet::calcJD(int y, int m, int d)
 * \param y Integer year as a 4 digit value
 * \param m Integer month, not 0 based
 * \param d Integer day, not 0 based
 * \return Returns the Julian date as a double for the calculations
 * 
 * A well known JD calculator
 */
double SunSet::calcJD(int y, int m, int d) {
    if (m <= 2) {
        y -= 1;
        m += 12;
    }
    const auto A = static_cast<int>(floor(y / 100));
    const auto B = static_cast<int>(2 - A + floor(A / 4));
    const auto JD = floor(365.25 * (y + 4716)) + floor(30.6001 * (m + 1)) + d + B - 1524.5;
    return JD;
}

double SunSet::calcJDFromJulianCent(double t) {
    auto JD = t * 36525.0 + 2451545.0;
    return JD;
}

double SunSet::calcSunEqOfCenter(double t) {
    const auto m = calcGeomMeanAnomalySun(t);
    const auto mrad = degToRad(m);
    const auto sinm = sin(mrad);
    const auto sin2m = sin(mrad + mrad);
    const auto sin3m = sin(mrad + mrad + mrad);
    const auto C = sinm * (1.914602 - t * (0.004817 + 0.000014 * t)) + sin2m * (0.019993 - 0.000101 * t) + sin3m * 0.000289;
    return C;  // in degrees
}

/**
 * \fn double SunSet::calcAbsSunrise(double offset)
 * \param offset Double The specific angle to use when calculating sunrise
 * \return Returns the time in minutes past midnight in UTC for sunrise at your location
 * 
 * This does a bunch of work to get to an accurate angle. Note that it does it 2x, once
 * to get a rough position, and then it doubles back and redoes the calculations to 
 * refine the value. The first time through, it will be off by as much as 2 minutes, but
 * the second time through, it will be nearly perfect.
 * 
 * Note that this is the base calculation for all sunrise calls. The others just modify
 * the offset angle to account for the different needs.
 */
double SunSet::calcAbsSunrise(double offset) {
    const auto t = calcTimeJulianCent(julianDate);
    // *** First pass to approximate sunrise
    auto eqTime = calcEquationOfTime(t);
    auto solarDec = calcSunDeclination(t);
    auto hourAngle = calcHourAngleSunrise(latitude, solarDec, offset);
    auto delta = longitude + radToDeg(hourAngle);
    auto timeDiff = 4 * delta;  // in minutes of time
    auto timeUTC = 720 - timeDiff - eqTime;  // in minutes
    const auto newt = calcTimeJulianCent(calcJDFromJulianCent(t) + timeUTC / 1440.0);
    eqTime = calcEquationOfTime(newt);
    solarDec = calcSunDeclination(newt);
    hourAngle = calcHourAngleSunrise(latitude, solarDec, offset);
    delta = longitude + radToDeg(hourAngle);
    timeDiff = 4 * delta;
    timeUTC = 720 - timeDiff - eqTime;  // in minutes
    return timeUTC;  // return time in minutes from midnight
}

/**
 * \fn double SunSet::calcAbsSunset(double offset)
 * \param offset Double The specific angle to use when calculating sunset
 * \return Returns the time in minutes past midnight in UTC for sunset at your location
 * 
 * This does a bunch of work to get to an accurate angle. Note that it does it 2x, once
 * to get a rough position, and then it doubles back and redoes the calculations to 
 * refine the value. The first time through, it will be off by as much as 2 minutes, but
 * the second time through, it will be nearly perfect.
 *
 * Note that this is the base calculation for all sunset calls. The others just modify
 * the offset angle to account for the different needs.
*/
double SunSet::calcAbsSunset(double offset) {
    const auto t = calcTimeJulianCent(julianDate);
    // *** First pass to approximate sunset
    auto eqTime = calcEquationOfTime(t);
    auto solarDec = calcSunDeclination(t);
    auto hourAngle = calcHourAngleSunset(latitude, solarDec, offset);
    auto delta = longitude + radToDeg(hourAngle);
    auto timeDiff = 4 * delta;  // in minutes of time
    auto timeUTC = 720 - timeDiff - eqTime;  // in minutes
    const auto newt = calcTimeJulianCent(calcJDFromJulianCent(t) + timeUTC / 1440.0);
    eqTime = calcEquationOfTime(newt);
    solarDec = calcSunDeclination(newt);
    hourAngle = calcHourAngleSunset(latitude, solarDec, offset);
    delta = longitude + radToDeg(hourAngle);
    timeDiff = 4 * delta;
    timeUTC = 720 - timeDiff - eqTime;  // in minutes
    return timeUTC;  // return time in minutes from midnight
}

/**
 * \fn double SunSet::calcSunrise()
 * \return Returns local sunrise in minutes past midnight.
 * 
 * This function will return the Official sunrise in local time for your location
 */
double SunSet::calcSunrise() {
    return calcAbsSunrise(SUNSET_OFFICIAL) + (60 * tzOffset);
}

/**
 * \fn double SunSet::calcSunset()
 * \return Returns local sunset in minutes past midnight.
 * 
 * This function will return the Official sunset in local time for your location
 */
double SunSet::calcSunset() {
    return calcAbsSunset(SUNSET_OFFICIAL) + (60 * tzOffset);
}

/**
 * double SunSet::setCurrentDate(int y, int m, int d)
 * \param y Integer year, must be 4 digits
 * \param m Integer month, not zero based (Jan = 1)
 * \param d Integer day of month, not zero based (month starts on day 1)
 * \return Returns the result of the Julian Date conversion if you want to save it
 * 
 * Since these calculations are done based on the Julian Calendar, we must convert
 * our year month day into Julian before we use it. You get the Julian value for
 * free if you want it.
 */
double SunSet::setCurrentDate(int y, int m, int d) {
    julianDate = calcJD(y, m, d);
    return julianDate;
}

/**
 * \fn void SunSet::setTZOffset(int tz)
 * \param tz Integer timezone, may be positive or negative
 * 
 * Critical to set your timezone so results are accurate for your time and date.
 * This function is critical to make sure the system works correctly. If you
 * do not set the timezone correctly, the return value will not be correct for
 * your location. Forgetting this will result in return values that may actually
 * be negative in some cases.
 * 
 * This function is a holdover from the previous design using an integer timezone
 * and will not be deprecated. It is preferred to use the setTZOffset(doubble).
 */
void SunSet::setTZOffset(int tz) {
    if (tz >= -12 && tz <= 14)
        tzOffset = static_cast<double>(tz);
    else
        tzOffset = 0.0;
}
