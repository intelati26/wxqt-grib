// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "misc/UtilityRtma.h"
#include <algorithm>
#include "objects/WString.h"
#include "settings/Location.h"
#include "settings/UtilityLocation.h"
#include "util/To.h"
#include "util/Utility.h"
#include "util/UtilityIO.h"
#include "util/UtilityString.h"

// using std::unique;
// using std::distance;

string UtilityRtma::getNearest(const LatLon& latLon) {
    return UtilityLocation::getNearest(latLon, sectorToLatLon);
}

string UtilityRtma::get(const string& sector) {
    if (sector == "CONUS") {
        return baseUrl + "CONUS-LARGE_0.gif";
    }
    return baseUrl + sector + "_0.gif";
}

vector<string> UtilityRtma::getAnimation([[maybe_unused]] const string& product, const string& sector, [[maybe_unused]] int unused) {
    vector<string> returnList;
    string add;
    if (sector == "CONUS") {
        add = "-LARGE";
    }
    for (auto i = 9; i >= 0; i -= 1) {
        returnList.push_back(baseUrl + sector + add + "_" + To::string(i) + ".gif");
    }
    return returnList;
}

const string UtilityRtma::baseUrl{"https://radar.weather.gov/ridge/standard/"};

vector<string> UtilityRtma::getTimes() {
    const auto html = UtilityIO::getHtml("https://mag.ncep.noaa.gov/observation-parameter.php?group=Observations%20and%20Analyses&obstype=RTMA&area=MI&ps=area");
    // title="20221116 00 UTC"
    auto v = UtilityString::parseColumn(html, "([0-9]{8} [0-9]{2} UTC)");
    std::sort(v.begin(), v.end());
    std::reverse(v.begin(), v.end());
    auto last = std::unique(v.begin(), v.end());
    v.erase(last, v.end());
    return v;
}

string UtilityRtma::getUrl(int index, int indexSector, string runTime) {
    const auto currentRun = WString::split(runTime, " ")[1];
    return "https://mag.ncep.noaa.gov/data/rtma/" + currentRun + "/rtma_" + sectors[indexSector] + "_000_" + codes[index] + ".gif";
}

string UtilityRtma::getUrlForHomeScreen(string product) {
    const auto sector = getNearest(Location::getLatLonCurrent());
    const auto runTimes = getTimes();
    if (runTimes.empty()) {
        return "";
    } else {
        const auto runTime = runTimes.front();
        const auto tokens = WString::split(runTime, " ");
        const auto currentRun = Utility::safeGet(tokens, 1);
        return "https://mag.ncep.noaa.gov/data/rtma/" + currentRun + "/rtma_" + sector + "_000_" + product + ".gif";
    }
}

const vector<string> UtilityRtma::codes{
    "2m_temp",
    "10m_wnd",
    "2m_dwpt",
    "ceiling",
    "vis"
};

const vector<string> UtilityRtma::labels{
    "2-Meter Temperature (F)",
    "10-Meter Wind Speed (Knots) / Direction",
    "2-Meter Dew Point (F)",
    "Cloud Ceiling (AGL FT X 100)",
    "Visibility (Miles)"
};


const vector<string> UtilityRtma::sectors{
    "alaska",
    "ca",
    "co",
    "fl",
    "guam",
    "gulf-coast",
    "mi",
    "mid-atl",
    "mid-west",
    "mt",
    "nc_sc",
    "nd_sd",
    "new-eng",
    "nw-pacific",
    "ohio-valley",
    "sw_us",
    "tx",
    "wi"
};

const unordered_map<string, LatLon> UtilityRtma::sectorToLatLon{
    {"alaska",      {63.25, -156.5}},
    {"ca",          {38.0, -118.5}},
    {"co",          {39.0, -105.25}},
    {"fl",          {27.5, -83.25}},
    {"guam",        {13.5, 144.75}},
    {"gulf-coast",  {32.75, -90.25}},
    {"mi",          {43.75, -84.75}},
    {"mid-atl",     {39.75, -75.75}},
    {"mid-west",    {39.5, -93.0}},
    {"mt",          {45.0, -109.25}},
    {"nc_sc",       {34.5, -79.75}},
    {"nd_sd",       {45.5, -98.25}},
    {"new-eng",     {43.0, -71.25}},
    {"nw-pacific",  {45.5, -122.75}},
    {"ohio-valley", {39.0, -84.75}},
    {"sw_us",       {34.5, -104.25}},
    {"tx",          {32.0, -100.25}},
    {"wi",          {44.25, -89.75}},
};
