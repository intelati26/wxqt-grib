// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "UtilityModelEsrlInputOutput.h"
#include "objects/WString.h"
#include "util/UtilityIO.h"
#include "util/UtilityList.h"
#include "util/UtilityString.h"

const string UtilityModelEsrlInputOutput::pattern1{"<option selected>([0-9]{2} \\w{3} [0-9]{4} - [0-9]{2}Z)<.option>"};
const string UtilityModelEsrlInputOutput::pattern2{"<option>([0-9]{2} \\w{3} [0-9]{4} - [0-9]{2}Z)<.option>"};
const string UtilityModelEsrlInputOutput::pattern3{"[0-9]{2} \\w{3} ([0-9]{4}) - [0-9]{2}Z"};
const string UtilityModelEsrlInputOutput::pattern4{"([0-9]{2}) \\w{3} [0-9]{4} - [0-9]{2}Z"};
const string UtilityModelEsrlInputOutput::pattern5{"[0-9]{2} \\w{3} [0-9]{4} - ([0-9]{2})Z"};
const string UtilityModelEsrlInputOutput::pattern6{"[0-9]{2} (\\w{3}) [0-9]{4} - [0-9]{2}Z"};

RunTimeData UtilityModelEsrlInputOutput::getRunTime(ObjectModel * om) {
    RunTimeData runData;
    string htmlRunstatus;
    if (om->model == "HRRR_AK") {
        htmlRunstatus = UtilityIO::getHtml("https://rapidrefresh.noaa.gov/alaska/");
    } else if (om->model == "RAP_NCEP") {
        htmlRunstatus = UtilityIO::getHtml("https://rapidrefresh.noaa.gov/RAP/Welcome.cgi?dsKey=" + WString::toLower(om->model) + "_jet&domain=full");
    } else if (om->model == "RAP") {
        htmlRunstatus = UtilityIO::getHtml("https://rapidrefresh.noaa.gov/RAP/");
    } else if (om->model == "HRRR_NCEP") {
        htmlRunstatus = UtilityIO::getHtml("https://rapidrefresh.noaa.gov/hrrr/HRRR/Welcome.cgi?dsKey=" + WString::toLower(om->model) + "_jet&domain=full");
    } else if (om->model == "HRRR_SMOKE") {
        htmlRunstatus = UtilityIO::getHtml("https://rapidrefresh.noaa.gov/hrrr/HRRRsmoke/");
    } else {
        htmlRunstatus = UtilityIO::getHtml("https://rapidrefresh.noaa.gov/" +
                        WString::toLower(om->model) +
                        "/" +
                        om->model +
                        "/Welcome.cgi?dsKey=" +
                        WString::toLower(om->model) +
                        "_jet&domain=full");
    }
    auto html = UtilityString::parse(htmlRunstatus, pattern1);
    const auto oldRunTimes = UtilityString::parseColumn(htmlRunstatus, pattern2);
    const auto year1 = UtilityString::parse(html, pattern3);
    const auto day1 = UtilityString::parse(html, pattern4);
    const auto hour1 = UtilityString::parse(html, pattern5);
    const auto monthStr1 = monthWordToNumber(UtilityString::parse(html, pattern6));
    html = year1 + monthStr1 + day1 + hour1;
    runData.appendListRun(html);
    runData.mostRecentRun = html;
    if (!html.empty()) {
        for (auto data : range3(0, 12, 1)) {
            const auto year = UtilityString::parse(oldRunTimes[data], pattern3);
            const auto day = UtilityString::parse(oldRunTimes[data], pattern4);
            const auto hour = UtilityString::parse(oldRunTimes[data], pattern5);
            const auto monthStr = monthWordToNumber(UtilityString::parse(oldRunTimes[data], pattern6));
            runData.appendListRun(year + monthStr + day + hour);
        }
        runData.timeStringConversion = UtilityString::parse(html, "([0-9]{2})$");
    }
    return runData;
}

string UtilityModelEsrlInputOutput::getImageUrl(ObjectModel * om) {
    auto parentModel = WString::replace(om->model, "HRRR_AK", "alaska");
    if (om->model == "RAP_NCEP") {
        parentModel = "RAP";
    } else if (om->model == "HRRR_NCEP") {
        parentModel = "HRRR";
    } else if (om->model == "HRRR_SMOKE") {
        parentModel = "HRRR";
    } else {
    }
    string imgUrl;
    string ondemandUrl;
    auto sectorLocal = WString::replace(om->sector, " ", "");
    sectorLocal = WString::replace(sectorLocal, "Full", "full");
    sectorLocal = WString::replace(sectorLocal, "CONUS", "conus");
    sectorLocal = WString::replace(sectorLocal, "Seattle-Portland", "SEA-POR");
    sectorLocal = WString::replace(sectorLocal, "Chicago-Detroit", "CHI-DET");
    sectorLocal = WString::replace(sectorLocal, "NYC-Boston", "NYC-BOS");
    sectorLocal = WString::replace(sectorLocal, "Wash-Balt-Phil", "DCArea");
    sectorLocal = WString::replace(sectorLocal, "SouthFlorida", "SouthFL");
    sectorLocal = WString::replace(sectorLocal, "Atlanta-AL-GA-SC", "ATL");
    sectorLocal = WString::replace(sectorLocal, "SouthernCA", "SouthCA");
    sectorLocal = WString::replace(sectorLocal, "NW-Large", "NW-large");

    auto param = WString::replace(om->param, "_full_", "_" + sectorLocal + "_");
    if (WString::contains(parentModel, "RAP")) {
        imgUrl = "https://rapidrefresh.noaa.gov/" +
                parentModel +
                "/for_web/" +
                WString::toLower(om->model) +
                "_jet/" +
                WString::replace(om->run, "Z", "") +
                "/" +
                sectorLocal +
                "/" +
                param +
                "_f0" +
                om->getTime() +
                ".png";
        ondemandUrl = "https://rapidrefresh.noaa.gov/" +
                parentModel +
                "/" +
                "displayMapUpdated" +
                ".cgi?keys=" +
                WString::toLower(om->model) +
                "_jet:&runtime=" +
                WString::replace(om->run, "Z", "") +
                "&plot_type=" +
                param +
                "&fcst=" +
                om->getTime() +
                "&time_inc=60&num_times=16&model=" +
                WString::toLower(om->model) +
                "&ptitle=" +
                om->model +
                "%20Model%20Fields%20-%20Experimental&maxFcstLen=15&fcstStrLen=-1&domain=" +
                sectorLocal +
                "&adtfn=1";
    } else if (om->model == "HRRR_SMOKE") {
        const string modelName = "HRRRsmoke";
        imgUrl = "https://rapidrefresh.noaa.gov/hrrr/" +
                modelName +
                "/for_web/" +
                WString::toLower(parentModel) + "_ncep"
                "_jet/" +
                WString::replace(om->run, "Z", "") +
                "/" +
                sectorLocal +
                "/" +
                param +
                "_f0" +
                om->getTime() +
                ".png";
        ondemandUrl = "https://rapidrefresh.noaa.gov/hrrr/" +
                modelName +
                "/" +
                "displayMapUpdated" +
                ".cgi?keys=" +
                WString::toLower(parentModel) + "_ncep"
                "_jet:&runtime=" +
                WString::replace(om->run, "Z", "") +
                "&plot_type=" +
                param +
                "&fcst=" +
                om->getTime() +
                "&time_inc=60&num_times=16&model=" +
                WString::toLower(parentModel) +
                "&ptitle=" +
                "HRRR-Smoke%20Graphics&maxFcstLen=15&fcstStrLen=-1&domain=" +
                sectorLocal +
                "&adtfn=1";
    } else {
        imgUrl = "https://rapidrefresh.noaa.gov/hrrr/" +
                WString::toUpper(parentModel) +
                "/for_web/" +
                WString::toLower(om->model) +
                "_jet/" +
                WString::replace(om->run, "Z", "") +
                "/" +
                sectorLocal +
                "/" +
                param +
                "_f0" +
                om->getTime() +
                ".png";
        ondemandUrl = "https://rapidrefresh.noaa.gov/hrrr/" +
                WString::toUpper(parentModel) +
                "/" +
                "displayMapUpdated" +
                ".cgi?keys=" +
                WString::toLower(om->model) +
                "_jet:&runtime=" +
                WString::replace(om->run, "Z", "") +
                "&plot_type=" +
                param +
                "&fcst=" +
                om->getTime() +
                "&time_inc=60&num_times=16&model=" +
                WString::toLower(om->model) +
                "&ptitle=" +
                om->model +
                "%20Model%20Fields%20-%20Experimental&maxFcstLen=15&fcstStrLen=-1&domain=" +
                sectorLocal +
                "&adtfn=1";
    }
    UtilityIO::getHtml(ondemandUrl);
    return imgUrl;
}

string UtilityModelEsrlInputOutput::monthWordToNumber(const string& month) {
    auto tmp = month;
    tmp = WString::replace(tmp, "Jan", "01");
    tmp = WString::replace(tmp, "Feb", "02");
    tmp = WString::replace(tmp, "Mar", "03");
    tmp = WString::replace(tmp, "Apr", "04");
    tmp = WString::replace(tmp, "May", "05");
    tmp = WString::replace(tmp, "Jun", "06");
    tmp = WString::replace(tmp, "Jul", "07");
    tmp = WString::replace(tmp, "Aug", "08");
    tmp = WString::replace(tmp, "Sep", "09");
    tmp = WString::replace(tmp, "Oct", "10");
    tmp = WString::replace(tmp, "Nov", "11");
    tmp = WString::replace(tmp, "Dec", "12");
    return tmp;
}
