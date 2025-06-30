// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "UtilityHourlyOldApi.h"
#include "common/GlobalVariables.h"
#include "objects/ObjectDateTime.h"
#include "objects/WString.h"
#include "settings/Location.h"
#include "util/To.h"
#include "util/UtilityDownloadNws.h"
#include "util/UtilityIO.h"
#include "util/UtilityList.h"
#include "util/UtilityString.h"

const vector<string> UtilityHourlyOldApi::regexpList{
    "<temperature type=.hourly.*?>(.*?)</temperature>",
    "<temperature type=.dew point.*?>(.*?)</temperature>",
    "<time-layout.*?>(.*?)</time-layout>",
    "<probability-of-precipitation.*?>(.*?)</probability-of-precipitation>",
    "<cloud-amount type=.total.*?>(.*?)</cloud-amount>"
};

string UtilityHourlyOldApi::getHourlyString(int locNumber) {
    const auto latLon  = Location::getLatLon(locNumber);
    const auto html = UtilityDownloadNws::getHourlyOldData(latLon);
    const auto header = To::stringPadLeft("Time", 10) + " "
        + To::stringPadLeft("Temp", 8)
        + To::stringPadLeft("Dew", 8)
        + To::stringPadLeft("Precip", 8)
        + To::stringPadLeft("Cloud", 8)
        + GlobalVariables::newline;
    return GlobalVariables::newline + header + parseHourly(html);
}

string UtilityHourlyOldApi::parseHourly(const string& html) {
    const auto rawData = UtilityString::parseXmlExt(regexpList, html);
    auto temp2List = UtilityString::parseXmlValue(rawData[0]);
    auto temp3List = UtilityString::parseXmlValue(rawData[1]);
    auto time2List = UtilityString::parseXml(rawData[2], "start-valid-time");
    auto temp4List = UtilityString::parseXmlValue(rawData[3]);
    auto temp5List = UtilityString::parseXmlValue(rawData[4]);
    auto temp2Len = temp2List.size();
    auto temp3Len = temp3List.size();
    auto temp4Len = temp4List.size();
    auto temp5Len = temp5List.size();
    string sb;
    for (auto j : range2(1, temp2Len)) {
        string temp3Val = ".";
        string temp4Val = ".";
        string temp5Val = ".";
        if (temp2Len == temp3Len) {
            temp3Val = temp3List[j];
        }
        if (temp2Len == temp4Len) {
            temp4Val = temp4List[j];
        }
        if (temp2Len == temp5Len) {
            temp5Val = temp5List[j];
        }
        const auto time = ObjectDateTime::translateTimeForHourly(time2List[j]);
        sb += To::stringPadLeft(time, 8);
        sb += "   ";
        sb += To::stringPadLeft(temp2List[j], 8);
        sb += To::stringPadLeft(temp3Val, 8);
        sb += To::stringPadLeft(temp4Val, 8);
        sb += To::stringPadLeft(temp5Val, 8);
        sb += GlobalVariables::newline;
    }
    return sb;
}
