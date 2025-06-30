// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "settings/UIPreferences.h"
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
bool UIPreferences::nexradScrollWheelMotion;
bool UIPreferences::rememberGOES;
bool UIPreferences::rememberMosaic;
vector<PrefBool> UIPreferences::homeScreenItemsImage{
    PrefBool{"Radar Mosaic", "RADAR_MOSAIC", true},
    PrefBool{"Visible Satellite", "VISIBLE_SATELLITE", true},
    PrefBool{"Alerts", "USWARN", false},
    PrefBool{"Analysis", "ANALYSIS_RADAR_AND_WARNINGS", false},
    PrefBool{"RTMA Temp", "RTMA_TEMP", false},
    PrefBool{"SPC Meso - MSLP", "SPC_MESO_MSLP", false},
    PrefBool{"SPC Meso - 500mb", "SPC_MESO_500MB", false},
};
vector<PrefBool> UIPreferences::homeScreenItemsText{
    PrefBool{"Hourly", "HOURLY", true},
    PrefBool{"Wfo Text", "WFO_TEXT", false}
};

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
    nexradScrollWheelMotion = WString::startsWith(Utility::readPref("NEXRAD_SCROLLWHEEL", "false"), "t");
    rememberGOES = WString::startsWith(Utility::readPref("REMEMBER_GOES", "false"), "t");
    rememberMosaic = WString::startsWith(Utility::readPref("REMEMBER_MOSAIC", "false"), "t");
    tiledWindows = WString::startsWith(Utility::readPref("TILED_WINDOWS", "false"), "t");
}
