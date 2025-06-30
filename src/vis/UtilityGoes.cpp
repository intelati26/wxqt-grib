// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "UtilityGoes.h"
#include <list>
#include "common/GlobalVariables.h"
#include "objects/WString.h"
#include "settings/UtilityLocation.h"
#include "util/To.h"
#include "util/UtilityIO.h"
#include "util/UtilityList.h"
#include "util/UtilityString.h"

string UtilityGoes::getNearest(const LatLon& latLon) {
    return UtilityLocation::getNearest(latLon, sectorToLatLon);
}

string UtilityGoes::getImageFileName(const string& sector) {
    const string fullsize{"latest"};
    string size;
    if (sizeMap.contains(sector)) {
        size = sizeMap.at(sector);
    } else {
        size = fullsize;
    }
    return size + ".jpg";
}

string UtilityGoes::getImageGoesFloater(const string& url, const string& product) {
    return WString::replace(url, "GEOCOLOR", product);
}

// https://cdn.star.nesdis.noaa.gov/GOES16/GLM/CONUS/EXTENT/20201641856GOES16-GLM-CONUS-EXTENT-2500x1500.jpg
// https://cdn.star.nesdis.noaa.gov/GOES16/GLM/CONUS/EXTENT/1250x750.jpg
string UtilityGoes::getImage(const string& product, const string& sector) {
    auto sectorLocal = "SECTOR/" + sector;
    if (sector == "FD" || sector == "CONUS" || sector == "CONUS-G17" || sector == "FD-G17")
        sectorLocal = sector;
    string satellite{"GOES19"};
    if (contains(sectorsInGoes17, sector)) {
        satellite = "GOES17";
        if (sector == "CONUS-G17") {
            sectorLocal = "CONUS";
        }
        if (sector == "FD-G17") {
            sectorLocal = "FD";
        }
    }
    auto url = GlobalVariables::goes16Url + "/" + satellite + "/ABI/" + sectorLocal + "/" + product + "/" + getImageFileName(sector);
    if (product == "GLM") {
        url = WString::replace(url, "ABI", "GLM");
        url = WString::replace(url, sector + "/GLM", sector + "/EXTENT3");
    }
    return url;
}

// vector<string> UtilityGoes::getAnimation(const string& product, const string& sector, size_t frameCount) {
//     auto baseUrl = getImage(product, sector);
//     auto items = WString::split(baseUrl, "/");
//     items.pop_back();
//     items.pop_back();
//     if (product == "GLM") {
//         baseUrl = WString::join(items, "/") + "/EXTENT3/";
//     } else {
//         baseUrl = WString::join(items, "/") + "/" + product + "/";
//     }
//     const auto html = UtilityIO::getHtml(baseUrl);
//     vector<string> urlList;
//     if (product == "GLM" || WString::startsWith(sector, "CONUS")) {
//         urlList = UtilityString::parseColumn(WString::replace(html, "\r\n", " "), "<a href=\"([^\\s]*?1250x750.jpg)\">");
//     } else if (WString::startsWith(sector, "FD")) {
//         urlList = UtilityString::parseColumn(WString::replace(html, "\r\n", " "), "<a href=\"([^\\s]*?1808x1808.jpg)\">");
//     } else {
//         urlList = UtilityString::parseColumn(WString::replace(html, "\r\n", " "), "<a href=\"([^\\s]*?1200x1200.jpg)\">");
//     }
//     std::list<string> returnList;
//     if (urlList.size() > frameCount) {
//         for ([[maybe_unused]] auto t : range(frameCount)) {
//             auto u = urlList.back();
//             urlList.pop_back();
//             returnList.push_front(baseUrl + u);
//         }
//     }
//     return {returnList.begin(), returnList.end()};
//     // <a href="20211842100_GOES16-ABI-FL-GEOCOLOR-AL052021-1000x1000.jpg">
// }

vector<string> UtilityGoes::getAnimation(const string& product, const string& sector, size_t frameCnt) {
    const auto frameCount = To::string(frameCnt);
    string url;
    string satellite = "G19";
    if (contains(sectorsInGoes17, sector)) {
        satellite = "G18";
    }
    if (sector == "FD") {
        url = "https://www.star.nesdis.noaa.gov/GOES/fulldisk_band.php?sat=G19&band=" + WString::replace(product, "GLM", "EXTENT") + "&length=" + frameCount;
    } else if (sector == "CONUS" || sector == "CONUS-G17") {
        url = "https://www.star.nesdis.noaa.gov/GOES/conus_band.php?sat=" + satellite + "&band=" + WString::replace(product, "GLM", "EXTENT") + "&length=" + frameCount;
    } else {
        url = "https://www.star.nesdis.noaa.gov/GOES/sector_band.php?sat=" + satellite + "&sector=" + sector + "&band=" + product + "&length=" + frameCount;
    }
    const auto data = UtilityIO::getHtml(url);
    auto html = WString::replace(data, "\n", "");
    html = WString::replace(html, "\r", "");
    const auto imageHtml = UtilityString::parse(html, "animationImages = \\[(.*?)\\];");
    return UtilityString::parseColumn(imageHtml, "'(https.*?jpg)'");
}

vector<string> UtilityGoes::getAnimationGoesFloater(const string& product, const string& url, size_t frameCount) {
    auto baseUrl = url;
    baseUrl = WString::replace(baseUrl, "GEOCOLOR", product);
    baseUrl = WString::replace(baseUrl, "latest.jpg", "");
    const auto html = UtilityIO::getHtml(baseUrl);
    auto urlList = UtilityString::parseColumn(html, "<a href=\"([^\\s]*?1000x1000.jpg)\">");
    std::list<string> returnList;
    if (urlList.size() > frameCount) {
        for ([[maybe_unused]] auto t : range(frameCount)) {
            string u = urlList.back();
            urlList.pop_back();
            returnList.push_front(baseUrl + u);
        }
    }
    return {returnList.begin(), returnList.end()};
}

const vector<string> UtilityGoes::sectors{
    "FD: GOES-EAST Full Disk",
    "FD-G17: GOES-WEST Full Disk",
    "CONUS: GOES-EAST US",
    "CONUS-G17: GOES-WEST US",
    "pnw: Pacific Northwest",
    "nr: Northern Rockies",
    "umv: Upper Mississippi Valley",
    "cgl: Central Great Lakes",
    "ne: Northeast",
    "psw: Pacific Southwest",
    "sr: Southern Rockies",
    "sp: Southern Plains",
    "smv: Southern Mississippi Valley",
    "se: Southeast",
    "gm: Gulf of Mexico",
    "car: Caribbean",
    "eus: U.S. Atlantic Coast",
    "pr: Puerto Rico",
    "taw: Tropical Atlantic: wide view",
    "ak: Alaska",
    "cak: Central Alaska",
    "sea: Southeastern Alaska",
    "hi: Hawaii",
    "tpw: US Pacific Coast",
    "wus: Tropical Pacific",
    "eep: Eastern Pacific",
    "np: Northern Pacific",
    "can: Canada",
    "mex: Mexico",
    "nsa: South America (north)",
    "ssa: South America (south)"
};

const vector<string> UtilityGoes::sectorsInGoes17{
    "CONUS-G17",
    "FD-G17",
    "ak",
    "cak",
    "sea",
    "hi",
    "pnw",
    "psw",
    "tpw",
    "wus",
    "np"
};

const vector<string> UtilityGoes::labels{
    "true color daytime, multispectral IR at night",
    "00.47 um (Band 1) Blue - Visible",
    "00.64 um (Band 2) Red - Visible",
    "00.86 um (Band 3) Veggie - Near IR",
    "01.37 um (Band 4) Cirrus - Near IR",
    "01.6 um (Band 5) Snow/Ice - Near IR",
    "02.2 um (Band 6) Cloud Particle - Near IR",
    "03.9 um (Band 7) Shortwave Window - IR",
    "06.2 um (Band 8) Upper-Level Water Vapor - IR",
    "06.9 um (Band 9) Mid-Level Water Vapor - IR",
    "07.3 um (Band 10) Lower-level Water Vapor - IR",
    "08.4 um (Band 11) Cloud Top - IR",
    "09.6 um (Band 12) Ozone - IR",
    "10.3 um (Band 13) Clean Longwave Window - IR",
    "11.2 um (Band 14) Longwave Window - IR",
    "12.3 um (Band 15) Dirty Longwave Window - IR",
    "13.3 um (Band 16) CO2 Longwave - IR",
    "AirMass - RGB composite based on the data from IR  and  and  WV",
    "Sandwich RGB - Bands 3  and  and  13 combo",
    "Day Cloud Phase",
    "Night Microphysics",
    "Fire Temperature",
    "Dust RGB",
    "GLM FED+GeoColor",
    "DMW"
};

const vector<string> UtilityGoes::productCodes{
    "GEOCOLOR",
    "01",
    "02",
    "03",
    "04",
    "05",
    "06",
    "07",
    "08",
    "09",
    "10",
    "11",
    "12",
    "13",
    "14",
    "15",
    "16",
    "AirMass",
    "Sandwich",
    "DayCloudPhase",
    "NightMicrophysics",
    "FireTemperature",
    "Dust",
    "GLM",
    "DMW"
};

const unordered_map<string, LatLon> UtilityGoes::sectorToLatLon{
    {"cgl", {39.123405, -82.532938}},   // wellston, Oh;
    {"ne",  {39.360611, -74.431877}},   // Atlantic City, NJ;
    {"umv", {40.622777, -93.934116}},   // Lamoni, IA;
    {"pnw", {41.589703, -119.858865}},  // Vya, NV;
    {"psw", {38.524448, -118.623611}},  // Hawthorne, NV;
    {"nr",  {41.139980, -104.820244}},  // Cheyenne, Wy;
    {"sr",  {34.653376, -108.677852}},  // Fence Lake, NM;
    {"sp",  {31.463787, -96.058022}},   // Buffalo, TX;
    {"smv", {31.326460, -89.289658}},   // Hattiesburg, MS;
    {"se",  {30.332184, -81.655647}},   // Jacksonville, FL;
    {"hi",  {21.315603, -157.858093}},  // Honolulu, HI
    {"cak", {61.217381, -149.863129}},  // Anchorage, AK;
};

const unordered_map<string, string> UtilityGoes::sizeMap{
    {"CONUS", "1250x750"},
    {"CONUS-G17", "1250x750"},
    {"FD", "1808x1808"},
    {"FD-G17", "1808x1808"},
    {"gm", "1000x1000"},
    {"car", "1000x1000"},
    {"eus", "1000x1000"},
    {"eep", "1800x1080"},
    {"wus", "2000x2000"},
    {"tpw", "1800x1080"},
    {"taw", "1800x1080"},
    {"can", "1125x560"},
    {"mex", "1000x1000"},
    {"nsa", "1800x1080"},
    {"ssa", "1800x1080"},
    {"np", "1800x1080"},
    {"cam", "1000x1000"},
    {"np", "1800x1080"},
    {"ak", "1000x1000"},
    {"cak", "1200x1200"},
    {"sea", "1200x1200"},
    {"hi", "1200x1200"},
    {"pnw", "1200x1200"},
    // {"nr", "1200x1200"},
    // {"umv", "1200x1200"},
    // {"cgl", "1200x1200"},
    // {"ne", "1200x1200"},
    // {"psw", "1200x1200"},
    // {"sr", "1200x1200"},
    // {"sp", "1200x1200"},
    // {"smv", "1200x1200"},
    // {"se", "1200x1200"},
};
