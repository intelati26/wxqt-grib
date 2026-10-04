// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "NexradLayerDownload.h"
#include "objects/PolygonWarning.h"
#include "objects/PolygonWatch.h"
#include "radar/FireDayOne.h"
#include "radar/Metar.h"
#include "radar/SwoDayOne.h"
#include "radar/WpcFronts.h"
#include "settings/RadarPreferences.h"
#include "util/UtilityList.h"

NexradLayerDownload::NexradLayerDownload(Window * parent, vector<NexradWidget *> * nexradList)
    : parent{parent}
    , nexradList{nexradList}
    , mtx{std::make_unique<std::mutex>()}
{}

void NexradLayerDownload::downloadLayers() {
    mtx->lock();
    for (auto polygonGenericType : PolygonWarning::polygonList) {
        if (PolygonWarning::byType[polygonGenericType]->isEnabled) {
            new FutureVoid{parent, [polygonGenericType] { PolygonWarning::byType[polygonGenericType]->download(); },
            [this, polygonGenericType] { updateWarnings(polygonGenericType); }};
        }
    }
    for (const auto t : {Mcd, Watch, Mpd}) {
        if (PolygonWatch::byType[t]->isEnabled) {
            new FutureVoid{parent,
                [t] { PolygonWatch::byType[t]->download(); },
                [this, t] { processWatch(t); }};
        }
    }
    if (RadarPreferences::swo) {
        new FutureVoid{parent,
            [] { SwoDayOne::get(); },
            [this] { constructSwo(); }};
    }
    if (RadarPreferences::fire) {
        new FutureVoid{parent,
            [] { FireDayOne::get(); },
            [this] { constructFire(); }};
    }
    if (RadarPreferences::obsWindbarbs || RadarPreferences::obs) {
        for (auto i : range(nexradList->size())) {
            auto * nw = (*nexradList)[i];
            nw->runJob([nw] { Metar::getStateMetarArrayForWXOGL(nw->nexradState.getRadarSite(), nw->fileStorage); },
                       [this, i] { constructWBLines(i); });
        }
    }
    if (RadarPreferences::wpcFronts) {
        new FutureVoid{parent,
            [] { WpcFronts::get(); },
            [this] { constructWpcFronts(); }};
    }
    mtx->unlock();
}

void NexradLayerDownload::updateWarnings(PolygonType type) {
    for (auto nw : *nexradList) {
        nw->processWarnings(type);
        nw->update();
    }
}

void NexradLayerDownload::processWatch(PolygonType type) {
    for (auto nw : *nexradList) {
        nw->process(type);
        if (type == Watch) {
            nw->process(WatchTornado);
        }
        nw->update();
    }
}

void NexradLayerDownload::constructWBLines(int i) {
    (*nexradList)[i]->constructWBLines();
    (*nexradList)[i]->update();
}

void NexradLayerDownload::constructSwo() {
    for (auto nw : *nexradList) {
        nw->constructSwo();
        nw->update();
    }
}

void NexradLayerDownload::constructFire() {
    for (auto nw : *nexradList) {
        nw->constructFire();
        nw->update();
    }
}

void NexradLayerDownload::constructWpcFronts() {
    for (auto nw : *nexradList) {
        nw->constructWpcFronts();
        nw->update();
    }
}
