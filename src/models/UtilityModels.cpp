// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include <cmath>
#include "UtilityModels.h"
#include "objects/ObjectDateTime.h"
#include "objects/WString.h"
#include "util/To.h"

const string UtilityModels::urlSeperator{","};

string UtilityModels::convertTimeRuntoTimeString(const string& runStr, const string& timeStrFunc) {
    const auto timeStr = WString::split(timeStrFunc, " ")[0];
    const auto runInt = To::Int(runStr);
    const auto timeInt = To::Int(timeStr);
    const auto realTimeGmt = runInt + timeInt;
    const auto offsetFromUtc = ObjectDateTime::offsetFromUtcInSeconds();
    const auto realTime = realTimeGmt + static_cast<int>(floor(offsetFromUtc / 60 / 60));
    auto hourOfDay = realTime % 24;
    string amPm = "am";
    if (hourOfDay > 11) {
        amPm = "pm";
        if (hourOfDay > 12) {
            hourOfDay -= 12;
        }
    }
    auto day = realTime / 24.0;
    if (hourOfDay < 0) {
        hourOfDay = 12 + hourOfDay;
        amPm = "pm";
        day -= 1;
    }
    const auto dayOfWeek = ObjectDateTime::getDayOfWeek();
    const auto hourOfDayLocal = ObjectDateTime::getHour();
    if (runInt >= 0 && runInt < -offsetFromUtc / 60 / 60 && (hourOfDayLocal - offsetFromUtc / 60 / 60) >= 24) {
        day += 1;
    }
    string futureDay;
    auto dayMod = static_cast<int>(dayOfWeek + day) % 7;
    // Vala/Dart: is Monday(1)..Sunday(7), Swift is Sat(0)..Fri(6)
    if (dayMod == 0) {
        dayMod = 7;
    }
    switch (dayMod) {
        case 7:
            futureDay = "Sun";
            break;
        case 1:
            futureDay = "Mon";
            break;
        case 2:
            futureDay = "Tue";
            break;
        case 3:
            futureDay = "Wed";
            break;
        case 4:
            futureDay = "Thu";
            break;
        case 5:
            futureDay = "Fri";
            break;
        case 6:
            futureDay = "Sat";
            break;
        default:
            break;
    }
    return futureDay + "  " + To::string(hourOfDay) + amPm;
}

vector<string> UtilityModels::updateTime(
    const string& run,
    const string& modelCurrentTime,
    const vector<string>& listTime,
    const string& prefix
) {
    auto run2 = run;
    run2 = WString::replace(run2, "Z", "");
    run2 = WString::replace(run2, "z", "");
    vector<string> listTimeNew;
    auto modelCurrentTime2 = modelCurrentTime;
    modelCurrentTime2 = WString::replace(modelCurrentTime2, "Z", "");
    modelCurrentTime2 = WString::replace(modelCurrentTime2, "z", "");
    if (!modelCurrentTime2.empty()) {
        if (To::Int(run2) > To::Int(modelCurrentTime2)) {
            run2 = To::string(To::Int(run2) - 24);
        }
        for (const auto& value : listTime) {
            const auto tmp = WString::split(value, " ")[0];
            auto tmpStr = tmp;
            if (!prefix.empty()) {
                tmpStr = WString::replace(tmp, prefix, "");
            }
            listTimeNew.push_back(prefix + tmpStr + " " + convertTimeRuntoTimeString(run2, tmpStr));
        }
    }
    return listTimeNew;
}
