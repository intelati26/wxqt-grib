// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "UtilityModelWpcGefsInputOutput.h"
#include "common/GlobalVariables.h"
#include "objects/ObjectDateTime.h"
#include "objects/WString.h"

RunTimeData UtilityModelWpcGefsInputOutput::getRunTime() {
    RunTimeData runData;
    const auto currentHour = ObjectDateTime::getCurrentHourInUTC();
    runData.mostRecentRun = "00";
    if (currentHour >= 12 && currentHour < 18) {
        runData.mostRecentRun = "06";
    }
    if (currentHour >= 18) {
        runData.mostRecentRun = "12";
    }
    if (currentHour < 6) {
        runData.mostRecentRun = "18";
    }
    runData.listRun = {"00", "06", "12", "18"};
    runData.timeStringConversion = runData.mostRecentRun;
    return runData;
}

string UtilityModelWpcGefsInputOutput::getImageUrl(ObjectModel * om) {
    string sectorAdd;
    if (om->sector == "AK") {
        sectorAdd = "ak";
    }
    return GlobalVariables::nwsWPCwebsitePrefix + "/exper/gefs/" + om->run + "/GEFS_" + om->param + "_" + om->run + "Z_f" + WString::split(om->getTime(), " ")[0] + sectorAdd + ".gif";
}
