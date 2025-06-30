// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "ObjectLocation.h"
#include "objects/WString.h"
#include "radar/RadarSites.h"
#include "util/To.h"
#include "util/Utility.h"

ObjectLocation::ObjectLocation(int locationNumber)
    : locNumAsString{To::string(locationNumber + 1) }
    , lat{Utility::readPref("LOC" + locNumAsString + "_X", "35.231")}
    , lon{Utility::readPref("LOC" + locNumAsString + "_Y", "-97.451")}
    , name{Utility::readPref("LOC" + locNumAsString + "_LABEL", "Home")}
    , wfo{Utility::readPref("NWS" + locNumAsString, "OUN")}
    , rid{Utility::readPref("RID" + locNumAsString, "KTLX")}
    , state{WString::split(RadarSites::getName(rid), ",")[0]}
{}

void ObjectLocation::saveToNewSlot(int newLocNumInt) {
    locNumAsString = To::string(newLocNumInt + 1);
    Utility::writePref("LOC" + locNumAsString + "_X", lat);
    Utility::writePref("LOC" + locNumAsString + "_Y", lon);
    Utility::writePref("LOC" + locNumAsString + "_LABEL", name);
    Utility::writePref("NWS" + locNumAsString, wfo);
    Utility::writePref("RID" + locNumAsString, rid);
}

string ObjectLocation::getLat() const {
    return lat;
}

string ObjectLocation::getLon() const {
    return lon;
}

LatLon ObjectLocation::getLatLon() const {
    return {getLat(), getLon()};
}

string ObjectLocation::getName() const {
    return name;
}

string ObjectLocation::getWfo() const {
    return wfo;
}

string ObjectLocation::getRadarSite() const {
    return rid;
}

string ObjectLocation::getState() const {
    return state;
}
