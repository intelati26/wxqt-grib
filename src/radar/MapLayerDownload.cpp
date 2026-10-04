// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "MapLayerDownload.h"
#include "objects/PolygonWarning.h"
#include "objects/PolygonWatch.h"
#include "radar/FireDayOne.h"
#include "radar/Metar.h"
#include "radar/SwoDayOne.h"
#include "radar/WpcFronts.h"
#include "settings/RadarPreferences.h"
#include "util/UtilityList.h"

MapLayerDownload::MapLayerDownload(Window * parent, vector<MapWidget *> * mapList)
    : parent{parent}
    , mapList{mapList}
    , mtx{std::make_unique<std::mutex>()}
{}

void MapLayerDownload::downloadLayers() {
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
        for (auto i : range(mapList->size())) {
            auto * nw = (*mapList)[i];
            nw->runJob([nw] { Metar::getStateMetarArrayForWXOGL(nw->mapState.getRadarSite(), nw->fileStorage); },
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

void MapLayerDownload::updateWarnings(PolygonType type) {
    for (auto nw : *mapList) {
        nw->processWarnings(type);
        nw->update();
    }
}

void MapLayerDownload::processWatch(PolygonType type) {
    for (auto nw : *mapList) {
        nw->process(type);
        if (type == Watch) {
            nw->process(WatchTornado);
        }
        nw->update();
    }
}

void MapLayerDownload::constructWBLines(int i) {
    (*mapList)[i]->constructWBLines();
    (*mapList)[i]->update();
}

void MapLayerDownload::constructSwo() {
    for (auto nw : *mapList) {
        nw->constructSwo();
        nw->update();
    }
}

void MapLayerDownload::constructFire() {
    for (auto nw : *mapList) {
        nw->constructFire();
        nw->update();
    }
}

void MapLayerDownload::constructWpcFronts() {
    for (auto nw : *mapList) {
        nw->constructWpcFronts();
        nw->update();
    }
}
