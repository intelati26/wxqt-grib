// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "Site.h"

Site::Site(const string& codeName, const string& fullName, const string& lat, const string& lon1, bool lonReversed)
    : codeName{codeName}
    , fullName{fullName}
{
    auto lon = lon1;
    if (lonReversed) {
        lon = "-" + lon;
    }
    latLon = LatLon(lat, lon);
}

Site Site::fromLatLon(const string& codeName, const string& fullName, LatLon latLon, double distance) {
    auto site = Site{codeName, fullName, latLon.latStr(), latLon.lonStr(), false};
    site.distance = static_cast<int>(distance);
    return site;
}
