// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "MyApplication.h"
#include <QDir>
#include "common/GlobalVariables.h"
#include "radar/Metar.h"
#include "radar/RadarSites.h"
#include "settings/Location.h"
#include "settings/RadarPreferences.h"
#include "settings/UIPreferences.h"
#include "settings/UtilityStorePreferences.h"
#include "util/SoundingSites.h"
#include "util/WfoSites.h"

QSettings * MyApplication::preferences;

void MyApplication::onCreate() {
    preferences = new QSettings{QString::fromStdString(GlobalVariables::appOrgName), QString::fromStdString(GlobalVariables::appName)};
    UtilityStorePreferences::setDefaults();
    SoundingSites::initialize();
    WfoSites::initialize();
    RadarSites::initialize();
    Metar::initialize();
    Location::refresh();
    RadarPreferences::initialize();
    UIPreferences::initialize();
}
