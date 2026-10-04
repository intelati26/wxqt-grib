// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include <mutex>
#include "MapTextObject.h"
#include "objects/WString.h"
#include "radar/PressureCenterTypeEnum.h"
#include "radar/CitiesExtended.h"
#include "radar/CountyLabels.h"
#include "radar/Projection.h"
#include "radar/WpcFronts.h"
#include "settings/RadarPreferences.h"
#include "ui/TextViewMetal.h"
#include "util/To.h"
#include "util/UtilityList.h"

const double MapTextObject::cityMinZoom{0.20};
const double MapTextObject::obsMinZoom{0.20};
const double MapTextObject::countyMinZoom{0.20};

MapTextObject::MapTextObject(int numPanes, MapState * mapState, FileStorage * fileStorage)
    : mapState{mapState}
    , fileStorage{fileStorage}
    , maxCitiesPerGlview{static_cast<size_t>(40.0 / numPanes)}
{
    initialize();
}

void MapTextObject::addTextLabelsCitiesExtended() {
    if (RadarPreferences::cities) {
        mapState->cities.clear();
        if (mapState->zoom > cityMinZoom) {
            const auto cityExtLength = CitiesExtended::cities.size();
            for (auto index : range(cityExtLength)) {
                if (mapState->cities.size() <= maxCitiesPerGlview) {
                    checkAndDrawText(
                        mapState->cities,
                        CitiesExtended::cities[index].latitude,
                        CitiesExtended::cities[index].longitude,
                        CitiesExtended::cities[index].name,
                        true);
                }
            }
        }
    }
}

void MapTextObject::checkAndDrawText(vector<TextViewMetal>& tvList, double lat, double lon, const string& text, bool checkBounds) {
    const auto latLon = Projection::computeMercatorNumbers(lat, lon, mapState->getPn());
    const auto xPos = latLon[0];
    const auto yPos = latLon[1];
    const auto dimScale = 0.5;
    if (checkBounds
        && mapState->originalWidth * -1.0 * dimScale < (xPos * mapState->zoom)
        && (xPos * mapState->zoom)  < mapState->originalWidth * dimScale
        &&  mapState->originalHeight * -1.0 * dimScale < (yPos * mapState->zoom)
        && (yPos * mapState->zoom)  < mapState->originalHeight * dimScale
    ) {
        tvList.emplace_back(xPos, yPos, text);
    } else if (!checkBounds) {
        tvList.emplace_back(xPos, yPos, text);
    }
}

void MapTextObject::initializeTextLabelsCitiesExtended() const {
    if (RadarPreferences::cities) {
        CitiesExtended::create();
    }
}

void MapTextObject::initializeTextLabelsCountyLabels() {
    if (RadarPreferences::countyLabels) {
        CountyLabels::create();
    }
}

void MapTextObject::addTextLabelsCountyLabels() {
    if (RadarPreferences::countyLabels) {
        mapState->countyLabels.clear();
        if (mapState->zoom > countyMinZoom) {
            for (auto index : range(CountyLabels::names.size())) {
                checkAndDrawText(
                    mapState->countyLabels,
                    CountyLabels::location[index].lat(),
                    CountyLabels::location[index].lon(),
                    CountyLabels::names[index],
                    true);
            }
        }
    }
}

void MapTextObject::initialize() {
    initializeTextLabelsCitiesExtended();
    initializeTextLabelsCountyLabels();
}

void MapTextObject::add() {
    if (RadarPreferences::cities) {
        addTextLabelsCitiesExtended();
    }
    if (RadarPreferences::countyLabels) {
        addTextLabelsCountyLabels();
    }
    if (RadarPreferences::wpcFronts) {
        addWpcPressureCenters();
    }
    if (RadarPreferences::obs) {
        addTextLabelsObservations();
    }
}

void MapTextObject::addWpcPressureCenters() {
    if (RadarPreferences::wpcFronts) {
        mapState->pressureCenterLabelsRed.clear();
        mapState->pressureCenterLabelsBlue.clear();
        if (mapState->zoom < mapState->zoomToHideMiscFeatures) {
            for (const auto& p : WpcFronts::pressureCenters) {
                if (p.centerType == LOW) {
                    checkAndDrawText(mapState->pressureCenterLabelsRed, p.lat, p.lon, p.pressureInMb, false);
                } else {
                    checkAndDrawText(mapState->pressureCenterLabelsBlue, p.lat, p.lon, p.pressureInMb, false);
                }
            }
        }
    }
}

void MapTextObject::addTextLabelsObservations() {
    if (RadarPreferences::obs || RadarPreferences::obsWindbarbs) {
        mapState->observations.clear();
        if (mapState->zoom > obsMinZoom) {
            // copies taken under the lock: a worker may be replacing the lists while this runs (every pan step)
            vector<string> obsArr;
            vector<string> obsArrExt;
            {
                const std::lock_guard<std::mutex> guard{*fileStorage->lock};
                obsArr = fileStorage->obsArr;
                obsArrExt = fileStorage->obsArrExt;
            }
            for (auto index : range(obsArr.size())) {
                if (index < obsArr.size() && index < obsArrExt.size()) {
                    const auto tmpArrObs = WString::split(obsArr[index], ":");
                    const auto lat = To::Double(tmpArrObs[0]);
                    const auto lon = To::Double(tmpArrObs[1]);
                    checkAndDrawText(mapState->observations, lat, -1.0 * lon, tmpArrObs[2], true);
                }
            }
        }
    }
}
