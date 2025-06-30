// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "ProjectionNumbers.h"
#include "radar/RadarSites.h"
#include "settings/Location.h"
#include "util/UtilityMath.h"

ProjectionNumbers::ProjectionNumbers() {
    setRadarSite(Location::radarSite());
}

void ProjectionNumbers::setRadarSite(const string& radarSite) {
    this->radarSite = radarSite;
    scale = 190.0;
    xCenter = 0.0;
    yCenter = 0.0;
    latLon = RadarSites::getLatLon(radarSite).reverseLon();
    oneDegreeScaleFactor = UtilityMath::pixPerDegreeLon(latLon.lat(), scale);
}

double ProjectionNumbers::getOneDegreeScaleFactor() const {
    return oneDegreeScaleFactor;
}

string ProjectionNumbers::getRadarSite() const {
    return radarSite;
}

LatLon ProjectionNumbers::getLatLon() const {
    return latLon;
}
