// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef OBJECTLOCATION_H
#define OBJECTLOCATION_H

#include <string>
#include "objects/LatLon.h"

using std::string;

class ObjectLocation {
public:
    explicit ObjectLocation(int);
    void saveToNewSlot(int);
    string getLat() const;
    string getLon() const;
    LatLon getLatLon() const;
    string getName() const;
    string getWfo() const;
    string getRadarSite() const;
    string getState() const;

private:
    string locNumAsString;
    string lat;
    string lon;
    string name;
    string wfo;
    string rid;
    string state;
};

#endif  // OBJECTLOCATION_H
