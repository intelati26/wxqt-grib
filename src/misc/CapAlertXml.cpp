// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "CapAlertXml.h"
#include "common/GlobalVariables.h"
#include "objects/LatLon.h"
#include "objects/WString.h"
#include "radar/RadarSites.h"
#include "util/To.h"
#include "util/UtilityString.h"

CapAlertXml::CapAlertXml(const string& s) {
    url = UtilityString::parse(s, "<id>(.*?)</id>");
    title = UtilityString::parse(s, "<title>(.*?)</title>");
    summary = UtilityString::parse(s, "<summary>(.*?)</summary>");
    instructions = UtilityString::parse(s, "</description>.*?<instruction>(.*?)</instruction>.*?<areaDesc>");
    area = UtilityString::parse(s, "<cap:areaDesc>(.*?)</cap:areaDesc>");
    area = WString::replace(area, "&apos;", "'");
    effective = UtilityString::parse(s, "<cap:effective>(.*?)</cap:effective>");
    expires = UtilityString::parse(s, "<cap:expires>(.*?)</cap:expires>");
    event = UtilityString::parse(s, "<cap:event>(.*?)</cap:event>");
    polygon = UtilityString::parse(s, "<cap:polygon>(.*?)</cap:polygon>");
    text = title;
    text += GlobalVariables::newline;
    text += "Counties: ";
    text += area;
    text += GlobalVariables::newline;
    text += summary;
    text += GlobalVariables::newline;
    text += instructions;
    text += GlobalVariables::newline;
    summary = WString::replace(summary, "<br>\\*", "<br><br>*");
    points = WString::split(polygon, " ");
}

string CapAlertXml::getClosestRadar() const {
    if (points.size() > 2) {
        const auto lat = To::Double(WString::split(points[0], ",")[0]);
        const auto lon = To::Double(WString::split(points[0], ",")[1]);
        return RadarSites::getNearestCode(LatLon{lat, lon}, false);
    } else {
        return "";
    }
}
