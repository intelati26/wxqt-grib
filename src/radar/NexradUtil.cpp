// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "NexradUtil.h"
#include "objects/ObjectDateTime.h"
#include "objects/WString.h"
#include "radar/RadarSites.h"
#include "util/UtilityList.h"
#include "util/UtilityString.h"

double NexradUtil::wxoglDspLegendMax{1.0};

const vector<string> NexradUtil::radarProductList{
    "N0Q: Base Reflectivity",
    "N0U: Base Velocity",
//    "N0B: Base Reflectivity super-res",
//    "N0G: Base Velocity super-res",
    "EET: Enhanced Echo Tops",
    "DVL: Vertically Integrated Liquid",
    "N0C: Correlation Coefficient",
    "N0X: Differential Reflectivity",
    "N0K: Specific Differential Phase",
    "H0C: Hydrometer Classification",
    "DSP: Digital Storm Total Precipitation",
    "DAA: Digital Accumulation Array",
    "N0S: Storm Relative Mean Velocity",
    "NSW: Base Spectrum Width",
    "NCR: Composite Reflectivity 124nm",
    "NCZ: Composite Reflectivity 248nm"
};

const vector<string> NexradUtil::radarProductListTdwr{
    "TZL: Long Range Digital Base Reflectivity",
    "TZ0: Digital Base Reflectivity",
    "TV0: Digital Base Velocity"
};

int NexradUtil::findRadarProductIndex(const string& product) {
    for (auto index : range(radarProductList.size())) {
        if (WString::startsWith(radarProductList[index], product + ":")) {
            return index;
        }
    }
    return 0;
}

int NexradUtil::findRadarProductIndexTdwr(const string& product) {
    for (auto index : range(radarProductListTdwr.size())) {
        if (WString::startsWith(radarProductListTdwr[index], product + ":")) {
            return index;
        }
    }
    return 0;
}

bool NexradUtil::isProductTdwr(const string& product) {
    return WString::startsWith(product, "TV") || product == "TZL" || WString::startsWith(product, "TZ");
}

bool NexradUtil::isRadarTdwr(const string& radarSite) {
    const auto siteUpper = WString::toUpper(radarSite);
    return contains(RadarSites::tdwrRadarCodes(), siteUpper);
}

double NexradUtil::getBinSize(uint16_t productCode) {
    const auto binSize54 = 2.0;
    const auto binSize13 = 0.50;
    const auto binSize08 = 0.295011;
    const auto binSize16 = 0.590022;
    const auto binSize110 = 2.0 * binSize54;
    switch (productCode) {
        case 134:
        case 135:
            return binSize54;
        case 186:
            return binSize16;
        case 159:
        case 161:
        case 163:
        case 165:
        case 99:
        case 170:
        case 172:
            return binSize13;
        case 180:
        case 181:
        case 182:
            return binSize08;
        case 153:
        case 154:
        case 155:
        case 2161:
            return binSize13;
        case 78:
        case 80:
            return binSize110;
        default:
            return binSize54;
    }
}

int NexradUtil::getNumberRangeBins(uint16_t productCode) {
    switch (productCode) {
        case 78:
        case 80:
            return 592;
        case 134:
            return 460;
        case 153:
        case 154:
            return 720;
        case 186:
            return 1390;
        case 180:
        case 181:
        case 182:
            return 720;
        case 135:
        case 99:
        case 159:
        case 161:
        case 163:
        case 170:
        case 172:
            return 1200;
        default:
            return 460;
    }
}

bool NexradUtil::isRadarTimeOld(int radarMilli) {
    // 1 min is 60k ms
    if (radarMilli > 20 * 60000) {
        return true;
    }
    return false;
}

bool NexradUtil::isVtecCurrent(const string& vtec) {
    // example "190512T1252Z-190512T1545Z"
    const auto vtecTimeRange = UtilityString::parse(vtec, "-([0-9]{6}T[0-9]{4})Z");
    const auto vtecTime = ObjectDateTime::decodeVtecTime(vtecTimeRange);
    const auto currentTime = ObjectDateTime::decodeVtecTime(ObjectDateTime::getGmtTimeForVtec());
    return currentTime.isBefore(vtecTime);
}
