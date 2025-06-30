// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "Hazards.h"
#include "util/UtilityIO.h"

void Hazards::process(const LatLon& latLon) {
    data = UtilityIO::getHtml("https://api.weather.gov/alerts?point=" + latLon.latForNws() + "," + latLon.lonForNws() + "&active=1");
}
