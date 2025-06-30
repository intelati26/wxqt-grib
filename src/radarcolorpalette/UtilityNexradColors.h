// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef UTILITYNEXRADCOLORS_H
#define UTILITYNEXRADCOLORS_H

#include <vector>

using std::vector;

class UtilityNexradColors {
public:
    static vector<double> interpolateColor(const vector<double>&, const vector<double>&, double);

private:
    static double interpolate(double, double, double);
    static double interpolateHue(double, double, double);
};

#endif  // UTILITYNEXRADCOLORS_H
