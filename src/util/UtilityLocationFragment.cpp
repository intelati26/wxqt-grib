// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "UtilityLocationFragment.h"
#include "objects/WString.h"
#include "util/UtilityString.h"

const string UtilityLocationFragment::nws7dayTemp1{"with a low around (-?[0-9]{1,3})\\."};
const string UtilityLocationFragment::nws7dayTemp2{"with a high near (-?[0-9]{1,3})\\."};
const string UtilityLocationFragment::nws7dayTemp3{"teady temperature around (-?[0-9]{1,3})\\."};
const string UtilityLocationFragment::nws7dayTemp4{"Low around (-?[0-9]{1,3})\\."};
const string UtilityLocationFragment::nws7dayTemp5{"High near (-?[0-9]{1,3})\\."};
const string UtilityLocationFragment::nws7dayTemp6{"emperature falling to around (-?[0-9]{1,3}) "};
const string UtilityLocationFragment::nws7dayTemp7{"emperature rising to around (-?[0-9]{1,3}) "};
const string UtilityLocationFragment::nws7dayTemp8{"emperature falling to near (-?[0-9]{1,3}) "};
const string UtilityLocationFragment::nws7dayTemp9{"emperature rising to near (-?[0-9]{1,3}) "};
const string UtilityLocationFragment::nws7dayTemp10{"High near (-?[0-9]{1,3}),"};
const string UtilityLocationFragment::nws7dayTemp11{"Low around (-?[0-9]{1,3}),"};
const string UtilityLocationFragment::sevenDayWind1{"wind ([0-9]*) to ([0-9]*) mph"};
const string UtilityLocationFragment::sevenDayWind2{"wind around ([0-9]*) mph"};
const string UtilityLocationFragment::sevenDayWind3{"with gusts as high as ([0-9]*) mph"};
const string UtilityLocationFragment::sevenDayWind4{" ([0-9]*) to ([0-9]*) mph after"};
const string UtilityLocationFragment::sevenDayWind5{" around ([0-9]*) mph after "};
const string UtilityLocationFragment::sevenDayWind6{" ([0-9]*) to ([0-9]*) mph in "};
const string UtilityLocationFragment::sevenDayWind7{"around ([0-9]*) mph"};
const string UtilityLocationFragment::sevenDayWind8{"Winds could gust as high as ([0-9]*) mph\\."};
const string UtilityLocationFragment::sevenDayWind9{" ([0-9]*) to ([0-9]*) mph."};

const unordered_map<string, string> UtilityLocationFragment::windDir{
    {"north", "N"},
    {"north northeast", "NNE"},
    {"northeast", "NE"},
    {"east northeast", "ENE"},
    {"east", "E"},
    {"east southeast", "ESE"},
    {"south southeast", "SSE"},
    {"southeast", "SE"},
    {"south", "S"},
    {"south southwest", "SSW"},
    {"southwest", "SW"},
    {"west southwest", "WSW"},
    {"west", "W"},
    {"west northwest", "WNW"},
    {"northwest", "NW"},
    {"north northwest", "NNW"}
};

string UtilityLocationFragment::extract7DayMetrics(const string& chunk) {
    const string spacing{" "};
    // wind 24 to 29 mph
    const auto wind = UtilityString::parseTwo(chunk, sevenDayWind1);
    // wind around 9 mph
    const auto wind2 = UtilityString::parse(chunk, sevenDayWind2);
    // 5 to 10 mph after
    const auto wind3 = UtilityString::parseTwo(chunk, sevenDayWind4);
    // around 5 mph after
    const auto wind4 = UtilityString::parse(chunk, sevenDayWind5);
    // 5 to 7 mph in
    const auto wind5 = UtilityString::parseTwo(chunk, sevenDayWind6);
    // around 6 mph.
    const auto wind7 = UtilityString::parse(chunk, sevenDayWind7);
    // with gusts as high as 21 mph
    auto gust = UtilityString::parse(chunk, sevenDayWind3);
    // 5 to 7 mph.
    const auto wind9 = UtilityString::parseTwo(chunk, sevenDayWind9);
    // Winds could gust as high as 21 mph.
    if (gust.empty()) {
        gust = UtilityString::parse(chunk, sevenDayWind8);
    }
    gust = (!gust.empty()) ? " G " + gust + " mph" : " mph";
    if (!wind[0].empty() && !wind[1].empty()) {
        return spacing + wind[0] + "-" + wind[1] + gust;
    } else if (!wind2.empty()) {
        return spacing + wind2 + gust;
    } else if (!wind3[0].empty() && !wind3[1].empty()) {
        return spacing + wind3[0] + "-" + wind3[1] + gust;
    } else if (!wind4.empty()) {
        return spacing + wind4 + gust;
    } else if (!wind5[0].empty() && !wind5[1].empty()) {
        return spacing + wind5[0] + "-" + wind5[1] + gust;
    } else if (!wind7.empty()) {
        return spacing + wind7 + gust;
    } else if (!wind9[0].empty() && !wind9[1].empty()) {
        return spacing + wind9[0] + "-" + wind9[1] + gust;
    } else {
        return "";
    }
}

const vector<string> UtilityLocationFragment::patterns{
    "Light (.*?) wind increasing",
    "Breezy, with a[n]? (.*?) wind",
    "wind becoming (\\w+\\s?\\w*) around",
    "wind becoming (.*?) [0-9]",
    "\\. (\\w+\\s?\\w*) wind ",
    "Windy, with a[n]? (.*?) wind",
    "Blustery, with a[n]? (.*?) wind",
    "Light (.*?) wind"
};

string UtilityLocationFragment::extractWindDirection(const string& chunk) {
    vector<string> windResults;
    for (const auto& pattern : patterns) {
        windResults.push_back(UtilityString::parse(chunk, pattern));
    }
    string retStr;
    for (const auto& windToken : windResults) {
        if (!windToken.empty()) {
            retStr = windToken;
            break;
        }
    }
    if (retStr.empty()) {
        return "";
    } else {
        const auto tmp = WString::toLower(retStr);
        string ret;
        if (windDir.contains(tmp)) {
            ret = " " + windDir.at(tmp);
        } else {
            ret = "";
        }
        return ret;
    }
}

string UtilityLocationFragment::extractTemp(const string& blob) {
    const vector<string> regexps{
        nws7dayTemp1,
        nws7dayTemp2,
        nws7dayTemp3,
        nws7dayTemp4,
        nws7dayTemp5,
        nws7dayTemp6,
        nws7dayTemp7,
        nws7dayTemp8,
        nws7dayTemp9,
        nws7dayTemp10,
        nws7dayTemp11
    };
    for (const auto& regexp : regexps) {
        const auto temp = UtilityString::parse(blob, regexp);
        if (!temp.empty()) {
            return temp;
        }
    }
    return "";
}
