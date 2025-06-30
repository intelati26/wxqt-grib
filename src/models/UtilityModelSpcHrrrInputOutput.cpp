// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "UtilityModelSpcHrrrInputOutput.h"
#include "common/GlobalVariables.h"
#include "objects/WString.h"
#include "util/UtilityIO.h"
#include "util/UtilityList.h"
#include "util/UtilityString.h"
#include "util/To.h"
#include "UtilityModelSpcHrrrInterface.h"

RunTimeData UtilityModelSpcHrrrInputOutput::getRunTime() {
    RunTimeData runData;
    const auto htmlRunStatus = UtilityIO::getHtml(GlobalVariables::nwsSPCwebsitePrefix + "/exper/hrrr/data/hrrr3/cron.log");
    runData.validTime = UtilityString::parse(htmlRunStatus, "Latest Run: ([0-9]{10})");
    runData.mostRecentRun = runData.validTime;
    runData.appendListRun(runData.mostRecentRun);
    const auto runTimes = UtilityString::parseColumn(htmlRunStatus, "Run: ([0-9]{8}/[0-9]{4})");
    for (const auto& time : UtilityList::reversed(runTimes)) {
        auto t = WString::replace(time, "/", "");
        if (t != (runData.mostRecentRun + "00")) {
            t.pop_back();
            t.pop_back();
            runData.appendListRun(t);
        }
    }
    return runData;
}

string UtilityModelSpcHrrrInputOutput::getImageUrl(ObjectModel * om) {
    const auto url = GlobalVariables::nwsSPCwebsitePrefix +
            "/exper/hrrr/data/hrrr3/" +
            WString::toLower(getSectorCode(om->sector)) +
            "/R" +
            WString::replace(om->run, "Z", "") +
            "_F" +
            formatTime(om->getTime()) +
            "_V" +
            getValidTime(om->run, om->getTime(), om->runTimeData.validTime) +
            "_" +
            getSectorCode(om->sector) +
            "_" +
            om->param +
            ".gif";
    return url;
}

string UtilityModelSpcHrrrInputOutput::getSectorCode(const string& sectorName) {
    string sectorCode = "S19";
    for (auto index : range(UtilityModelSpcHrrrInterface::sectors.size())) {
        if (sectorName == UtilityModelSpcHrrrInterface::sectors[index]) {
            sectorCode = UtilityModelSpcHrrrInterface::sectorCodes[index];
            break;
        }
    }
    return sectorCode;
}

//https://www.spc.noaa.gov/exper/hrrr/data/hrrr3/s14/R2020100317_F006_V2020100323_S14_ttd.gif
string UtilityModelSpcHrrrInputOutput::getValidTime(const string& run, const string& validTimeForecast, const string& validTime) {
    string validTimeCurrent;
    if (run.size() == 10 && validTime.size() == 10) {
        auto runTimePrefix = UtilityString::substring(run, 0, 8);
        auto runTimeHr = UtilityString::substring(run, 8, 2);
        auto endTimePrefix = UtilityString::substring(validTime, 0, 8);
        auto runTimeHrInt = To::Int(runTimeHr);
        auto forecastInt = To::Int(validTimeForecast);
        if ((runTimeHrInt + forecastInt) > 23) {
            validTimeCurrent = endTimePrefix + WString::fixedLengthStringPad0(To::string(runTimeHrInt + forecastInt - 24), 2);
        } else {
            validTimeCurrent = runTimePrefix + WString::fixedLengthStringPad0(To::string(runTimeHrInt + forecastInt), 2);
        }
    }
    return validTimeCurrent;
}

string UtilityModelSpcHrrrInputOutput::formatTime(const string& time) {
    return "0" + time;
}
