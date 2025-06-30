// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef ROUTE_H
#define ROUTE_H

#include <string>
#include "objects/LatLon.h"
#include "ui/Window.h"

using std::string;

class Route {
public:
    static void adhocLocation(Window *, const LatLon&);
    static void lightning(Window *);
    static void nexradRadar(Window *, int);
    static void nexradRadarSpecificSite(Window *, const string&);
    static void radarMosaicBySector(Window *, const string&);
    static void radarMosaic(Window *);
    static void spcSoundingBySector(Window *, const string& = "");
    static void spcMesoBySector(Window *, const string&);
    static void vis(Window *);
    static void visBySector(Window *, const string&);
    static void wfoTextBySector(Window *, const string&);
};

#endif  // ROUTE_H
