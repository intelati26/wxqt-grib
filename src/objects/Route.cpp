// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "Route.h"
#include "misc/AdhocLocation.h"
#include "misc/WfoText.h"
#include "radar/Nexrad.h"
#include "radar/RadarMosaic.h"
#include "settings/UIPreferences.h"
#include "spc/SpcMeso.h"
#include "spc/SpcSoundings.h"
#include "vis/GoesViewer.h"

using std::string;

void Route::adhocLocation(Window * parent, const LatLon& latLon) {
    new AdhocLocation{parent, latLon};
}

void Route::lightning(Window * parent) {
    new GoesViewer{parent, "", "GLM", "CONUS", false};
}

void Route::nexradRadar(Window * parent, int paneCount) {
    new Nexrad{parent, paneCount, false, ""};
}

void Route::nexradRadarSpecificSite(Window * parent, const string& radarSite) {
    new Nexrad{parent, 1, true, radarSite};
}

void Route::radarMosaicBySector(Window * parent, const string& sector) {
    new RadarMosaic{parent, sector};
}

void Route::radarMosaic(Window * parent) {
    new RadarMosaic{parent};
}

void Route::spcSoundingBySector(Window * parent, const string& sector) {
    new SpcSoundings{parent, sector};
}

void Route::spcMesoBySector(Window * parent, const string& sector) {
    new SpcMeso{parent, "", sector, false};
}

void Route::vis(Window * parent) {
    new GoesViewer{parent, ""};
}

void Route::visBySector(Window * parent, const string& sector) {
    new GoesViewer{parent, "", "GEOCOLOR", sector, false};
}

void Route::wfoTextBySector(Window * parent, const string& sector) {
    new WfoText{parent, sector};
}
