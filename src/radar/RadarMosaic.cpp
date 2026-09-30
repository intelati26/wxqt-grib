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
    , image{this}
    , comboboxSector{this, UtilityRadarMosaic::sectors}
    , objectAnimate{this, &image, &UtilityRadarMosaic::getAnimation}
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
    boxH.addWidget(autoUpdate);
    box.addLayout(boxH);
    objectAnimate.addTo(box);
    box.addWidgetReal(&image, 1, Qt::Alignment{});
    box.getAndShow(this);

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
    new FutureBytes{this, url, [this] (const auto& ba) { showLatest(ba); }};
    objectAnimate.refresh();
}

// the newest still image; also what Save exports when no loop has been rendered
void RadarMosaic::showLatest(const QByteArray& bytes) {
    image.setBytesKeepView(bytes);
    objectAnimate.setCurrentBytes(bytes);
}

void RadarMosaic::changeSector() {
    objectAnimate.sector = UtilityRadarMosaic::sectors[comboboxSector.getIndex()];
    objectAnimate.stopAnimateNoDownload();
    reload();
}

void RadarMosaic::resizeEventCustom() {
    // ZoomImage re-fits itself on resize while the user has not zoomed
}

void RadarMosaic::closeEventCustom() {
    objectAnimate.stopAnimateNoDownload();
    autoUpdate.stopNoDownload();
}
