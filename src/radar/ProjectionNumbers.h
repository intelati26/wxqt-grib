// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef PROJECTIONNUMBERS_H
#define PROJECTIONNUMBERS_H

#include <string>
#include "objects/LatLon.h"

using std::string;

class ProjectionNumbers {
public:
    ProjectionNumbers();
    void setRadarSite(const string&);
    double getOneDegreeScaleFactor() const;
    string getRadarSite() const;
    LatLon getLatLon() const;
    double xCenter;
    double yCenter;

private:
    string radarSite;
    LatLon latLon;
    double scale;
    double oneDegreeScaleFactor;
};

#endif  // PROJECTIONNUMBERS_H
