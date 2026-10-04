// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "NexradState.h"
#include "settings/Location.h"

NexradState::NexradState(int paneNumber, int numberOfPanes, bool useASpecificRadar, const string& radarToUse, int originalWidth, int originalHeight)
    : paneNumber{paneNumber}
    , numberOfPanes{numberOfPanes}
    , useASpecificRadar{useASpecificRadar}
    , radarSite{Location::radarSite()}
    , originalWidth{originalWidth}
    , originalHeight{originalHeight}
{
    if (numberOfPanes == 2) {
        xPos = 0.0 - (originalWidth / 4.0) * zoom;
    }
    setRadar(useASpecificRadar && !radarToUse.empty() ? radarToUse : Location::radarSite());
}

ProjectionNumbers NexradState::getPn() const {
    const std::lock_guard<std::recursive_mutex> guard{*lock};
    return pn;
}

string NexradState::getRadarSite() const {
    const std::lock_guard<std::recursive_mutex> guard{*lock};
    return radarSite;
}

void NexradState::setRadar(const string& site) {
    const std::lock_guard<std::recursive_mutex> guard{*lock};
    radarSite = site;
    pn.setRadarSite(radarSite);
}

void NexradState::reset() {
    xPos = 0.0;
    yPos = 0.0;
    zoom = 0.7;
}

// void NexradState::resetZoom() {
//     zoom = 0.7;
//     xPos = 0.0;
//     if (numberOfPanes == 2) {
//         xPos = 0.0 - (originalWidth / 4.0) * zoom;
//     }
//     yPos = 0.0;
// }
