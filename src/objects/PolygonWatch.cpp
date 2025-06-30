// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "PolygonWatch.h"
#include "common/GlobalVariables.h"
#include "objects/Color.h"
#include "objects/WString.h"
#include "util/DownloadText.h"
#include "util/To.h"
#include "util/Utility.h"
#include "util/UtilityIO.h"
#include "util/UtilityString.h"

unordered_map<PolygonType, unique_ptr<PolygonWatch>> PolygonWatch::byType;
DataStorage PolygonWatch::watchLatlonCombined{"WATCH_LATLON_COMBINED"};

const vector<PolygonType> PolygonWatch::polygonList{
    Watch,
    WatchTornado,
    Mcd,
    Mpd
};

const unordered_map<PolygonType, string> PolygonWatch::namesByEnumId{
    {Mcd, "mcd"},
    {Mpd, "mpd"},
    {Watch, "watch"},
    {WatchTornado, "watchTornado"},
};

const unordered_map<PolygonType, int> PolygonWatch::colorDefaultByType{
    {Mcd, Color::rgb(153, 51, 255)},
    {Mpd, Color::rgb(0, 255, 0)},
    {Watch, Color::rgb(255, 187, 0)},
    {WatchTornado, Color::rgb(255, 0, 0)},
};

const unordered_map<PolygonType, string> PolygonWatch::colorPrefByType{
    {Mcd, "RADAR_COLOR_MCD"},
    {Mpd, "RADAR_COLOR_MPD"},
    {Watch, "RADAR_COLOR_TSTORM_WATCH"},
    {WatchTornado, "RADAR_COLOR_TOR_WATCH"},
};

PolygonWatch::PolygonWatch(PolygonType type)
    : type{type}
    , latLonList{getPrefTokenLatLon()}
    , numberList{getPrefTokenNumberList()}
    , timer{"WATCH_" + getTypeName()}
    , storage{getPrefTokenStorage()}
    , colorInt{Utility::readPrefInt(colorPrefByType.at(type), colorDefaultByType.at(type))}
    , isEnabled{WString::startsWith(Utility::readPref(prefTokenEnabled(), "false"), "t")}
{
    storage.update();
    latLonList.update();
    numberList.update();
}

void PolygonWatch::download() {
    if (timer.isRefreshNeeded()) {
        const auto html = UtilityIO::getHtml(getUrl());
        storage.setValue(html);
        if (type == Mpd) {
            string numberListString;
            string latLonString;
            const auto numbers = UtilityString::parseColumn(storage.getValue(), ">MPD #(.*?)</a></strong>");
            for (const auto& number : numbers) {
                const auto num = To::Int(number);
                const auto numberModified = To::stringPadLeftZeros(num, 4);
                const auto text = DownloadText::byProduct("WPCMPD" + numberModified);
                numberListString += numberModified + ":";
                latLonString += storeWatchMcdLatLon(text);
            }
            latLonList.setValue(latLonString);
            numberList.setValue(numberListString);
        } else if (type == Mcd) {
            string numberListString;
            string latLonString;
            const auto numbers = UtilityString::parseColumn(html, "<strong><a href=./products/md/md.....html.>Mesoscale Discussion #(.*?)</a></strong>");
            for (const auto& number : numbers) {
                const auto num = To::Int(number);
                const auto numberModified = To::stringPadLeftZeros(num, 4);
                const auto text = DownloadText::byProduct("SPCMCD" + numberModified);
                numberListString += numberModified + ":";
                latLonString += storeWatchMcdLatLon(text);
            }
            latLonList.setValue(latLonString);
            numberList.setValue(numberListString);
        } else if (type == Watch) {
            string numberListString;
            string latLonString;
            string latLonTorString;
            string latLonCombinedString;
            const auto numbers = UtilityString::parseColumn(html, "[om] Watch #([0-9]*?)</a>");
            for (const auto& number : numbers) {
                numberListString += To::stringPadLeftZeros(number, 4) + ":";
                const auto text = UtilityIO::getHtml(GlobalVariables::nwsSPCwebsitePrefix + "/products/watch/wou" + To::stringPadLeftZeros(number, 4) + ".html");
                const auto preText = UtilityString::parseMultiLineLastMatch(text, GlobalVariables::pre2Pattern);
                if (WString::contains(preText, "SEVERE TSTM")) {
                    latLonString += storeWatchMcdLatLon(preText);
                } else {
                    latLonTorString += storeWatchMcdLatLon(preText);
                }
                latLonCombinedString += storeWatchMcdLatLon(preText);
            }
            latLonList.setValue(latLonString);
            numberList.setValue(numberListString);
            byType[WatchTornado]->latLonList.setValue(latLonTorString);
            watchLatlonCombined.setValue(latLonCombinedString);
        }
    }
}

string PolygonWatch::getUrl() const {
    switch (type) {
        case Mcd:
            return GlobalVariables::nwsSPCwebsitePrefix + "/products/md/";
        case Watch:
            return GlobalVariables::nwsSPCwebsitePrefix + "/products/watch/";
        case WatchTornado:
            return GlobalVariables::nwsSPCwebsitePrefix + "/products/watch/";
        case Mpd:
            return GlobalVariables::nwsWPCwebsitePrefix + "/metwatch/metwatch_mpd.php";
        default:
            return "";
    }
}

string PolygonWatch::getPrefTokenStorage() const {
    return "SEVERE_DASHBOARD_" + getTypeName();
}

string PolygonWatch::prefTokenEnabled() const {
    return "RADAR_SHOW_" + WString::toUpper(getTypeName());
}

string PolygonWatch::getPrefTokenNumberList() const {
    return getTypeName() + "_NO_LIST";
}

string PolygonWatch::getPrefTokenLatLon() const {
    return getTypeName() + "_LATLON";
}

string PolygonWatch::getTypeName() const {
    return namesByEnumId.at(type);
}

void PolygonWatch::update() {
    isEnabled = WString::startsWith(Utility::readPref(prefTokenEnabled(), "false"), "t");
    colorInt = Utility::readPrefInt(colorPrefByType.at(type), colorDefaultByType.at(type));
}

void PolygonWatch::load() {
    for (auto& data : polygonList) {
        if (byType.contains(data)) {
            byType[data]->update();
        } else {
            byType[data] = std::make_unique<PolygonWatch>(data);
        }
    }
    byType[WatchTornado]->isEnabled = byType[Watch]->isEnabled;
}

string PolygonWatch::getShortName(const PolygonType& type1) {
    return WString::toUpper(namesByEnumId.at(type1));
}

string PolygonWatch::getLatLon(const string& number) {
    const auto html = UtilityIO::getHtml(GlobalVariables::nwsSPCwebsitePrefix + "/products/watch/wou" + number + ".html");
    return UtilityString::parseMultiLineLastMatch(html, GlobalVariables::pre2Pattern);
}

// 36517623 is 3651 -7623
LatLon PolygonWatch::getLatLonFromString(const string& latLonString) {
    const string latString{UtilityString::substring(latLonString, 0, 4)};
    const string lonString{UtilityString::substring(latLonString, 4, 8)};
    auto lat = To::Double(latString);
    auto lon = To::Double(lonString);
    lat /= 100.0;
    lon /= 100.0;
    if (lon < 40.0) {
        lon += 100.0;
    }
    return {lat, -1.0 * lon};
}

string PolygonWatch::storeWatchMcdLatLon(const string& html) {
    const auto coordinates = UtilityString::parseColumn(html, "([0-9]{8}).*?");
    string stringValue;
    for (const auto& coordinate : coordinates) {
        stringValue += getLatLonFromString(coordinate).printSpaceSeparated();
    }
    stringValue += ":";
    return WString::replace(stringValue, " :", ":");
}
