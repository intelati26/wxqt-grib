// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef NEXRADLAYERDOWNLOAD_H
#define NEXRADLAYERDOWNLOAD_H

#include <memory>
#include <mutex>
#include <vector>
#include "objects/FutureVoid.h"
#include "radar/NexradWidget.h"
#include "radar/PolygonType.h"
#include "ui/Window.h"

using std::unique_ptr;
using std::vector;

class NexradLayerDownload {
public:
    NexradLayerDownload(Window *, vector<NexradWidget *> *);
    void downloadLayers();

private:
    void updateWarnings(PolygonType);
    void processWatch(PolygonType);
    void constructWBLines(int);
    void constructSwo();
    void constructFire();
    void constructHi(int);
    void constructSti(int);
    void constructTvs(int);
    void constructWpcFronts();
    Window * parent;
    vector<NexradWidget *> * nexradList;
    vector<unique_ptr<FutureVoid>> futures;
    unique_ptr<std::mutex> mtx;
};

#endif  // NEXRADLAYERDOWNLOAD_H
