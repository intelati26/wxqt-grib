// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "UtilityRadarMosaic.h"
#include "settings/UtilityLocation.h"
#include "util/To.h"

string UtilityRadarMosaic::getNearest(const LatLon& latLon) {
    return UtilityLocation::getNearest(latLon, cityToLatLon);
}

string UtilityRadarMosaic::get(const string& sector) {
    if (sector == "CONUS") {
        return baseUrl + "CONUS-LARGE_0.gif";
    }
    return baseUrl + sector + "_0.gif";
}

vector<string> UtilityRadarMosaic::getAnimation([[maybe_unused]] const string& product, const string& sector, [[maybe_unused]] int unused) {
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

const string UtilityRadarMosaic::baseUrl{"https://radar.weather.gov/ridge/standard/"};

const vector<string> UtilityRadarMosaic::sectors{
    "CONUS",
    "ALASKA",
    "CARIB",
    "CENTGRLAKES",
    "GUAM",
    "HAWAII",
    "NORTHEAST",
    "NORTHROCKIES",
    "PACNORTHWEST",
    "PACSOUTHWEST",
    "SOUTHEAST",
    "SOUTHMISSVLY",
    "SOUTHPLAINS",
    "SOUTHROCKIES",
    "UPPERMISSVLY",
};

const unordered_map<string, LatLon> UtilityRadarMosaic::cityToLatLon{
    {"ALASKA",       {63.8683, -149.3669}},
    {"CARIB",        {18.356, -69.592}},
    {"CENTGRLAKES",  {42.4396, -84.7305}},
    {"GUAM",         {13.4208, 144.7540}},
    {"HAWAII",       {19.5910, -155.4343}},
    {"NORTHEAST",    {42.7544, -73.4800}},
    {"NORTHROCKIES", {44.0813, -108.1309}},
    {"PACNORTHWEST", {43.1995, -118.9174}},
    {"PACSOUTHWEST", {35.8313, -119.2245}},
    {"SOUTHEAST",    {30.2196, -82.1522}},
    {"SOUTHMISSVLY", {32.8184, -90.0434}},
    {"SOUTHPLAINS",  {32.4484, -99.7781}},
    {"SOUTHROCKIES", {33.2210, -110.3162}},
    {"UPPERMISSVLY", {42.9304, -95.7488}}
};
