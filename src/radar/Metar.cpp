// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "Metar.h"
#include "common/GlobalVariables.h"
#include "objects/Color.h"
#include "objects/WString.h"
#include "radar/RadarSites.h"
#include "util/To.h"
#include "util/UtilityIO.h"
#include "util/UtilityList.h"
#include "util/UtilityLog.h"
#include "util/UtilityMath.h"
#include "util/UtilityString.h"

unique_ptr<Sites> Metar::sites;

void Metar::initialize() {
    unordered_map<string, string> name;
    unordered_map<string, string> lat;
    unordered_map<string, string> lon;
    auto lines = UtilityIO::rawFileToStringArray(GlobalVariables::resDir + "obs_all.txt");
    for (auto line : lines) {
        auto items = WString::split(WString::strip(line), ",");
        if (items.size() > 2) {
            name[items[0]] = items[1] + ", " + items[2];
            lat[items[0]] = items[3];
            lon[items[0]] = items[4];
        }
    }
    sites = make_unique<Sites>(name, lat, lon, false);
}

void Metar::getStateMetarArrayForWXOGL(const string& radarSite, FileStorage& fileStorage) {
    if (fileStorage.obsDownloadTimer.isRefreshNeeded() || radarSite != fileStorage.obsOldRadarSite) {
        vector<string> obsAl;
        vector<string> obsAlExt;
        vector<string> obsAlWb;
        vector<string> obsAlWbGust;
        vector<double> obsAlX;
        vector<double> obsAlY;
        vector<int> obsAlAviationColor;
        fileStorage.obsOldRadarSite = radarSite;
        const auto obsList = getNearbyObsSites(radarSite);
        const auto url = "https://aviationweather.gov/cgi-bin/data/metar.php?ids=" + obsList;
        const auto html = UtilityIO::getHtml(url);
        const auto metarsTmp = WString::split(html, GlobalVariables::newline);
        const auto metars = condense(metarsTmp);
        // initObsMap();
        for (const auto& metar : metars) {
            if ((WString::startsWith(metar, "K") || WString::startsWith(metar, "P") || WString::startsWith(metar, "T")) && !WString::contains(metar, "NIL")) {
                const auto metarItems = WString::split(metar, " ");
                const auto tmpBlob = UtilityString::parse(metar, ".*? (M?../M?..) .*?");
                const auto tempAndDewpointList = WString::split(tmpBlob, "/");
                const auto timeBlob = (metarItems.size() > 1) ? metarItems[1] : "";
                auto pressureBlob = UtilityString::parse(metar, ".*? A([0-9]{4})");
                auto windBlob = UtilityString::parse(metar, "AUTO ([0-9].*?KT) .*?");
                if (windBlob.empty()) {
                    windBlob = UtilityString::parse(metar, "Z ([0-9].*?KT) .*?");
                }
                const auto conditionsBlob = UtilityString::parse(metar, "SM (.*?) M?[0-9]{2}/");
                auto visBlob = UtilityString::parse(metar, " ([0-9].*?SM) ");
                const auto visBlobArr = WString::split(visBlob, " ");
                string visBlobDisplay;
                if (!visBlobArr.empty()) {
                    visBlobDisplay = visBlobArr[visBlobArr.size() - 1];
                    visBlob = visBlobArr[visBlobArr.size() - 1];
                    visBlob = WString::replace(visBlob, "SM", "");
                }
                auto visInt = 20000;
                if (WString::contains(visBlob, "/")) {
                    visInt = 0;
                } else if (!visBlob.empty()) {
                    visInt = To::Int(visBlob);
                }
                auto ovcStr = UtilityString::parse(conditionsBlob, "OVC([0-9]{3})");
                auto bknStr = UtilityString::parse(conditionsBlob, "BKN([0-9]{3})");
                auto ovcInt = 100000;
                auto bknInt = 100000;
                if (!ovcStr.empty()) {
                    ovcStr += "00";
                    ovcInt = To::Int(ovcStr);
                }
                if (!bknStr.empty()) {
                    bknStr += "00";
                    bknInt = To::Int(bknStr);
                }
                auto lowestCig = bknInt < ovcInt ? bknInt : ovcInt;
                auto aviationColor = Color::greenInt;
                if (visInt > 5 && lowestCig > 3000) {
                    aviationColor = Color::greenInt;
                }
                if ((visInt >= 3 && visInt <= 5) || (lowestCig >= 1000 && lowestCig <= 3000)) {
                    aviationColor = Color::rgb(0, 100, 255);
                }
                if ((visInt >= 1 && visInt < 3) || (lowestCig >= 500 && lowestCig < 1000)) {
                    aviationColor = Color::redInt;
                }
                if (visInt < 1 || lowestCig < 500) {
                    aviationColor = Color::magentaInt;
                }
                if (pressureBlob.size() == 4) {
                    pressureBlob = UtilityString::insert(pressureBlob, pressureBlob.size() - 2, ".");
                    pressureBlob = UtilityMath::unitsPressure(pressureBlob);
                }
                auto validWind = false;
                auto validWindGust = false;
                string windDir;
                string windInKt;
                string windGustInKt;
                if (WString::contains(windBlob, "KT") && windBlob.size() == 7) {
                    validWind = true;
                    windDir = UtilityString::substring(windBlob, 0, 3);
                    windInKt = UtilityString::substring(windBlob, 3, 5);
                    const auto windDirInt = To::Int(windDir);
                    windBlob = windDir + " (" + UtilityMath::bearingToDirection(windDirInt) + ") " + windInKt + " kt";
                } else if (WString::contains(windBlob, "KT") && windBlob.size() == 10) {
                    validWind = true;
                    validWindGust = true;
                    windDir = UtilityString::substring(windBlob, 0, 3);
                    windInKt = UtilityString::substring(windBlob, 3, 5);
                    windGustInKt = UtilityString::substring(windBlob, 6, 8);
                    const auto windDirInt = To::Int(windDir);
                    windBlob = windDir + " (" + UtilityMath::bearingToDirection(windDirInt) + ") " + windInKt + " G " + windGustInKt + " kt";
                }
                if (tempAndDewpointList.size() > 1) {
                    auto temperature = tempAndDewpointList[0];
                    auto dewPoint = tempAndDewpointList[1];
                    temperature = WString::replace(temperature, "M", "-");
                    temperature = UtilityMath::celsiusToFahrenheit(temperature);
                    temperature = WString::split(temperature, ".")[0];
                    dewPoint = WString::replace(dewPoint, "M", "-");
                    dewPoint = UtilityMath::celsiusToFahrenheit(dewPoint);
                    dewPoint = WString::split(dewPoint, ".")[0];
                    const auto& obsSite = metarItems[0];
                    // Metar.sites.byCode[obsSite].latLon.forNexrad()
                    LatLon latlon;
                    if (sites->byCode.contains(obsSite)) {
                        latlon = sites->byCode[obsSite]->latLon;  //.forNexrad();
                    } else {
                        UtilityLog::d("Metar.obsLatlon not found: " + obsSite);
                    }
                    latlon.setLon(To::Double(WString::replace(latlon.lonStr(), "-0", "-")));
                    obsAl.push_back(latlon.latStr() + ":" + latlon.lonStr() + ":" + temperature + "/" + dewPoint);
                    obsAlExt.push_back(latlon.latStr() + ":" + latlon.lonStr() + ":" + temperature + "/" + dewPoint + " (" + obsSite + ")" + GlobalVariables::newline + pressureBlob + " - " + visBlobDisplay + GlobalVariables::newline + windBlob + GlobalVariables::newline + conditionsBlob + GlobalVariables::newline + timeBlob);
                    if (validWind) {
                        obsAlWb.push_back(latlon.latStr() + ":" + latlon.lonStr() + ":" + windDir + ":" + windInKt);
                        obsAlX.push_back(latlon.lat());
                        obsAlY.push_back(latlon.lon() * -1.0);
                        obsAlAviationColor.push_back(aviationColor);
                    }
                    if (validWindGust) {
                        obsAlWbGust.push_back(latlon.latStr() + ":" + latlon.lonStr() + ":" + windDir + ":" + windGustInKt);
                    }
                }
            }
        }
        fileStorage.obsArr = obsAl;
        fileStorage.obsArrExt = obsAlExt;
        fileStorage.obsArrWb = obsAlWb;
        fileStorage.obsArrWbGust = obsAlWbGust;
        fileStorage.obsArrX = obsAlX;
        fileStorage.obsArrY = obsAlY;
        fileStorage.obsArrAviationColor = obsAlAviationColor;
    }
}

// void Metar::initObsMap() {
//     mtx.lock();
//     if (obsLatlon.empty()) {
//         const auto lines = UtilityIO::rawFileToStringArray(GlobalVariables::resDir + metarFileName);
//         for (const auto& line : lines) {
//             const auto items = WString::split(line, " ");
//             if (items.size() > 2) {
//                 obsLatlon[items[0]] = LatLon{items[1], items[2]};
//             }
//         }
//     }
//     mtx.unlock();
// }

string Metar::getNearbyObsSites(const string& radarSite) {
    string obsListSb;
    const auto radarLocation = RadarSites::getLatLon(radarSite);
    // loadMetarData();
    for (const auto& site : sites->sites) {
        if (LatLon::distance(radarLocation, site.latLon) < 200.0) {
            obsListSb += site.codeName + ",";
        }
    }
    return WString::replace(obsListSb, ",$", "");
}

// void Metar::loadMetarData() {
//     if (metarDataRaw.empty()) {
//         metarDataRaw = UtilityIO::rawFileToStringArray(GlobalVariables::resDir + metarFileName);
//         metarSites.clear();
//         for (const auto& metar : metarDataRaw) {
//             const auto items = WString::split(metar, " ");
//             if (items.size() > 2) {
//                 metarSites.push_back(Site::fromLatLon(items[0], "", LatLon{items[1], items[2]}));
//             }
//         }
//     }
// }

// used to condense a list of metar that contains multiple entries for one site,
// newest is first so simply grab first/append
vector<string> Metar::condense(const vector<string>& observations) {
    unordered_map<string, bool> siteMap;
    vector<string> goodObsList;
    for (const auto& ob : observations) {
        const auto items = WString::split(ob, " ");
        if (items.size() > 3) {
            if (!siteMap.contains(items[0])) {
                siteMap[items[0]] = true;
                goodObsList.push_back(ob);
            }
        }
    }
    return goodObsList;
}

Site Metar::findClosestObservation(const LatLon& latLon, int order) {
    return sites->getNearestSite(latLon, order);

    // loadMetarData();
    // vector<Site> obsSites{metarSites};
    // for (auto i : range(obsSites.size())) {
    //     obsSites[i].distance = LatLon::distance(latLon, obsSites[i].latLon);
    // }
    // std::sort(
    //     obsSites.begin(),
    //     obsSites.end(),
    //     [] (const auto& s1, const auto& s2) { return s1.distance < s2.distance; });
    // return obsSites[index];
}
