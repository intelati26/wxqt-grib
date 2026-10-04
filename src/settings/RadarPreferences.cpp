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

bool RadarPreferences::swo;
bool RadarPreferences::fire;
bool RadarPreferences::locationDot;
bool RadarPreferences::cities;
bool RadarPreferences::countyLabels;
bool RadarPreferences::obs;
bool RadarPreferences::obsWindbarbs;
bool RadarPreferences::wpcFronts;
int RadarPreferences::textSize;
double RadarPreferences::warnLinesize;
double RadarPreferences::watmcdLinesize;
// double RadarPreferences::gpsCircleLinesize = 0.0;
double RadarPreferences::swoLinesize;
double RadarPreferences::wbLinesize;
// was 10.0 for vala/gtk port
double RadarPreferences::lineFactor{20.0};
double RadarPreferences::locdotSize;
double RadarPreferences::aviationSize;
QColor RadarPreferences::colorLocdot;
QColor RadarPreferences::colorCity;
QColor RadarPreferences::colorObs;
QColor RadarPreferences::colorObsWindbarbs;
QColor RadarPreferences::colorCountyLabels;
QColor RadarPreferences::nexradRadarBackgroundColor;

void RadarPreferences::initialize() {
    // locdotFollowsGps = Utility::readPref("LOCDOT_FOLLOWS_GPS", "false").startsWith("t");
    swo = WString::startsWith(Utility::readPref("RADAR_SHOW_SWO", "false"), "t");
    fire = WString::startsWith(Utility::readPref("RADAR_SHOW_FIRE", "false"), "t");
    // dataRefreshInterval = Utility::readPrefInt("RADAR_DATA_REFRESH_INTERVAL", 3);
    obs = WString::startsWith(Utility::readPref("WXOGL_OBS", "false"), "t");
    obsWindbarbs = WString::startsWith(Utility::readPref("WXOGL_OBS_WINDBARBS", "false"), "t");
    locationDot = WString::startsWith(Utility::readPref("COD_LOCDOT_DEFAULT", "true"), "t");
    wpcFronts = WString::startsWith(Utility::readPref("RADAR_SHOW_WPC_FRONTS", "false"), "t");
    cities = WString::startsWith(Utility::readPref("COD_CITIES_DEFAULT", "false"), "t");
    countyLabels = WString::startsWith(Utility::readPref("RADAR_COUNTY_LABELS", "false"), "t");
    textSize = Utility::readPrefInt("RADAR_TEXT_SIZE", 8);
    TextViewMetal::fontSize = static_cast<float>(textSize);
    warnLinesize = Utility::readPrefInt("RADAR_WARN_LINESIZE", 20) / lineFactor;
    watmcdLinesize = Utility::readPrefInt("RADAR_WATMCD_LINESIZE", 20) / lineFactor;
    // gpsCircleLinesize = Utility::readPrefInt("RADAR_GPSCIRCLE_LINESIZE", 4) / lineFactor;
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
    colorObs = getInitialPreference("RADAR_COLOR_OBS", Color::rgb(255, 255, 255));
    colorObsWindbarbs = getInitialPreference("RADAR_COLOR_OBS_WINDBARBS", Color::rgb(255, 255, 255));
    colorCountyLabels = getInitialPreference("RADAR_COLOR_COUNTY_LABELS", Color::rgb(234, 214, 123));
    nexradRadarBackgroundColor = getInitialPreference("NEXRAD_RADAR_BACKGROUND_COLOR", Color::rgb(0, 0, 0));
}

QColor RadarPreferences::getInitialPreference(const string& pref, int colorAsInt) {
    return Color::intToQColor(Utility::readPrefInt(pref, colorAsInt));
}
