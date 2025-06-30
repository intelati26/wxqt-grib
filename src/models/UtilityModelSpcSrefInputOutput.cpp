// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "UtilityModelSpcSrefInputOutput.h"
#include "common/GlobalVariables.h"
#include "objects/WString.h"
#include "util/UtilityIO.h"
#include "util/UtilityString.h"

const string UtilityModelSpcSrefInputOutput::srefPattern2{"([0-9]{10}z</a>&nbsp in through <b>f[0-9]{3})"};
const string UtilityModelSpcSrefInputOutput::srefPattern3{"<tr><td class=.previous.><a href=sref.php\\?run=[0-9]{10}&id=SREF_H5__>([0-9]{10}z)</a></td></tr>"};

RunTimeData UtilityModelSpcSrefInputOutput::getRunTime() {
    RunTimeData runData;
    const auto html = UtilityIO::getHtml(GlobalVariables::nwsSPCwebsitePrefix + "/exper/sref/");
    const auto tmpTxt = UtilityString::parse(html, srefPattern2);
    const auto result = UtilityString::parseColumn(html, srefPattern3);
    const auto latestRun = WString::split(tmpTxt, "</a>")[0];
    runData.appendListRun(WString::replace(latestRun, "z", ""));
    if (!result.empty()) {
        for (const auto& data : result) {
            runData.appendListRun(WString::replace(data, "z", ""));
        }
    }
    if (!runData.listRun.empty()) {
        runData.mostRecentRun = runData.listRun[0];
    }
    return runData;
}

string UtilityModelSpcSrefInputOutput::getImageUrl(ObjectModel * om) {
    return GlobalVariables::nwsSPCwebsitePrefix + "/exper/sref/gifs/" + WString::replace(om->run, "z", "") + "/" + om->param + "f" + om->getTime() + ".gif";
}
