// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "DownloadTimer.h"
#include <algorithm>
#include "objects/ObjectDateTime.h"
#include "objects/WString.h"
#include "settings/RadarPreferences.h"
#include "util/Utility.h"

DownloadTimer::DownloadTimer(const string& identifier)
    : identifier{identifier}
    , lastRefresh{0}
{}

bool DownloadTimer::isRefreshNeeded() {
    radarDataRefreshInterval = Utility::readPrefInt("RADAR_DATA_REFRESH_INTERVAL", 3);
    refreshDataInMinutes = std::max(radarDataRefreshInterval, 6);
    if (WString::contains(identifier, "WARNINGS")) {
        refreshDataInMinutes = std::max(radarDataRefreshInterval, 3);
    }
    auto refreshNeeded = false;
    const auto currentTime = ObjectDateTime::currentTimeMillis();
    const auto currentTimeSeconds = static_cast<int>(currentTime / 1000);
    const auto refreshIntervalSeconds = refreshDataInMinutes * 60;
    if ((currentTimeSeconds > (lastRefresh + refreshIntervalSeconds)) || !initialized) {
        refreshNeeded = true;
        initialized = true;
        lastRefresh = currentTime / 1000;
    }
    return refreshNeeded;
}
