// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef UTILITYLOCATIONFRAGMENT_H
#define UTILITYLOCATIONFRAGMENT_H

#include <string>
#include <unordered_map>
#include <vector>

using std::string;
using std::unordered_map;
using std::vector;

class UtilityLocationFragment {
public:
    static string extract7DayMetrics(const string&);
    static string extractWindDirection(const string&);
    static string extractTemp(const string&);

private:
    static const string nws7dayTemp1;
    static const string nws7dayTemp2;
    static const string nws7dayTemp3;
    static const string nws7dayTemp4;
    static const string nws7dayTemp5;
    static const string nws7dayTemp6;
    static const string nws7dayTemp7;
    static const string nws7dayTemp8;
    static const string nws7dayTemp9;
    static const string nws7dayTemp10;
    static const string nws7dayTemp11;
    static const string sevenDayWind1;
    static const string sevenDayWind2;
    static const string sevenDayWind3;
    static const string sevenDayWind4;
    static const string sevenDayWind5;
    static const string sevenDayWind6;
    static const string sevenDayWind7;
    static const string sevenDayWind8;
    static const string sevenDayWind9;
    static const unordered_map<string, string> windDir;
    static const vector<string> patterns;
};

#endif  // UTILITYLOCATIONFRAGMENT_H
