// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "spc/SpcMcdWatchMpdViewer.h"
#include "misc/ImageViewer.h"
#include "objects/FutureBytes.h"
#include "objects/FutureText.h"
#include "objects/LatLon.h"
#include "objects/PolygonWatch.h"
#include "objects/Route.h"
#include "objects/WString.h"
#include "radar/RadarSites.h"
#include "settings/UtilityLocation.h"
#include "util/UtilityString.h"

SpcMcdWatchMpdViewer::SpcMcdWatchMpdViewer(Window * parent, const string& url)
    : Window{parent}
    , parent{parent}
    , text{this}
    , photo{this, Scaled}
    , sw{this, photo, boxText}
    , button{this, None, "Radar"}
    , token{getToken(url) }
    , shortcut{QKeySequence{"R"}, this}
    , shortcutImage{QKeySequence{"1"}, this}
{
    setTitle(token);
    boxText.addWidget(button);
    boxText.addWidget(text);
    photo.connect([this, parent] { new ImageViewer{parent, photo.bytes}; });
    shortcutImage.connect([this, parent] { new ImageViewer{parent, photo.bytes}; });
    new FutureBytes{this, url, [this] (const auto& ba) { photo.setBytes(ba); }};
    new FutureText{this, token, [this] (const auto& s) { updateText(s); }};
}

void SpcMcdWatchMpdViewer::updateText(const string& html) {
    text.setText(html);
    auto textWithLatLon = html;
    if (WString::contains(token, "SPCWAT")) {
        textWithLatLon = PolygonWatch::getLatLon(WString::replace(token, "SPCWAT", ""));
    }
    auto stringOfLatLon = PolygonWatch::storeWatchMcdLatLon(textWithLatLon);
    stringOfLatLon = WString::replace(stringOfLatLon, ":", "");
    const auto latLonList = LatLon::parseStringToLatLons(stringOfLatLon, -1.0, false);
    const auto center = UtilityLocation::getCenterOfPolygon(latLonList);
    const auto radarSite = RadarSites::getNearestCode(center, false);
    radar = radarSite;
    const auto buttonRadarText = "Show Radar - " + radarSite;
    button.setText(buttonRadarText);
    button.connect([this, radarSite] { Route::nexradRadarSpecificSite(this, radarSite); });
    shortcut.connect([this, radarSite] { Route::nexradRadarSpecificSite(this, radarSite);; });
}

string SpcMcdWatchMpdViewer::getToken(const string& url) {
    const auto items = WString::split(url, "/");
    auto s = items.back();
    s = WString::replace(s, ".gif", "");
    s = WString::replace(s, ".png", "");
    s = WString::toUpper(s);
    const auto tokenTrimmed = UtilityString::substring(s, s.size() - 4);
    if (WString::contains(url, "www.wpc.ncep.noaa.gov")) {
        s = "WPCMPD" + UtilityString::substring(s, s.size() - 4, s.size());
    } else if (WString::contains(url, "www.spc.noaa.gov") && WString::contains(url, "mcd")) {
        s = "SPCMCD" + tokenTrimmed;
    } else {
        const auto items1 = WString::split(url, "/");
        s = items1.back();
        s = WString::replace(s, "_radar.gif", "");
        s = WString::replace(s, "ww", "");
        s = "SPCWAT" + s;
    }
    return s;
}
