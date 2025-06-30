// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "UtilityNexradColors.h"
#include "objects/Color.h"
#include "util/UtilityList.h"

double UtilityNexradColors::interpolate(double a, double b, double proportion) {
    return a + ((b - a) * proportion);
}

double UtilityNexradColors::interpolateHue(double colorA, double colorB, double proportion) {
    const auto diff = colorB - colorA;
    const auto total = 1.0;
    if (diff > total / 2) {
        const auto ret = (total - (colorB - colorA)) * -1.0;
        if (ret < 0) {
            return ret + total;
        }
        return ret;
    }
    return colorA + ((colorB - colorA) * proportion);
}

// color comes in as int which must be broken up and converted to 0.0-1.0
// for HSV, all values are also 0.0..1.0 including the first value which must be div by 360 etc. in Color.cc
vector<double> UtilityNexradColors::interpolateColor(const vector<double>& colorA, const vector<double>& colorB, double proportion) {
    const auto hsva = Color::colorToHsv(colorA);
    auto hsvb = Color::colorToHsv(colorB);
    for (auto index : range(3)) {
        if (index > 0) {
            hsvb[index] = interpolate(hsva[index], hsvb[index], proportion);
        } else {
            hsvb[index] = interpolateHue(hsva[index], hsvb[index], proportion);
        }
    }
    return Color::hsvToColor(hsvb);
}
