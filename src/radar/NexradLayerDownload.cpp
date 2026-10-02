// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "NexradLayerDownload.h"
#include "radar/HistoricalWarnings.h"
#include "objects/PolygonWarning.h"
#include "objects/PolygonWatch.h"
#include "radar/FireDayOne.h"
#include "radar/Metar.h"
#include "radar/SwoDayOne.h"
#include "radar/WpcFronts.h"
#include "radar/NexradLevel3HailIndex.h"
#include "radar/NexradLevel3StormInfo.h"
#include "radar/NexradLevel3Tvs.h"
#include "settings/RadarPreferences.h"
#include "util/UtilityList.h"

NexradLayerDownload::NexradLayerDownload(Window * parent, vector<NexradWidget *> * nexradList)
    : parent{parent}
    , nexradList{nexradList}
    , mtx{std::make_unique<std::mutex>()}
{}

void NexradLayerDownload::downloadLayers() {
    mtx->lock();
    if (const auto history = (*nexradList)[0]->historyTime(); history.isValid()) {
        // a past time: the warnings that were in effect then, from the archive; nothing else is live data to show
        new FutureVoid{parent,
            [history] {
                for (auto type : PolygonWarning::polygonList) {
                    HistoricalWarnings::polygonsAt(type, history);   // one download, kept for the next type
                }
            },
            [this] {
                for (auto nw : *nexradList) {
                    for (auto type : PolygonWarning::polygonList) {
                        nw->processHistoricalWarnings(type);
                    }
                    nw->update();
                }
            }};
        mtx->unlock();
        return;
    }
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
    if (RadarPreferences::sti) {
        for (auto i : range(nexradList->size())) {
            auto * nw = (*nexradList)[i];
            nw->runJob([nw] { NexradLevel3StormInfo::decode(nw->nexradState.getPn(), nw->fileStorage); },
                       [this, i] { constructSti(i); });
        }
    }
    if (RadarPreferences::hailIndex) {
        for (auto i : range(nexradList->size())) {
            auto * nw = (*nexradList)[i];
            nw->runJob([nw] { NexradLevel3HailIndex::decode(nw->nexradState.getPn(), nw->fileStorage); },
                       [this, i] { constructHi(i); });
        }
    }
    if (RadarPreferences::tvs) {
        for (auto i : range(nexradList->size())) {
            auto * nw = (*nexradList)[i];
            nw->runJob([nw] { NexradLevel3Tvs::decode(nw->nexradState.getPn(), nw->fileStorage); },
                       [this, i] { constructTvs(i); });
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

void NexradLayerDownload::constructHi(int i) {
    (*nexradList)[i]->constructHi();
    (*nexradList)[i]->update();
}

void NexradLayerDownload::constructSti(int i) {
    (*nexradList)[i]->constructSti();
    (*nexradList)[i]->update();
}

void NexradLayerDownload::constructTvs(int i) {
    (*nexradList)[i]->constructTvs();
    (*nexradList)[i]->update();
}

void NexradLayerDownload::constructWpcFronts() {
    for (auto nw : *nexradList) {
        nw->constructWpcFronts();
        nw->update();
    }
}
