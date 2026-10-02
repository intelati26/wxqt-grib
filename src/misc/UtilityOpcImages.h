// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef UTILITYOPCIMAGES_H
#define UTILITYOPCIMAGES_H

#include <string>
#include <vector>

using std::string;
using std::vector;

class UtilityOpcImages {
public:
    static const vector<string> labels;
    static const vector<string> urls;
    // The charts that make a run with chart `index` (same area, same kind of chart): the analysis, then the 24, 48 and 96 hour
    // forecasts, in listed order. A chart that stands alone gives just itself.
    static vector<int> seriesOf(int index);
    static string seriesName(int index);
};

#endif  // UTILITYOPCIMAGES_H
