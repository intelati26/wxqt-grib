// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "CapAlert.h"
#include "common/GlobalVariables.h"
#include "objects/WString.h"
#include "util/UtilityIO.h"
#include "util/UtilityString.h"

CapAlert::CapAlert() = default;

CapAlert::CapAlert(const string& url)
    : url{url}
{
    const auto html = UtilityIO::getHtml(url);
    title = UtilityString::parse(html, "\"headline\": \"(.*?)\"");
    summary = UtilityString::parse(html, "\"description\": \"(.*?)\"");
    instructions = UtilityString::parse(html, "\"instruction\": \"(.*?)\"");
    area = UtilityString::parse(html, "\"areaDesc\": \"(.*?)\"");
    summary = WString::replace(summary, "\\n", "\n");
    instructions = WString::replace(instructions, "\\n", "\n");
    windThreat = UtilityString::parse(html, "\"windThreat\": \\[.*?\"(.*?)\".*?\\],");
    maxWindGust = UtilityString::parse(html, "\"maxWindGust\": \\[.*?\"(.*?)\".*?\\],");
    hailThreat = UtilityString::parse(html, "\"hailThreat\": \\[.*?\"(.*?)\".*?\\],");
    maxHailSize = UtilityString::parse(html, "\"maxHailSize\": \\[\\s*(.*?)\\s*\\],");
    tornadoThreat = UtilityString::parse(html, "\"tornadoDetection\": \\[.*?\"(.*?)\".*?\\],");
    nwsHeadLine = UtilityString::parse(html, "\"NWSheadline\": \\[.*?\"(.*?)\".*?\\],");
    motion = UtilityString::parse(html, "\"eventMotionDescription\": \\[.*?\"(.*?)\".*?\\],");
    vtec = UtilityString::parse(html, "\"VTEC\": \\[.*?\"(.*?)\".*?\\],");
    text = "";
    text += title;
    text += GlobalVariables::newline;
    text += "Counties: ";
    text += area;
    text += GlobalVariables::newline;
    if (!nwsHeadLine.empty()) {
        text += "..." + nwsHeadLine + "..." + GlobalVariables::newline + GlobalVariables::newline;
    }
    text += summary;
    text += GlobalVariables::newline + GlobalVariables::newline;
    if (!instructions.empty()) {
        text += "PRECAUTIONARY/PREPAREDNESS ACTIONS..." + GlobalVariables::newline + GlobalVariables::newline;
        text += instructions;
    }
    text += GlobalVariables::newline + GlobalVariables::newline;
    if (!windThreat.empty()) {
        text += "WIND THREAT..." + windThreat;
        text += GlobalVariables::newline;
    }
    if (!maxWindGust.empty() && maxWindGust != "0") {
        text += "MAX WIND GUST..." + maxWindGust;
        text += GlobalVariables::newline;
    }
    if (!hailThreat.empty()) {
        text += "HAIL THREAT..." + hailThreat;
        text += GlobalVariables::newline;
        text += "MAX HAIL SIZE..." + maxHailSize + " in";
        text += GlobalVariables::newline;
    }
    if (!tornadoThreat.empty()) {
        text += "TORNADO THREAT..." + tornadoThreat;
        text += GlobalVariables::newline;
    }
    text += motion + GlobalVariables::newline;
    text += vtec + GlobalVariables::newline;
}
