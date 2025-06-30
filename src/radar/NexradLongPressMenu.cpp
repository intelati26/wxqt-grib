// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "NexradLongPressMenu.h"
#include <cmath>
// #include <numbers>
#include <vector>
#include "common/GlobalVariables.h"
#include "misc/AlertsDetail.h"
#include "misc/TextViewer.h"
#include "objects/PolygonWarning.h"
#include "objects/PolygonWatch.h"
#include "objects/Route.h"
#include "objects/WString.h"
#include "radar/Metar.h"
#include "radar/NexradUtil.h"
#include "radar/NexradRenderUI.h"
#include "radar/RadarSites.h"
#include "radar/UtilityRadarMosaic.h"
#include "settings/Location.h"
#include "settings/UtilityLocation.h"
#include "spc/UtilitySpcMeso.h"
#include "ui/CMenuItem.h"
#include "ui/LongPressMenu.h"
#include "util/SoundingSites.h"
#include "util/To.h"
#include "util/UtilityList.h"
#include "util/WfoSites.h"
#include "vis/UtilityGoes.h"

using std::vector;

void NexradLongPressMenu::setupContextMenu(
    Window * parent,
    const QPoint& posGlobal,
    const NexradState& nexradState,
    const LatLon& latLon,
    const function<void(int, const string&)>& fnSector,
    const function<void(int, const string&)>& fnProduct
) {
    const auto distFromLocation = To::string(static_cast<int>(LatLon::distance(latLon, Location::getLatLonCurrent())));
    const auto directionFromLocation = LatLon::calculateDirection(Location::getLatLonCurrent(), latLon);
    const auto obsSite = Metar::findClosestObservation(latLon, 0);
    const auto obsDirection = LatLon::calculateDirection(latLon, obsSite.latLon);
    const auto radarSites = RadarSites::getNearest(latLon, 5, true);
    const auto obsLabel = "Observation: " + obsSite.codeName + " " + obsSite.fullName + " " + To::string(obsSite.distance) + " mi " + obsDirection;

    const auto actionDistance = CMenuItem{latLon.printPretty() + " " + distFromLocation + " miles from location to the " + directionFromLocation, [] {}};
    const auto actionObservation = CMenuItem{obsLabel, [parent, obsSite, obsLabel] {
        const auto url = GlobalVariables::tgftpSitePrefix + "/data/observations/metar/decoded/" + obsSite.codeName + ".TXT";
        new TextViewer{parent, url, true, obsLabel};
    }};
    const auto actionSaveLocation = CMenuItem{"Save as a location: " + latLon.printPretty(), [latLon] {
        Location::save(latLon, latLon.printPretty() + " - " + UtilityLocation::getNearestCity(latLon));
        Location::setMainScreenComboBox();
    }};
    const auto actionAdhocLocation = CMenuItem{"View location: " + latLon.printPretty(), [parent, latLon] {
        Route::adhocLocation(parent, latLon);
    }};
    const auto actionWarnings = CMenuItem{"Show Warning", [parent, latLon] { NexradRenderUI::showPolygonText(parent, latLon); }};
    const auto actionWatch = CMenuItem{"Show Watch", [parent, latLon] { NexradRenderUI::showNearestProduct(parent, Watch, latLon); }};
    const auto actionMcd = CMenuItem{"Show MCD", [parent, latLon] { NexradRenderUI::showNearestProduct(parent, Mcd, latLon); }};
    const auto actionMpd = CMenuItem{"Show MPD", [parent, latLon] { NexradRenderUI::showNearestProduct(parent, Mpd, latLon); }};
    const auto closestRadar = radarSites[0].codeName;
    const auto actionRadarStatusMessage = CMenuItem{"Show radar status message: " + closestRadar, [parent, closestRadar] {
        NexradRenderUI::showRadarStatusMessage(parent, closestRadar);
    }};
    //
    // radar sites
    //
    vector<CMenuItem> actionRadarSitesList;
    for (const auto& rid : radarSites) {
        const auto bearingToRadar = LatLon::calculateDirection(latLon, rid.latLon);
        const auto radarDescription = rid.codeName + " " + RadarSites::getName(rid.codeName) + " " + To::string(static_cast<int>(round(rid.distance))) + " mi " + bearingToRadar;
        actionRadarSitesList.emplace_back(radarDescription, [&rid, &fnSector, &nexradState] { fnSector(nexradState.paneNumber, rid.codeName); });
    }
    //
    // products
    //
    auto productList = NexradUtil::radarProductList;
    auto prodLength = 5;
    if (nexradState.isTdwrSite()) {
        productList = NexradUtil::radarProductListTdwr;
        prodLength = 3;
    }
    vector<CMenuItem> productActions;
    for (const auto& product : vector<string>{productList.begin(), productList.begin() + prodLength}) {
        const auto productShortCode = WString::split(product, ":")[0];
        productActions.emplace_back(product, [productShortCode, &nexradState, fnProduct] {
            fnProduct(nexradState.paneNumber, productShortCode);
        });
    }

    const auto nearestSoundingCode = SoundingSites::sites->getNearest(latLon);
    const auto nearestSoundingLatLon = SoundingSites::sites->byCode[nearestSoundingCode]->latLon;
    const auto bearingToSounding = LatLon::calculateDirection(latLon, nearestSoundingLatLon);
    const auto actionSounding = CMenuItem{"Sounding: " + nearestSoundingCode + " " + To::string(SoundingSites::sites->getNearestInMiles(latLon)) + " mi " + bearingToSounding,
        [parent, nearestSoundingCode] { Route::spcSoundingBySector(parent, nearestSoundingCode); }};

    const auto nearestVisCode = UtilityGoes::getNearest(latLon);
    const auto actionVis = CMenuItem{"Vis Sat: " + nearestVisCode, [parent, nearestVisCode] { Route::visBySector(parent, nearestVisCode); }};

    const auto nearestWfo = WfoSites::sites->getNearest(latLon);
    const auto nearestWfoLatLon = WfoSites::sites->byCode[nearestWfo]->latLon;
    const auto bearingToWfo = LatLon::calculateDirection(latLon, nearestWfoLatLon);
    const auto actionWfo = CMenuItem{"AFD: " + nearestWfo + " " + To::string(WfoSites::sites->getNearestInMiles(latLon)) + " mi " + bearingToWfo,
        [parent, nearestWfo] { Route::wfoTextBySector(parent, nearestWfo); }};

    auto nearestSpcMeso = UtilitySpcMeso::getNearest(latLon);
    auto actionSpcMeso = CMenuItem{"Spc Meso: " + UtilitySpcMeso::sectorMap.at(nearestSpcMeso), [parent, nearestSpcMeso] { Route::spcMesoBySector(parent, nearestSpcMeso); }};

    auto nearestRadarMosaic = UtilityRadarMosaic::getNearest(latLon);
    auto actionRadarMosaic = CMenuItem{"Radar Mosaic: " + nearestRadarMosaic, [parent, nearestRadarMosaic] { Route::radarMosaicBySector(parent, nearestRadarMosaic); }};

    LongPressMenu longPressMenu{parent};
    longPressMenu.add(actionDistance);
    longPressMenu.add(actionObservation);
    longPressMenu.add(actionSaveLocation);
    longPressMenu.add(actionAdhocLocation);
    for (const auto& action : actionRadarSitesList) {
        longPressMenu.add(action);
    }
    if (PolygonWarning::areAnyEnabled() && PolygonWarning::isCountNonZero()) {
        longPressMenu.add(actionWarnings);
    }
    if (PolygonWatch::byType[PolygonType::Watch]->isEnabled && (!PolygonWatch::byType[PolygonType::Watch]->latLonList.getValue().empty() || !PolygonWatch::watchLatlonCombined.getValue().empty())) {
        longPressMenu.add(actionWatch);
    }
    if (PolygonWatch::byType[PolygonType::Mcd]->isEnabled && !PolygonWatch::byType[PolygonType::Mcd]->latLonList.getValue().empty()) {
        longPressMenu.add(actionMcd);
    }
    if (PolygonWatch::byType[PolygonType::Mpd]->isEnabled && !PolygonWatch::byType[PolygonType::Mpd]->latLonList.getValue().empty()) {
        longPressMenu.add(actionMpd);
    }
    for (const auto& action : productActions) {
        longPressMenu.add(action);
    }
    longPressMenu.add(actionRadarStatusMessage);
    longPressMenu.add(actionSounding);
    longPressMenu.add(actionVis);
    longPressMenu.add(actionWfo);
    longPressMenu.add(actionSpcMeso);
    longPressMenu.add(actionRadarMosaic);
    longPressMenu.show(posGlobal);
}
