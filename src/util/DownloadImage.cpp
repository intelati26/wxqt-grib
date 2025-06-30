// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "DownloadImage.h"
#include "common/GlobalVariables.h"
#include "misc/UtilityRtma.h"
#include "radar/UtilityRadarMosaic.h"
#include "settings/Location.h"
#include "spc/UtilitySpcMesoInputOutput.h"
#include "vis/UtilityGoes.h"

string DownloadImage::byProduct(const string& product) {
    string url;
    if (product == "STRPT") {
        url = GlobalVariables::nwsSPCwebsitePrefix + "/climo/reports/today.gif";
    } else if (product == "USWARN") {
        url = "https://forecast.weather.gov/wwamap/png/US.png";
    } else if (product == "VISIBLE_SATELLITE") {
        const auto sector = UtilityGoes::getNearest(Location::getLatLonCurrent());
        url = UtilityGoes::getImage("GEOCOLOR", sector);
    } else if (product == "ANALYSIS_RADAR_AND_WARNINGS") {
        url = GlobalVariables::nwsWPCwebsitePrefix + "/images/wwd/radnat/NATRAD_24.gif";
    } else if (product == "RADAR_MOSAIC") {
        const auto radarMosaicSector = UtilityRadarMosaic::getNearest(Location::getLatLonCurrent());
        url = UtilityRadarMosaic::get(radarMosaicSector);
    } else if (product == "RTMA_TEMP") {
        url = UtilityRtma::getUrlForHomeScreen("2m_temp");
    } else if (product == "SPC_MESO_MSLP") {
        url = UtilitySpcMesoInputOutput::getImageUrl("pmsl", "19");
    } else if (product == "SPC_MESO_500MB") {
        url = UtilitySpcMesoInputOutput::getImageUrl("500mb", "19");
    }
    return url;
}
