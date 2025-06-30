// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "NexradLevel3TextProduct.h"
#include "util/UtilityIO.h"
#include "NexradDownload.h"

string NexradLevel3TextProduct::download(const string& product, const string& radarSite) {
    // ISO-8859-1
    const auto url = NexradDownload::getRadarFileUrl(radarSite, product);
    const auto byteArray = UtilityIO::downloadAsByteArray(url);
    return string(byteArray.constData(), byteArray.length());
}
