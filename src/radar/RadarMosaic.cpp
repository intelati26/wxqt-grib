// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "RadarMosaic.h"
#include "objects/FutureBytes.h"
#include "radar/UtilityRadarMosaic.h"
#include "settings/Location.h"
#include "settings/UIPreferences.h"
#include "util/Utility.h"
#include "util/UtilityList.h"

RadarMosaic::RadarMosaic(Window * parent, const string& sector)
    : Window{parent}
    , autoUpdate{parent, "AUTO_UPDATE_INTERVAL_RADAR_MOSAIC", 5, [this] { reload(); }}
    , photo{this, FullWithHeight, [this] { return getPhotoHeight(); }}
    , comboboxSector{this, UtilityRadarMosaic::sectors}
    , objectAnimate{this, &photo, &UtilityRadarMosaic::getAnimation, [this] { reload(); }}
    , shortcutAnimate{QKeySequence{"A"}, this}
    , shortcutAutoUpdate{QKeySequence{"U"}, this}
    , shortcutLocal{QKeySequence{"L"}, this}
    , shortcutConus{QKeySequence{"C"}, this}
{
    if (!UIPreferences::rememberMosaic) {
        objectAnimate.sector = UtilityRadarMosaic::getNearest(Location::getLatLonCurrent());
    } else {
        objectAnimate.sector = Utility::readPref("REMEMBER_MOSAIC_SECTOR", UtilityRadarMosaic::getNearest(Location::getLatLonCurrent()));
    }
    if (sector != "") {
        objectAnimate.sector = sector;
    }

    comboboxSector.setIndex(findex(objectAnimate.sector, UtilityRadarMosaic::sectors));
    comboboxSector.connect([this] { changeSector(); });

    boxH.addWidget(comboboxSector);
    boxH.addWidget(objectAnimate);
    boxH.addWidget(autoUpdate);
    box.addLayout(boxH);
    box.addWidgetAndCenter(photo);
    box.getAndShow(this);

    shortcutAnimate.connect([this] { objectAnimate.animateClicked(); });
    shortcutAutoUpdate.connect([this] { autoUpdate.toggleAutoUpdate(); });
    shortcutLocal.connect([this] {
        objectAnimate.sector = UtilityRadarMosaic::getNearest(Location::getLatLonCurrent());
        reload();
     });
    shortcutConus.connect([this] {
        objectAnimate.sector = "CONUS";
        reload();
     });

    reload();
}

void RadarMosaic::reload() {
    setTitle("Radar Mosaics " + autoUpdate.titleAdd);
    objectAnimate.stopAnimateNoDownload();
    Utility::writePref("REMEMBER_MOSAIC_SECTOR", objectAnimate.sector);
    const auto url = UtilityRadarMosaic::get(objectAnimate.sector);
    new FutureBytes{this, url, [this] (const auto& ba) { photo.setBytes(ba); }};
}

void RadarMosaic::changeSector() {
    objectAnimate.sector = UtilityRadarMosaic::sectors[comboboxSector.getIndex()];
    objectAnimate.stopAnimate();
    reload();
}

void RadarMosaic::resizeEventCustom() {
    photo.setToHeight(getWindowHeight());
}

void RadarMosaic::closeEventCustom() {
    objectAnimate.stopAnimateNoDownload();
    autoUpdate.stopNoDownload();
}
