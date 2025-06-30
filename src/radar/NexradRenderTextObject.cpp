// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "NexradRenderTextObject.h"
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

const double NexradRenderTextObject::cityMinZoom{0.20};
const double NexradRenderTextObject::obsMinZoom{0.20};
const double NexradRenderTextObject::countyMinZoom{0.20};

NexradRenderTextObject::NexradRenderTextObject(int numPanes, NexradState * nexradState, FileStorage * fileStorage)
    : nexradState{nexradState}
    , fileStorage{fileStorage}
    , maxCitiesPerGlview{static_cast<size_t>(40.0 / numPanes)}
{
    initialize();
}

void NexradRenderTextObject::addTextLabelsCitiesExtended() {
    if (RadarPreferences::cities) {
        nexradState->cities.clear();
        if (nexradState->zoom > cityMinZoom) {
            const auto cityExtLength = CitiesExtended::cities.size();
            for (auto index : range(cityExtLength)) {
                if (nexradState->cities.size() <= maxCitiesPerGlview) {
                    checkAndDrawText(
                        nexradState->cities,
                        CitiesExtended::cities[index].latitude,
                        CitiesExtended::cities[index].longitude,
                        CitiesExtended::cities[index].name,
                        true);
                }
            }
        }
    }
}

void NexradRenderTextObject::checkAndDrawText(vector<TextViewMetal>& tvList, double lat, double lon, const string& text, bool checkBounds) {
    const auto latLon = Projection::computeMercatorNumbers(lat, lon, nexradState->getPn());
    const auto xPos = latLon[0];
    const auto yPos = latLon[1];
    const auto dimScale = 0.5;
    if (checkBounds
        && nexradState->originalWidth * -1.0 * dimScale < (xPos * nexradState->zoom)
        && (xPos * nexradState->zoom)  < nexradState->originalWidth * dimScale
        &&  nexradState->originalHeight * -1.0 * dimScale < (yPos * nexradState->zoom)
        && (yPos * nexradState->zoom)  < nexradState->originalHeight * dimScale
    ) {
        tvList.emplace_back(xPos, yPos, text);
    } else if (!checkBounds) {
        tvList.emplace_back(xPos, yPos, text);
    }
}

void NexradRenderTextObject::initializeTextLabelsCitiesExtended() const {
    if (RadarPreferences::cities) {
        CitiesExtended::create();
    }
}

void NexradRenderTextObject::initializeTextLabelsCountyLabels() {
    if (RadarPreferences::countyLabels) {
        CountyLabels::create();
    }
}

void NexradRenderTextObject::addTextLabelsCountyLabels() {
    if (RadarPreferences::countyLabels) {
        nexradState->countyLabels.clear();
        if (nexradState->zoom > countyMinZoom) {
            for (auto index : range(CountyLabels::names.size())) {
                checkAndDrawText(
                    nexradState->countyLabels,
                    CountyLabels::location[index].lat(),
                    CountyLabels::location[index].lon(),
                    CountyLabels::names[index],
                    true);
            }
        }
    }
}

void NexradRenderTextObject::initialize() {
    initializeTextLabelsCitiesExtended();
    initializeTextLabelsCountyLabels();
}

void NexradRenderTextObject::add() {
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

void NexradRenderTextObject::addWpcPressureCenters() {
    if (RadarPreferences::wpcFronts) {
        nexradState->pressureCenterLabelsRed.clear();
        nexradState->pressureCenterLabelsBlue.clear();
        if (nexradState->zoom < nexradState->zoomToHideMiscFeatures) {
            for (const auto& p : WpcFronts::pressureCenters) {
                if (p.centerType == LOW) {
                    checkAndDrawText(nexradState->pressureCenterLabelsRed, p.lat, p.lon, p.pressureInMb, false);
                } else {
                    checkAndDrawText(nexradState->pressureCenterLabelsBlue, p.lat, p.lon, p.pressureInMb, false);
                }
            }
        }
    }
}

void NexradRenderTextObject::addTextLabelsObservations() {
    if (RadarPreferences::obs || RadarPreferences::obsWindbarbs) {
        nexradState->observations.clear();
        if (nexradState->zoom > obsMinZoom) {
            for (auto index : range(fileStorage->obsArr.size())) {
                if (index < fileStorage->obsArr.size() && index < fileStorage->obsArrExt.size()) {
                    const auto tmpArrObs = WString::split(fileStorage->obsArr[index], ":");
                    const auto lat = To::Double(tmpArrObs[0]);
                    const auto lon = To::Double(tmpArrObs[1]);
                    checkAndDrawText(nexradState->observations, lat, -1.0 * lon, tmpArrObs[2], true);
                }
            }
        }
    }
}
