// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "GoesViewer.h"
#include <algorithm>
#include "objects/FutureBytes.h"
#include "objects/WString.h"
#include "settings/Location.h"
#include "settings/UIPreferences.h"
#include "util/To.h"
#include "util/Utility.h"
#include "util/UtilityList.h"
#include "vis/UtilityGoes.h"

GoesViewer::GoesViewer(Window * parent, const string& url, const string& product, const string& sector, bool savePref)
    : Window{parent}
    , autoUpdate{parent, "GOES_AUTO_UPDATE_INTERVAL", 5, [this] { reload(); }}
    , photo{this, FullWithHeight, [this] { return getPhotoHeight(); }}
    , comboboxSector{this, UtilityGoes::sectors}
    , comboboxProduct{this, UtilityGoes::labels}
    , comboboxCount{this, {"6", "12", "18", "24"}}
    , objectAnimate{this, &photo, &UtilityGoes::getAnimation, [this] { reload(); }}
    , backForward{this, [this] { moveBack(); }, [this] { moveForward(); }}
    , goesFloater{false}
    , shortcutAnimate{QKeySequence{"A"}, this}
    , shortcutAutoUpdate{QKeySequence{"U"}, this}
    , savePref{savePref}
{
    if (!url.empty()) {
        goesFloater = true;
        goesFloaterUrl = url;
    }
    if (sector.empty()) {
        if (!UIPreferences::rememberGOES) {
            objectAnimate.sector = UtilityGoes::getNearest(Location::getLatLonCurrent());
        } else {
            objectAnimate.sector = Utility::readPref("REMEMBER_GOES_SECTOR", UtilityGoes::getNearest(Location::getLatLonCurrent()));
        }
    } else {
        objectAnimate.sector = sector;
    }
    if (product.empty()) {
        if (!UIPreferences::rememberGOES) {
            objectAnimate.product = "GEOCOLOR";
        } else {
            objectAnimate.product = Utility::readPref("REMEMBER_GOES_PRODUCT", "GEOCOLOR");
        }
    } else {
        objectAnimate.product = product;
    }
    if (goesFloater) {
        objectAnimate.getFunction = &UtilityGoes::getAnimationGoesFloater;
        objectAnimate.sector = goesFloaterUrl;
    }
    comboboxSector.setIndexByValue(objectAnimate.sector);
    comboboxSector.connect([this] { changeSector(); });

    auto indexProd = findex(objectAnimate.product, UtilityGoes::productCodes);
    comboboxProduct.setIndex(indexProd);
    comboboxProduct.connect([this] { changeProduct(); });

    comboboxCount.setIndex(1);
    comboboxCount.connect([this] { changeCount(); });

    if (!goesFloater) {
        boxH.addWidget(comboboxSector);
    } else {
        comboboxSector.setVisible(false);
    }
    boxH.addWidget(comboboxProduct);
    boxH.addWidget(comboboxCount);
    boxH.addWidget(objectAnimate);
    boxH.addWidget(autoUpdate);
    boxH.addLayout(backForward);
    box.addLayout(boxH);
    box.addWidgetAndCenter(photo);
    box.getAndShow(this);

    shortcutAnimate.connect([this] { objectAnimate.animateClicked(); });
    shortcutAutoUpdate.connect([this] { autoUpdate.toggleAutoUpdate(); });
    reload();
}

void GoesViewer::reload() {
    setTitle("GOES Viewer - " + UtilityGoes::labels[comboboxProduct.getIndex()] + " " + autoUpdate.titleAdd);
    objectAnimate.stopAnimateNoDownload();
    if (!goesFloater) {
        if (savePref) {
            Utility::writePref("REMEMBER_GOES_SECTOR", objectAnimate.sector);
            Utility::writePref("REMEMBER_GOES_PRODUCT", objectAnimate.product);
        }
        new FutureBytes{this, UtilityGoes::getImage(objectAnimate.product, objectAnimate.sector), [this] (const auto& ba) { photo.setBytes(ba); }};
    } else {
        new FutureBytes{this, UtilityGoes::getImageGoesFloater(goesFloaterUrl, objectAnimate.product), [this] (const auto& ba) { photo.setBytes(ba); }};
    }
}

void GoesViewer::moveBack() {
    auto index = comboboxProduct.getIndex();
    index -= 1;
    index = std::max(index, 0);
    comboboxProduct.setIndex(index);
}

void GoesViewer::moveForward() {
    auto index = comboboxProduct.getIndex();
    index += 1;
    index = std::min(index, static_cast<int>(UtilityGoes::productCodes.size()) - 1);
    comboboxProduct.setIndex(index);
}

void GoesViewer::changeSector() {
    objectAnimate.sector = WString::split(comboboxSector.getValue(), ":")[0];
    reload();
}

void GoesViewer::changeProduct() {
    objectAnimate.product = UtilityGoes::productCodes[comboboxProduct.getIndex()];
    reload();
}

void GoesViewer::changeCount() {
    objectAnimate.setFrameCount(To::Int(comboboxCount.getValue()));
}

void GoesViewer::resizeEventCustom() {
    photo.setToHeight(getWindowHeight());
}

void GoesViewer::closeEventCustom() {
    objectAnimate.stopAnimateNoDownload();
    autoUpdate.stopNoDownload();
}
