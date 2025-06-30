// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "Location.h"
#include <algorithm>
#include "objects/WString.h"
#include "radar/Metar.h"
#include "radar/RadarSites.h"
#include "settings/ObjectLocation.h"
#include "util/To.h"
#include "util/Utility.h"
#include "util/UtilityIO.h"
#include "util/UtilityList.h"
#include "util/UtilityString.h"
#include "util/WfoSites.h"

ComboBox * Location::comboBox;
int Location::numberOfLocations{1};
size_t Location::currentLocationIndex{0};
vector<ObjectLocation> Location::locations;

int Location::getNumLocations() {
    return numberOfLocations;
}

void Location::setNumLocations(int newValue) {
    numberOfLocations = newValue;
    Utility::writePrefInt("LOC_NUM_INT", newValue);
}

string Location::radarSite() {
    return locations[currentLocationIndex].getRadarSite();
}

string Location::wfo() {
    return locations[currentLocationIndex].getWfo();
}

string Location::getRadarSite(int locationNumber) {
    return locations[locationNumber].getRadarSite();
}

string Location::getWfo(int locationNumber) {
    return locations[locationNumber].getWfo();
}

LatLon Location::getLatLon(int locationNumber) {
    return locations[locationNumber].getLatLon();
}

string Location::name() {
    return locations[currentLocationIndex].getName();
}

string Location::getName(size_t locationNumber) {
    return locations[locationNumber].getName();
}

void Location::setName(size_t locationNumber, const string& newName) {
    const auto iStr = To::string(locationNumber + 1);
    Utility::writePref("LOC" + iStr + "LABEL", newName);
}

string Location::state() {
    return locations[currentLocationIndex].getState();
}

LatLon Location::getLatLonCurrent() {
    return locations[currentLocationIndex].getLatLon();
}

void Location::refresh() {
    initNumLocations();
    locations.clear();
    for (auto index : range(getNumLocations())) {
        locations.emplace_back(index);
    }
    setCurrentLocationStr(Utility::readPref("CURRENT_LOC_FRAGMENT", "1"));
    checkCurrentLocationValidity();
}

void Location::initNumLocations() {
    setNumLocations(Utility::readPrefInt("LOC_NUM_INT", 1));
}

void Location::checkCurrentLocationValidity() {
    if (currentLocationIndex >= locations.size()) {
        currentLocationIndex = static_cast<int>(locations.size()) - 1;
        setCurrentLocationStr(To::string(currentLocationIndex + 1));
    }
}

vector<string> Location::save(const LatLon& latLon, const string& labelStr) {
    setNumLocations(getNumLocations() + 1);
    const auto locNum = To::string(getNumLocations());
    Utility::writePref("LOC" + locNum + "_X", latLon.latStr());
    Utility::writePref("LOC" + locNum + "_Y", latLon.lonStr());
    Utility::writePref("LOC" + locNum + "_LABEL", labelStr);
    const auto wfoAndRadar = getWfoRadarSiteFromPoint(latLon);
    auto wfo = wfoAndRadar[0];
    auto radarSite = wfoAndRadar[1];
    if (wfo.empty()) {
        wfo = WfoSites::sites->getNearest(latLon);
    }
    if (radarSite.empty()) {
        radarSite = RadarSites::getNearestCode(latLon);
    }
    Utility::writePref("RID" + locNum, WString::toUpper(radarSite));
    Utility::writePref("NWS" + locNum, WString::toUpper(wfo));
    refresh();
    return {locNum, "Saving location " + locNum + " as " + labelStr + " (" + latLon.latStr() + "," + latLon.lonStr() + ") " + "/" + " " + WString::toUpper(wfo) + "(" + WString::toUpper(radarSite) + ")"};
}

void Location::deleteLocation(int index) {
    if (index > numberOfLocations) {
        return;
    }
    if (index == (numberOfLocations - 1)) {
        numberOfLocations -= 1;
    } else {
        for (auto i : range2(index + 1, numberOfLocations)) {
            const auto jStr = To::string(i + 1);
            const auto iStr = To::string(i);
            const auto x = Utility::readPref("LOC" + jStr + "_X", "");
            const auto y = Utility::readPref("LOC" + jStr + "_Y", "");
            const auto name = Utility::readPref("LOC" + jStr + "_LABEL", "");
            const auto wfo = Utility::readPref("NWS" + jStr, "");
            const auto rid = Utility::readPref("RID" + jStr, "");
            Utility::writePref("LOC" + iStr + "_X", x);
            Utility::writePref("LOC" + iStr + "_Y", y);
            Utility::writePref("LOC" + iStr + "_LABEL", name);
            Utility::writePref("NWS" + iStr, wfo);
            Utility::writePref("RID" + iStr, rid);
        }
        numberOfLocations -= 1;
    }
    setCurrentLocationStr("1");
    refresh();
}

void Location::setCurrentLocationStr(const string& indexAsString) {
    Utility::writePref("CURRENT_LOC_FRAGMENT", indexAsString);
    Utility::writePrefInt("LOC_NUM_INT", getNumLocations());
    currentLocationIndex = To::Int(indexAsString) - 1;
}

vector<string> Location::listOfNames() {
    vector<string> names;
    std::transform(locations.cbegin(), locations.cend(), std::back_inserter(names), [] (const auto& location) { return location.getName(); });
    return names;
}

void Location::setCurrentLocation(int index) {
    currentLocationIndex = index;
    Utility::writePrefInt("CURRENT_LOC_FRAGMENT", index + 1);
}

vector<LatLon> Location::getListLatLons() {
    vector<LatLon> latLons;
    for (auto index : range(locations.size())) {
        latLons.push_back(getLatLon(index));
    }
    return latLons;
}

string Location::getObs() {
    return Metar::findClosestObservation(getLatLonCurrent()).codeName;
}

size_t Location::getCurrentLocation() {
    return currentLocationIndex;
}

void Location::setMainScreenComboBox() {
    comboBox->block();
    comboBox->setList(listOfNames());
    comboBox->setIndex(getCurrentLocation());
    comboBox->unblock();
}

vector<string> Location::getWfoRadarSiteFromPoint(const LatLon& latLon) {
    const auto pointData = UtilityIO::getHtml("https://api.weather.gov/points/" + latLon.latForNws() + "," + latLon.lonForNws());
    // "cwa": "IWX",
    // "radarStation": "KGRR"
    const auto wfo = UtilityString::parse(pointData, "\"cwa\": \"(.*?)\"");
    const auto radarStation = UtilityString::parse(pointData, "\"radarStation\": \"(.*?)\"");
    return {wfo, radarStation};
}
