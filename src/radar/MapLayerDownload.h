// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef MAPLAYERDOWNLOAD_H
#define MAPLAYERDOWNLOAD_H

#include <memory>
#include <mutex>
#include <vector>
#include "objects/FutureVoid.h"
#include "radar/MapWidget.h"
#include "radar/PolygonType.h"
#include "ui/Window.h"

using std::unique_ptr;
using std::vector;

class MapLayerDownload {
public:
    MapLayerDownload(Window *, vector<MapWidget *> *);
    void downloadLayers();

private:
    void updateWarnings(PolygonType);
    void processWatch(PolygonType);
    void constructWBLines(int);
    void constructSwo();
    void constructFire();
    void constructWpcFronts();
    Window * parent;
    vector<MapWidget *> * mapList;
    vector<unique_ptr<FutureVoid>> futures;
    unique_ptr<std::mutex> mtx;
};

#endif  // MAPLAYERDOWNLOAD_H
