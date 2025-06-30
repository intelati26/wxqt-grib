// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "UtilityModelNsslWrfInputOutput.h"
#include "objects/ObjectDateTime.h"
#include "objects/WString.h"
#include "util/UtilityIO.h"
#include "util/UtilityList.h"
#include "util/UtilityString.h"
#include "UtilityModelNsslWrfInterface.h"
#include "UtilityModels.h"

const string UtilityModelNsslWrfInputOutput::baseUrl{"https://cams.nssl.noaa.gov"};

RunTimeData UtilityModelNsslWrfInputOutput::getRunTime() {
    RunTimeData runData;
    const auto htmlRunstatus = UtilityIO::getHtml(baseUrl);
    const auto html = UtilityString::parse(htmlRunstatus, "\\{model: \"fv3nssl\",(rd: .[0-9]{8}\",rt: .[0-9]{4}\",)");
    auto day = UtilityString::parse(html, "rd:.(.*?),.*?");
    day = WString::replace(day, "\"", "");
    auto time = UtilityString::parse(html, "rt:.(.*?)00.,.*?");
    time = WString::replace(time, "\"", "");
    const string mostRecentRun = day + time;
    runData.appendListRun(mostRecentRun);
    runData.appendListRunWithList(ObjectDateTime::generateModelRuns(mostRecentRun, 12));
    runData.mostRecentRun = mostRecentRun;
    return runData;
}

string UtilityModelNsslWrfInputOutput::getImageUrl(ObjectModel * om) {
    const auto sectorIndex = indexOf(UtilityModelNsslWrfInterface::sectorsLong, om->sector);
    const auto& sector = UtilityModelNsslWrfInterface::sectors[sectorIndex];
    const auto baseLayerUrl = "https://cams.nssl.noaa.gov/graphics/blankmaps/spc" + sector + ".png";
    string modelPostfix = "nssl";
    auto model = WString::toLower(om->model);
    if (om->model == "HRRRV3") {
        modelPostfix = "";
    }
    if (om->model == "WRF3KM") {
        model = "wrfnssl3km";
        modelPostfix = "";
    }
    const auto year = UtilityString::substring(om->run, 0, 4);
    const auto month = UtilityString::substring(om->run, 4, 6);
    const auto day = UtilityString::substring(om->run, 6, 8);
    const auto hour = UtilityString::substring(om->run, 8, 10);
    string url = baseUrl +
            "/graphics/models/" +
            model +
            modelPostfix +
            "/" +
            year +
            "/" +
            month +
            "/" +
            day +
            "/" +
            hour +
            "00/f0" +
            om->getTime() +
            "00/" +
            om->param +
            ".spc" +
            WString::toLower(sector) +
            ".f0" +
            om->getTime() +
            "00.png";
    return url + UtilityModels::urlSeperator + baseLayerUrl;
}
