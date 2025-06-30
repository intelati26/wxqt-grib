// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "NexradState.h"
#include "common/GlobalDictionaries.h"
#include "objects/WString.h"
#include "radar/NexradUtil.h"
#include "settings/Location.h"
#include "settings/RadarPreferences.h"
#include "util/To.h"
#include "util/Utility.h"
#include "util/UtilityList.h"
#include "util/UtilityString.h"

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
    if (useASpecificRadar) {
        setRadar(radarToUse);
    } else {
        readPreferences();
    }
}

ProjectionNumbers NexradState::getPn() const {
    return pn;
}

string NexradState::getRadarSite() const {
    return radarSite;
}

void NexradState::setRadar(const string& site) {
    radarSite = site;
    pn.setRadarSite(radarSite);
}

string NexradState::getRadarProduct() const {
    return UtilityString::replaceRegex(radarProduct, "[0-3]", To::string(tiltInt));
}

uint16_t NexradState::getRadarProductId() const {
    return GlobalDictionaries::radarProductStringToShortInt.at(getRadarProduct());
}

void NexradState::setRadarProduct(const string& product) {
    radarProduct = WString::split(product, ":")[0];
}

bool NexradState::isTdwrSite() const {
    return NexradUtil::isRadarTdwr(getRadarSite());
}

bool NexradState::isTdwrProduct() const {
    return NexradUtil::isProductTdwr(getRadarProduct());
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

void NexradState::readPreferences() {
    if (RadarPreferences::rememberLocation) {
        const auto numberOfPanesStr = To::string(numberOfPanes);
        const auto index = To::string(paneNumber);
        zoom = To::Double(Utility::readPref(radarType + numberOfPanesStr + "_ZOOM" + index, "1.0"));
        xPos = To::Double(Utility::readPref(radarType + numberOfPanesStr + "_X" + index, "0.0"));
        yPos = To::Double(Utility::readPref(radarType + numberOfPanesStr + "_Y" + index, "0.0"));
        setRadar(Utility::readPref(radarType + numberOfPanesStr + "_RID" + index, Location::radarSite()));
        radarProduct = Utility::readPref(radarType + numberOfPanesStr + "_PROD" + index, initialRadarProducts[paneNumber]);
        tiltInt = Utility::readPrefInt(radarType + numberOfPanesStr + "_TILT" + index, 0);
    } else {
        setRadar(radarSite);
    }
}

void NexradState::writePreferences() const {
    if (!useASpecificRadar) {
        const auto numberOfPanesStr = To::string(numberOfPanes);
        const auto index = To::string(paneNumber);
        Utility::writePref(radarType + numberOfPanesStr + "_ZOOM" + index, To::string(zoom));
        Utility::writePref(radarType + numberOfPanesStr + "_X" + index, To::string(xPos));
        Utility::writePref(radarType + numberOfPanesStr + "_Y" + index, To::string(yPos));
        Utility::writePref(radarType + numberOfPanesStr + "_RID" + index, radarSite);
        Utility::writePref(radarType + numberOfPanesStr + "_PROD" + index, radarProduct);
        Utility::writePrefInt(radarType + numberOfPanesStr + "_TILT" + index, tiltInt);
    }
}
