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
    , image{this}
    , comboboxSector{this, UtilityGoes::sectors}
    , comboboxProduct{this, UtilityGoes::labels}
    , comboboxCount{this, {"6", "12", "18", "24"}}
    , comboboxSize{this, {"Default size"}}
    , objectAnimate{this, &image, &UtilityGoes::getAnimation}
    , backForward{this, [this] { moveBack(); }, [this] { moveForward(); }}
    , goesFloater{false}
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
    if (!goesFloater) {
        comboboxSize.connect([this] { changeSize(); });
        boxH.addWidget(comboboxSize);
    } else {
        comboboxSize.setVisible(false);
    }
    boxH.addWidget(autoUpdate);
    boxH.addLayout(backForward);
    box.addLayout(boxH);
    notice.setWordWrap(true);
    notice.setStyleSheet("QLabel { background: #fff3cd; color: #664d03; padding: 4px 8px; border-radius: 3px; }");
    notice.hide();
    box.addWidgetReal(&notice);
    objectAnimate.addTo(box);
    box.addWidgetReal(&image, 1, Qt::Alignment{});
    box.getAndShow(this);

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
        loadSizes();
        loadImage();
    } else {
        new FutureBytes{this, UtilityGoes::getImageGoesFloater(goesFloaterUrl, objectAnimate.product), [this] (const auto& ba) { showLatest(ba); }};
    }
    objectAnimate.refresh();
}

// the sizes STAR offers depend on the sector and on the product, so ask its directory listing
void GoesViewer::loadSizes() {
    auto listing = UtilityGoes::getImage(objectAnimate.product, objectAnimate.sector);
    listing = listing.substr(0, listing.rfind('/') + 1);
    new FutureBytes{this, listing, [this] (const auto& ba) {
        auto sizes = UtilityGoes::parseSizes(ba.toStdString());
        if (sizes.empty()) {
            return;   // listing unavailable: keep whatever the list already offers
        }
        sizes.insert(sizes.begin(), "Default size");
        comboboxSize.block();
        comboboxSize.setList(sizes);
        // keep the chosen size when this product offers it too, otherwise fall back to the default
        const auto it = std::find(sizes.begin(), sizes.end(), sizeChoice);
        comboboxSize.setIndex(it == sizes.end() ? 0 : static_cast<size_t>(it - sizes.begin()));
        if (it == sizes.end()) {
            sizeChoice.clear();
        }
        comboboxSize.unblock();
    }};
}

void GoesViewer::loadImage() {
    new FutureBytes{this, UtilityGoes::getImage(objectAnimate.product, objectAnimate.sector, sizeChoice), [this] (const auto& ba) { showLatest(ba); }};
}

void GoesViewer::changeSize() {
    sizeChoice = comboboxSize.getIndex() <= 0 ? "" : comboboxSize.getValue();
    loadImage();
}

// the newest still image; also what Save exports when no loop has been rendered
void GoesViewer::showLatest(const QByteArray& bytes) {
    if (QImage::fromData(bytes).isNull()) {
        // a missing file on the server comes back empty or as an error page; say so instead of leaving the old image unexplained
        const auto what = sizeChoice.empty() ? string{"the latest image"} : "the " + sizeChoice + " image";
        notice.setText(QString::fromStdString("Could not load " + what + " for this product and sector - it may not exist on the server. " +
                                              (sizeChoice.empty() ? "Check the connection and try again." : "Try a different size.")));
        notice.show();
        return;
    }
    notice.hide();
    image.setBytesKeepView(bytes);
    objectAnimate.setCurrentBytes(bytes);
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
    // ZoomImage re-fits itself on resize while the user has not zoomed
}

void GoesViewer::closeEventCustom() {
    objectAnimate.stopAnimateNoDownload();
    autoUpdate.stopNoDownload();
}
