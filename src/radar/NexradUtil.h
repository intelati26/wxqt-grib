// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef NEXRADUTIL_H
#define NEXRADUTIL_H

#include <cstdint>
#include <string>
#include <vector>

using std::string;
using std::vector;

class NexradUtil {
public:
    static int findRadarProductIndex(const string&);
    static int findRadarProductIndexTdwr(const string&);
    static bool isRadarTdwr(const string&);
    static bool isProductTdwr(const string&);
    static int getNumberRangeBins(uint16_t);
    static double getBinSize(uint16_t);
    static bool isRadarTimeOld(int);
    static bool isVtecCurrent(const string&);
    static double wxoglDspLegendMax;
    static const vector<string> radarProductList;
    static const vector<string> radarProductListTdwr;
};

#endif  // NEXRADUTIL_H
