// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "settings/RadarPreferences.h"
#include "objects/Color.h"
#include "objects/PolygonWarning.h"
#include "objects/PolygonWatch.h"
#include "objects/WString.h"
#include "radar/RadarGeometry.h"
#include "ui/TextViewMetal.h"
#include "util/Utility.h"

bool RadarPreferences::rememberLocation;
bool RadarPreferences::swo;
bool RadarPreferences::fire;
bool RadarPreferences::locationDot;
bool RadarPreferences::locdotFollowsGps;
bool RadarPreferences::cities;
bool RadarPreferences::countyLabels;
bool RadarPreferences::obs;
bool RadarPreferences::obsWindbarbs;
bool RadarPreferences::sti;
bool RadarPreferences::hailIndex;
bool RadarPreferences::tvs;
bool RadarPreferences::wpcFronts;
bool RadarPreferences::dualpaneshareposn;
bool RadarPreferences::showControls;
bool RadarPreferences::colorLegend;
int RadarPreferences::textSize;
double RadarPreferences::warnLinesize;
double RadarPreferences::watmcdLinesize;
// double RadarPreferences::gpsCircleLinesize = 0.0;
double RadarPreferences::stiLinesize;
double RadarPreferences::swoLinesize;
double RadarPreferences::wbLinesize;
// was 10.0 for vala/gtk port
double RadarPreferences::lineFactor{20.0};
double RadarPreferences::locdotSize;
double RadarPreferences::aviationSize;
QColor RadarPreferences::colorLocdot;
QColor RadarPreferences::colorCity;
QColor RadarPreferences::colorSti;
QColor RadarPreferences::colorHi;
QColor RadarPreferences::colorTvs;
QColor RadarPreferences::colorObs;
QColor RadarPreferences::colorObsWindbarbs;
QColor RadarPreferences::colorCountyLabels;
QColor RadarPreferences::nexradRadarBackgroundColor;

void RadarPreferences::initialize() {
    // locdotFollowsGps = Utility::readPref("LOCDOT_FOLLOWS_GPS", "false").startsWith("t");
    rememberLocation = WString::startsWith(Utility::readPref("WXOGL_REMEMBER_LOCATION", "true"), "t");
    swo = WString::startsWith(Utility::readPref("RADAR_SHOW_SWO", "false"), "t");
    fire = WString::startsWith(Utility::readPref("RADAR_SHOW_FIRE", "false"), "t");
    // dataRefreshInterval = Utility::readPrefInt("RADAR_DATA_REFRESH_INTERVAL", 3);
    colorLegend = WString::startsWith(Utility::readPref("RADAR_COLOR_LEGEND", "false"), "t");
    dualpaneshareposn = WString::startsWith(Utility::readPref("DUALPANE_SHARE_POSN", "true"), "t");
    obs = WString::startsWith(Utility::readPref("WXOGL_OBS", "false"), "t");
    obsWindbarbs = WString::startsWith(Utility::readPref("WXOGL_OBS_WINDBARBS", "false"), "t");
    locationDot = WString::startsWith(Utility::readPref("COD_LOCDOT_DEFAULT", "true"), "t");
    sti = WString::startsWith(Utility::readPref("RADAR_SHOW_STI", "false"), "t");
    hailIndex = WString::startsWith(Utility::readPref("RADAR_SHOW_HI", "false"), "t");
    tvs = WString::startsWith(Utility::readPref("RADAR_SHOW_TVS", "false"), "t");
    wpcFronts = WString::startsWith(Utility::readPref("RADAR_SHOW_WPC_FRONTS", "false"), "t");
    showControls = WString::startsWith(Utility::readPref("RADAR_SHOW_CONTROLS", "true"), "t");
    cities = WString::startsWith(Utility::readPref("COD_CITIES_DEFAULT", "false"), "t");
    countyLabels = WString::startsWith(Utility::readPref("RADAR_COUNTY_LABELS", "false"), "t");
    textSize = Utility::readPrefInt("RADAR_TEXT_SIZE", 8);
    TextViewMetal::fontSize = static_cast<float>(textSize);
    warnLinesize = Utility::readPrefInt("RADAR_WARN_LINESIZE", 20) / lineFactor;
    watmcdLinesize = Utility::readPrefInt("RADAR_WATMCD_LINESIZE", 20) / lineFactor;
    // gpsCircleLinesize = Utility::readPrefInt("RADAR_GPSCIRCLE_LINESIZE", 4) / lineFactor;
    stiLinesize = Utility::readPrefInt("RADAR_STI_LINESIZE", 10) / lineFactor;
    swoLinesize = Utility::readPrefInt("RADAR_SWO_LINESIZE", 20) / lineFactor;
    wbLinesize = Utility::readPrefInt("RADAR_WB_LINESIZE", 10) / lineFactor;
    locdotSize = Utility::readPrefInt("RADAR_LOCDOT_SIZE", 20) / lineFactor;
    aviationSize = Utility::readPrefInt("RADAR_AVIATION_SIZE", 20) / lineFactor;

    PolygonWarning::load();
    PolygonWatch::load();
    initializeColors();
    RadarGeometry::initStatic();
}

void RadarPreferences::initializeColors() {
    colorLocdot = getInitialPreference("RADAR_COLOR_LOCDOT", Color::rgb(255, 255, 255));
    colorCity = getInitialPreference("RADAR_COLOR_CITY", Color::rgb(255, 255, 255));
    colorSti = getInitialPreference("RADAR_COLOR_STI", Color::rgb(255, 255, 255));
    colorHi = getInitialPreference("RADAR_COLOR_HI", Color::rgb(0, 255, 0));
    colorTvs = getInitialPreference("RADAR_COLOR_TVS", Color::rgb(255, 0, 0));
    colorObs = getInitialPreference("RADAR_COLOR_OBS", Color::rgb(255, 255, 255));
    colorObsWindbarbs = getInitialPreference("RADAR_COLOR_OBS_WINDBARBS", Color::rgb(255, 255, 255));
    colorCountyLabels = getInitialPreference("RADAR_COLOR_COUNTY_LABELS", Color::rgb(234, 214, 123));
    nexradRadarBackgroundColor = getInitialPreference("NEXRAD_RADAR_BACKGROUND_COLOR", Color::rgb(0, 0, 0));
}

QColor RadarPreferences::getInitialPreference(const string& pref, int colorAsInt) {
    return Color::intToQColor(Utility::readPrefInt(pref, colorAsInt));
}
