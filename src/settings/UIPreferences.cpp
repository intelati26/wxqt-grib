// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "settings/UIPreferences.h"
#include "settings/HomeLayout.h"
#include "util/HomeThumbnails.h"
#include <algorithm>
#include "objects/WString.h"
#include "radarcolorpalette/ColorPalettes.h"
#include "util/Utility.h"

const int UIPreferences::boxPadding{2};
const int UIPreferences::padding{4};
int UIPreferences::fontSize{13};
bool UIPreferences::unitsM{true};
bool UIPreferences::unitsF{true};
int UIPreferences::mainScreenImageSize{350};
int UIPreferences::nwsIconSize{62};
int UIPreferences::comboBoxSize{20};
int UIPreferences::toolbarIconSize{36};
bool UIPreferences::tiledWindows{false};
QMargins UIPreferences::textPadding;
const bool UIPreferences::useNwsApi{true};
const bool UIPreferences::useNwsApiForHourly{true};
bool UIPreferences::nexradMainScreen;
bool UIPreferences::mainScreenSevereDashboard;
bool UIPreferences::homeCaptions{true};
bool UIPreferences::nexradScrollWheelMotion;
bool UIPreferences::rememberGOES;
bool UIPreferences::rememberMosaic;
vector<PrefBool> UIPreferences::homeScreenItemsImage = [] {
    vector<PrefBool> items;
    for (const auto& entry : HomeThumbnails::all()) {
        items.emplace_back(entry.label, entry.token, entry.defaultOn);
    }
    return items;
}();
vector<PrefBool> UIPreferences::homeScreenItemsText{
    PrefBool{"Hourly", "HOURLY", true},
    PrefBool{"Wfo Text", "WFO_TEXT", false}
};

const string UIPreferences::homeScreenNexradToken{"NEXRAD_MAIN"};
const string UIPreferences::homeColumnImages{"IMAGES"};
const string UIPreferences::homeColumnForecast{"FORECAST"};
const string UIPreferences::homeColumnText{"TEXT"};

namespace {
    vector<string> tokensOf(const vector<PrefBool>& items, const vector<string>& leading = {}) {
        auto tokens = leading;
        for (const auto& item : items) {
            tokens.push_back(item.getPrefToken());
        }
        return tokens;
    }
}

HomeScreenOrder UIPreferences::homeScreenColumnOrder{"HOME_SCREEN_COLUMN_ORDER", {homeColumnImages, homeColumnForecast, homeColumnText}};
HomeScreenOrder UIPreferences::homeScreenImageOrder{"HOME_SCREEN_IMAGE_ORDER", tokensOf(homeScreenItemsImage, {homeScreenNexradToken})};
HomeScreenOrder UIPreferences::homeScreenTextOrder{"HOME_SCREEN_TEXT_ORDER", tokensOf(homeScreenItemsText)};

string UIPreferences::homeScreenLabel(const string& token) {
    if (token == homeColumnImages) {
        return "Images (radar, satellite, ...)";
    }
    if (token == homeColumnForecast) {
        return "Forecast (conditions, hazards, 7 day)";
    }
    if (token == homeColumnText) {
        return "Text (hourly, WFO text)";
    }
    if (token == homeScreenNexradToken) {
        return "Nexrad";
    }
    for (const auto& items : {&homeScreenItemsImage, &homeScreenItemsText}) {
        for (const auto& item : *items) {
            if (item.getPrefToken() == token) {
                return item.getLabel();
            }
        }
    }
    return token;
}

HomeScreenOrder::HomeScreenOrder(const string& prefToken, const vector<string>& defaults)
    : prefToken{prefToken}
    , defaults{defaults}
    , tokens{defaults}
{}

void HomeScreenOrder::load() {
    tokens.clear();
    const auto saved = Utility::readPref(prefToken, "");
    if (!saved.empty()) {
        for (const auto& token : WString::split(saved, ",")) {
            const auto known = std::find(defaults.begin(), defaults.end(), token) != defaults.end();
            const auto seen = std::find(tokens.begin(), tokens.end(), token) != tokens.end();
            if (known && !seen) {
                tokens.push_back(token);
            }
        }
    }
    for (const auto& token : defaults) {
        if (std::find(tokens.begin(), tokens.end(), token) == tokens.end()) {
            tokens.push_back(token);
        }
    }
}

void HomeScreenOrder::move(int from, int to) {
    const auto count = static_cast<int>(tokens.size());
    if (count < 2 || from < 0 || from >= count) {
        return;
    }
    to = ((to % count) + count) % count;
    std::swap(tokens[from], tokens[to]);
    Utility::writePref(prefToken, WString::join(tokens, ","));
}

const vector<string>& HomeScreenOrder::getTokens() const {
    return tokens;
}

void UIPreferences::initialize() {
    ColorPalettes::initialize();
    textPadding = QMargins(padding, padding, padding, padding);
    fontSize = Utility::readPrefInt("GENERAL_FONT_SIZE", UIPreferences::fontSize);
    mainScreenImageSize = Utility::readPrefInt("MAIN_SCREEN_IMAGE_SIZE", mainScreenImageSize);
    toolbarIconSize = Utility::readPrefInt("TOOLBAR_ICON_SIZE", toolbarIconSize);
    nwsIconSize = Utility::readPrefInt("NWS_ICON_SIZE_PREF", nwsIconSize);
    // useNwsApi = WString::startsWith(Utility::readPref("USE_NWS_API_SEVEN_DAY", "false"), "t");
    // useNwsApiForHourly = WString::startsWith(Utility::readPref("USE_NWS_API_HOURLY", "true"), "t");
    nexradMainScreen = WString::startsWith(Utility::readPref("NEXRAD_ON_MAIN_SCREEN", "false"), "t");
    mainScreenSevereDashboard = WString::startsWith(Utility::readPref("MAINSCREEN_SEVERE_DASH", "false"), "t");
    homeCaptions = WString::startsWith(Utility::readPref("HOME_CAPTIONS", "true"), "t");
    nexradScrollWheelMotion = WString::startsWith(Utility::readPref("NEXRAD_SCROLLWHEEL", "false"), "t");
    rememberGOES = WString::startsWith(Utility::readPref("REMEMBER_GOES", "false"), "t");
    rememberMosaic = WString::startsWith(Utility::readPref("REMEMBER_MOSAIC", "false"), "t");
    tiledWindows = WString::startsWith(Utility::readPref("TILED_WINDOWS", "false"), "t");
    homeScreenColumnOrder.load();
    HomeLayout::load();
    homeScreenImageOrder.load();
    homeScreenTextOrder.load();
}
