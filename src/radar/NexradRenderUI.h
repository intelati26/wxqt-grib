// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef NEXRADRENDERUI_H
#define NEXRADRENDERUI_H

#include <string>
#include "objects/LatLon.h"
#include "radar/NexradState.h"
#include "radar/PolygonType.h"
#include "ui/Window.h"

using std::string;

class NexradRenderUI {
public:
    static void showPolygonText(Window *, const LatLon&);
    static void showNearestProduct(Window *, PolygonType, const LatLon&);
    static LatLon getLatLonFromScreenPosition(const NexradState&, double, double);
    static void showRadarStatusMessage(Window *, const string&);
};

#endif  // NEXRADRENDERUI_H
