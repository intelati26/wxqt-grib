// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "UtilityMath.h"
#include <numbers>
#include <cmath>
#include "settings/UIPreferences.h"
#include "util/To.h"

double UtilityMath::distanceOfLine(double x1, double y1, double x2, double y2) {
    return sqrt(pow(x2 - x1, 2) + pow(y2 - y1, 2));
}

vector<double> UtilityMath::computeTipPoint(double x0, double y0, double x1, double y1, bool right) {
    const auto dx = x1 - x0;
    const auto dy = y1 - y0;
    const auto length = sqrt(dx * dx + dy * dy);
    const auto dirX = dx / length;
    const auto dirY = dy / length;
    const auto height = sqrt(3) / 2 * length;
    const auto cx = x0 + dx * 0.5;
    const auto cy = y0 + dy * 0.5;
    const auto pDirX = -dirY;
    const auto pDirY = dirX;
    if (right)
        return {cx + height * pDirX, cy + height * pDirY};
    else
        return {cx - height * pDirX, cy - height * pDirY};
}

vector<double> UtilityMath::computeMiddlePoint(double x0, double y0, double x1, double y1, double fraction) {
    return {x0 + fraction * (x1 - x0), y0 + fraction * (y1 - y0)};
}

double UtilityMath::pixPerDegreeLon(double centerX, double factor) {
    const auto radius = (180.0 / std::numbers::pi) * (1.0 / cos(deg2rad(30.51))) * factor;
    return radius * (std::numbers::pi / 180.0) * cos(deg2rad(centerX));
}

double UtilityMath::deg2rad(double deg) {
    return deg * std::numbers::pi / 180.0;
}

double UtilityMath::rad2deg(double rad) {
    return rad * 180.0 / std::numbers::pi;
}

string UtilityMath::heatIndex(const string& temperature, const string& relativeHumidity) {
    // temp >= 80 and RH >= 40
    if (temperature.empty() || relativeHumidity.empty()) {
        return "";
    }
    const auto T = To::Double(temperature);
    const auto R = To::Double(relativeHumidity);
    if (T > 80.0 && R > 40.0) {
        const auto s1 = -42.379;
        const auto s2 = 2.04901523 * T;
        const auto s3 = 10.14333127 * R;
        const auto s4 = 0.22475541 * T * R;
        const auto s5 = 6.83783 * pow(10.0, -3.0) * pow(T, 2.0);
        const auto s6 = 5.481717 * pow(10.0, -2.0) * pow(R, 2.0);
        const auto s7 = 1.22874 * pow(10.0, -3.0) * pow(T, 2.0) * R;
        const auto s8 = 8.5282 * pow(10.0, -4.0) * T * pow(R, 2.0);
        const auto s9 = 1.99 * pow(10.0, -6.0) * pow(T, 2.0) * pow(R, 2.0);
        return roundDTostring(s1 + s2 + s3 - s4 - s5 - s6 + s7 + s8 - s9);
    } else {
        return "";
    }
}

string UtilityMath::bearingToDirection(int direction) {
    const vector<string> windDirections{"N", "NNE", "NE", "ENE", "E", "ESE", "SE", "SSE", "S", "SSW", "SW", "WSW", "W", "WNW", "NW", "NNW", "N"};
    const auto normalizedDirection = direction % 360;
    const auto listIndex = static_cast<int>(round(normalizedDirection / 22.5));
    return windDirections[listIndex];
}

string UtilityMath::roundDTostring(double valueD) {
    return To::string(static_cast<int>(round(valueD)));
}

string UtilityMath::celsiusToFahrenheit(const string& value) {
    const auto s = To::string(round(To::Double(value) * 9.0 / 5.0 + 32.0));
    return UIPreferences::unitsF ? s : value;
}

string UtilityMath:: knotsToMph(const string& value) {
    return To::string(static_cast<int>(round(To::Double(value) * 1.151)));
}

string UtilityMath::unitsPressure(const string& value) {
    if (value.empty()) {
        return " mb";
    }
    const auto tmpNum = To::Double(value);
    return UIPreferences::unitsM ? To::string(tmpNum * 33.8637526) + " mb" : To::string(tmpNum) + " in";
}
