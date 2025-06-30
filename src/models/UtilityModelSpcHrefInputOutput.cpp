// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "UtilityModelSpcHrefInputOutput.h"
#include <vector>
#include "common/GlobalVariables.h"
#include "objects/ObjectDateTime.h"
#include "objects/WString.h"
#include "util/UtilityIO.h"
#include "util/UtilityList.h"
#include "util/UtilityString.h"
#include "UtilityModelSpcHrefInterface.h"

RunTimeData UtilityModelSpcHrefInputOutput::getRunTime() {
    RunTimeData runData;
    const auto htmlRunstatus = UtilityIO::getHtml(GlobalVariables::nwsSPCwebsitePrefix + "/exper/href/");
    const auto html = UtilityString::parse(htmlRunstatus, "\\{model: \"href\",product: \"500mbmean\",sector: \"conus\",(rd: .[0-9]{8}\",rt: .[0-9]{4}\",\\})");
    auto day = UtilityString::parse(html, "rd:.(.*?),.*?");
    day = WString::replace(day, "\"", "");
    auto time = UtilityString::parse(html, "rt:.(.*?)00.,.*?");
    time = WString::replace(time, "\"", "");
    const auto mostRecentRun = day + time;
    runData.appendListRun(mostRecentRun);
    runData.appendListRunWithList(ObjectDateTime::generateModelRuns(mostRecentRun, 12));
    runData.mostRecentRun = mostRecentRun;
    return runData;
}

string UtilityModelSpcHrefInputOutput::getImageUrl(ObjectModel * om) {
    const auto year = UtilityString::substring(om->run, 0, 4);
    const auto month = UtilityString::substring(om->run, 4, 6);
    const auto day = UtilityString::substring(om->run, 6, 8);
    const auto hour = UtilityString::substring(om->run, 8, 10);
    const auto products = WString::split(om->param, ",");
    vector<string> urlArr;
    urlArr.push_back(GlobalVariables::nwsSPCwebsitePrefix + "/exper/href/graphics/spcwhite1050px.png");
    urlArr.push_back(GlobalVariables::nwsSPCwebsitePrefix + "/exper/href/graphics/noaaoverlay1050px.png");
    const auto sectorIndex = indexOf(UtilityModelSpcHrefInterface::sectorsLong, om->sector);
    const auto& sector = UtilityModelSpcHrefInterface::sectors[sectorIndex];
    for (const auto& data : products) {
        string url;
        if (WString::contains(data, "crefmembers")) {
            auto paramArr = WString::split(data, " ");
            url = GlobalVariables::nwsSPCwebsitePrefix +
                    "/exper/href/graphics/models/href/" +
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
                    paramArr[0] +
                    "." +
                    sector +
                    ".f0" +
                    om->getTime() +
                    "00." +
                    paramArr[1] +
                    ".tl00.png";
        } else {
            url = GlobalVariables::nwsSPCwebsitePrefix +
                    "/exper/href/graphics/models/href/" +
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
                    data +
                    "." +
                    sector +
                    ".f0" +
                    om->getTime() +
                    "00.png";
        }
        urlArr.push_back(url);
    }
    urlArr.push_back(GlobalVariables::nwsSPCwebsitePrefix + "/exper/href/graphics/blankmaps/" + sector + ".png");
    return WString::join(urlArr, ",");
}
