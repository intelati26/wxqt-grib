// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef LOCATION_H
#define LOCATION_H

#include <string>
#include <vector>
#include "objects/LatLon.h"
#include "settings/ObjectLocation.h"
#include "ui/ComboBox.h"

using std::string;
using std::vector;

class Location {
public:
    static int getNumLocations();
    static void setNumLocations(int);
    static string radarSite();
    static string wfo();
    static string getRadarSite(int);
    static string getWfo(int);
    static LatLon getLatLon(int);
    static string name();
    static string getName(size_t);
    static void setName(size_t, const string&);
    static string state();
    static LatLon getLatLonCurrent();
    static void refresh();
    static void initNumLocations();
    static void checkCurrentLocationValidity();
    static vector<string> save(const LatLon&, const string&);
    static void deleteLocation(int);
    static void setCurrentLocationStr(const string&);
    static vector<string> listOfNames();
    static void setCurrentLocation(int);
    static vector<LatLon> getListLatLons();
    static string getObs();
    static size_t getCurrentLocation();
    static void setMainScreenComboBox();
    static ComboBox * comboBox;

private:
    static vector<string> getWfoRadarSiteFromPoint(const LatLon&);
    static int numberOfLocations;
    static size_t currentLocationIndex;
    static vector<ObjectLocation> locations;
};

#endif  // LOCATION_H
