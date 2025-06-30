// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "ObjectWarning.h"
#include "common/GlobalVariables.h"
#include "objects/WString.h"
#include "radar/NexradUtil.h"
#include "radar/RadarSites.h"
#include "util/Utility.h"
#include "util/UtilityList.h"
#include "util/UtilityString.h"

ObjectWarning::ObjectWarning(
    const string& url,
    const string& title,
    const string& area,
    const string& effective,
    const string& expires,
    const string& event,
    const string& sender,
    const string& polygon,
    const string& vtec
)
    : url{url}
    , title{title}
    , area{area}
    , effective{effective}
    , expires{expires}
    , event{event}
    , sender{sender}
    , polygon{polygon}
    , vtec{vtec}
    , isCurrent{NexradUtil::isVtecCurrent(vtec)}
{
    this->effective = WString::replace(this->effective, "T", " ");
    this->effective = UtilityString::replaceRegex(this->effective, ":00-0[0-9]:00", "");
    this->expires = WString::replace(this->expires, "T", " ");
    this->expires = UtilityString::replaceRegex(this->expires, ":00-0[0-9]:00", "");
    if (WString::startsWith(vtec, "O.EXP") || WString::startsWith(vtec, "O.CAN")) {
        isCurrent = false;
    }
}

vector<ObjectWarning> ObjectWarning::parseJson(const string& htmlF) {
    vector<ObjectWarning> warnings;
    const auto html = WString::replace(htmlF, "\"geometry\": null,", "\"geometry\": null, \"coordinates\":[[]]}");
    const auto urlList = UtilityString::parseColumn(html, "\"id\": \"(https://api.weather.gov/alerts/urn.*?)\"");
    const auto titleList = UtilityString::parseColumn(html, "\"description\": \"(.*?)\"");
    const auto areaDescList = UtilityString::parseColumn(html, "\"areaDesc\": \"(.*?)\"");
    const auto effectiveList = UtilityString::parseColumn(html, "\"effective\": \"(.*?)\"");
    const auto expiresList = UtilityString::parseColumn(html, "\"expires\": \"(.*?)\"");
    const auto eventList = UtilityString::parseColumn(html, "\"event\": \"(.*?)\"");
    const auto senderNameList = UtilityString::parseColumn(html, "\"senderName\": \"(.*?)\"");
    auto data = html;
    data = WString::replace(data, "\n", "");
    data = WString::replace(data, " ", "");
    const auto listOfPolygonRaw = UtilityString::parseColumn(data, GlobalVariables::warningLatLonPattern);
    const auto vtecs = UtilityString::parseColumn(html, GlobalVariables::vtecPattern);
    for (auto index : range(urlList.size())) {
        warnings.emplace_back(
            Utility::safeGet(urlList, index),
            Utility::safeGet(titleList, index),
            Utility::safeGet(areaDescList, index),
            Utility::safeGet(effectiveList, index),
            Utility::safeGet(expiresList, index),
            Utility::safeGet(eventList, index),
            Utility::safeGet(senderNameList, index),
            Utility::safeGet(listOfPolygonRaw, index),
            Utility::safeGet(vtecs, index));
    }
    return warnings;
}

string ObjectWarning::getClosestRadar() const {
    auto data = WString::replace(polygon, "[", "");
    data = WString::replace(data, "]", "");
    data = WString::replace(data, ",", " ");
    data = WString::replace(data, "-", "");
    const auto points = WString::split(data, " ");
    if (points.size() > 2) {
        const auto lat = points[1];
        const auto lon = "-" + points[0];
        return RadarSites::getNearestCode(LatLon{lat, lon}, false);
    } else {
        return "";
    }
}

string ObjectWarning::getUrl() const {
    return url;
}

vector<LatLon> ObjectWarning::getPolygonAsLatLons(int multiplier) const {
    auto polygonTmp = WString::replace(polygon, "[", "");
    polygonTmp = WString::replace(polygonTmp, "]", "");
    polygonTmp = WString::replace(polygonTmp, ",", " ");
    return LatLon::parseStringToLatLons(polygonTmp, multiplier, true);
}
