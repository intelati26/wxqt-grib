// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "UtilityModelNcepInputOutput.h"
#include "objects/WString.h"
#include "util/UtilityIO.h"
#include "util/UtilityString.h"

// const string UtilityModelNcepInputOutput::pattern{"current_cycle_white . .([0-9 ]{11} UTC)"};  //  "data-cycle-date=.([0-9 ]{11} UTC)"
const string UtilityModelNcepInputOutput::pattern{"data-cycle-date=.([0-9 ]{11} UTC)"};

RunTimeData UtilityModelNcepInputOutput::getRunTime(ObjectModel * om) {
    RunTimeData runData;
    const auto url = ("https://mag.ncep.noaa.gov/model-guidance-model-parameter.php?group=Model%20Guidance&model=" +
            WString::toUpper(om->model) +
            "&area=" +
            om->sector +
            "&ps=area");
    auto html = UtilityIO::getHtml(url);
    html = UtilityString::parse(html, pattern);
    html = WString::replace(html, "UTC", "Z");
    html = WString::replace(html, " ", "");
    auto runCompletionDataStr = WString::replace(html, "Z", " UTC");
    if (!runCompletionDataStr.empty()) {
        runCompletionDataStr = UtilityString::insert(runCompletionDataStr, 8, " ");
    }
    auto runCompletionUrl = "https://mag.ncep.noaa.gov/model-guidance-model-parameter.php?group=Model%20Guidance&model=" + WString::toUpper(om->model);
    runCompletionUrl += "&area=" + WString::toLower(om->sector);
    runCompletionUrl += "&cycle=" + runCompletionDataStr;
    runCompletionUrl += "&param=" + om->param + "&fourpan=no&imageSize=M&ps=area";
    runCompletionUrl = WString::replace(runCompletionUrl, " ", "%20");
    const string ncepPattern1 = "([0-9]{2}Z)";
    const auto time = UtilityString::parse(html, ncepPattern1);
    runData.mostRecentRun = time;
    runData.timeStringConversion = time;
    const auto timeCompleteUrl = "https://mag.ncep.noaa.gov/model-fhrs.php?group=Model%20Guidance&model=" +
            WString::toLower(om->model) +
            "&fhrmode=image&loopstart=-1&loopend=-1&area=" +
            om->sector +
            "&fourpan=no&imageSize=&preselectedformattedcycledate=" +
            runCompletionDataStr +
            "&cycle=" +
            runCompletionDataStr +
            "&param=" +
            om->param +
            "&ps=area";
    const auto timeCompleteHTML = UtilityIO::getHtml(WString::replace(timeCompleteUrl, " ", "%20"));
    return runData;
}

string UtilityModelNcepInputOutput::getImageUrl(ObjectModel * om) {
    string imgUrl;
    auto timeLocal = om->getTime();
    if (om->model == "HRRR") {
        timeLocal = om->getTime() + "00";
    }
    if (om->model == "GFS") {
        imgUrl = "https://mag.ncep.noaa.gov/data/" +
                WString::toLower(om->model) +
                "/" +
                WString::replace(om->run, "Z", "") +
                "/" +
                WString::toLower(om->sector) +
                "/" +
                om->param +
                "/" +
                WString::toLower(om->model) +
                "_" +
                WString::toLower(om->sector) +
                "_" +
                timeLocal +
                "_" +
                om->param +
                ".gif";
    } else {
        imgUrl = "https://mag.ncep.noaa.gov/data/" +
                WString::toLower(om->model) +
                "/" +
                WString::replace(om->run, "Z", "") +
                "/" +
                WString::toLower(om->model) +
                "_" +
                WString::toLower(om->sector) +
                "_" +
                timeLocal +
                "_" +
                om->param +
                ".gif";
    }
    return imgUrl;
}
