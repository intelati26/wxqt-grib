// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "SpcMeso.h"
#include <algorithm>
#include "objects/FutureBytes.h"
#include "spc/UtilitySpcMeso.h"
#include "spc/UtilitySpcMesoInputOutput.h"
#include "util/To.h"
#include "util/Utility.h"
#include "util/UtilityList.h"

SpcMeso::SpcMeso(Window * parent, const string& productCode, const string& sectorCode, bool savePrefs)
    : Window{parent}
    , autoUpdate{this, "SPCMESO_AUTO_UPDATE_INTERVAL", 15, [this] { reload(); }}
    , photo{this, FullWithHeight, [this] { return getPhotoHeight(); }}
    , comboboxSector{this, UtilitySpcMeso::sectors}
    , objectAnimate{this, &photo, &UtilitySpcMesoInputOutput::getAnimation, [this] { reload(); }}
    , backForward{this, [this] { moveBack(); }, [this] { moveForward(); }}
    , savePrefs{savePrefs}
    , shortcutAnimate{QKeySequence{"A"}, this}
    , shortcutAutoUpdate{QKeySequence{"U"}, this}
{
    objectAnimate.product = Utility::readPref(prefTokenProduct, "pmsl");
    objectAnimate.sector = Utility::readPref(prefTokenSector, "19");

    if (sectorCode != "") {
        objectAnimate.sector = sectorCode;
    }
    if (productCode != "") {
        objectAnimate.product = productCode;
        this->savePrefs = false;
    }

    index = findex(objectAnimate.product, UtilitySpcMeso::products);

    comboboxSector.setIndex(findex(objectAnimate.sector, UtilitySpcMeso::sectorCodes));
    comboboxSector.connect([this] { changeSector(); });

    boxFav.addWidget(comboboxSector);
    boxFav.addLayout(backForward);
    boxFav.addWidget(objectAnimate);
    boxFav.addWidget(autoUpdate);
    box.addLayout(boxH);

    auto j = 0;
    for (const auto& item : UtilitySpcMeso::favList) {
        buttons.emplace_back(this, None, "");
        buttons.back().setText(item);
        buttons.back().connect([this, j] { changeProductForFav(j); });
        boxFav.addWidget(buttons.back());
        j += 1;
    }
    boxFav.addStretch();

    imageLayout.addLayout(boxFav);
    imageLayout.addWidgetAndCenter(photo);
    box.addLayout(imageLayout);
    box.getAndShow(this);

    auto itemsSoFar = 0;
    for (auto& menu : UtilitySpcMeso::titles) {
        menu.setList(UtilitySpcMeso::labels, itemsSoFar);
        itemsSoFar += menu.count;
    }
    for (auto& menuTitle : UtilitySpcMeso::titles) {
        popoverMenus.emplace_back(this, menuTitle.title, menuTitle.get(), [this] (const auto& s) { changeProductByCode(s); });
        boxH.addWidget(popoverMenus.back());
    }
    shortcutAnimate.connect([this] { objectAnimate.animateClicked(); });
    shortcutAutoUpdate.connect([this] { autoUpdate.toggleAutoUpdate(); });
    reload();

    for (auto index : range(UtilitySpcMeso::favList.size())) {
        shortcuts.push_back(Shortcut{QKeySequence{QString::fromStdString(To::string((index + 1) % 10))}, this});
        shortcuts.back().connect([this, index] { changeProductForFav(index); });
    }
}

void SpcMeso::reload() {
    objectAnimate.stopAnimateNoDownload();
    if (savePrefs) {
        Utility::writePref(prefTokenProduct, objectAnimate.product);
        Utility::writePref(prefTokenSector, objectAnimate.sector);
    }
    index = indexOf(UtilitySpcMeso::products, objectAnimate.product);
    setTitle("SPC Mesoanalysis - " + UtilitySpcMeso::labels[index] + " " + autoUpdate.titleAdd);
    new FutureBytes{this, UtilitySpcMesoInputOutput::getImageUrl(objectAnimate.product, objectAnimate.sector), [this] (const auto& ba) { photo.setBytes(ba); }};
}

void SpcMeso::moveBack() {
    index -= 1;
    index = std::max(index, 0);
    objectAnimate.product = UtilitySpcMeso::products[index];
    reload();
}

void SpcMeso::moveForward() {
    index += 1;
    index = std::min(index, static_cast<int>(UtilitySpcMeso::products.size()) - 1);
    objectAnimate.product = UtilitySpcMeso::products[index];
    reload();
}

void SpcMeso::changeProductForFav(int indexB) {
    const auto& b = UtilitySpcMeso::favList[indexB];
    index = findex(b, UtilitySpcMeso::products);
    objectAnimate.product = UtilitySpcMeso::products[index];
    reload();
}

void SpcMeso::changeProductByCode(const string& label) {
    index = findex(label, UtilitySpcMeso::labels);
    objectAnimate.product = UtilitySpcMeso::products[index];
    reload();
}

void SpcMeso::changeSector() {
    objectAnimate.sector = UtilitySpcMeso::sectorCodes[comboboxSector.getIndex()];
    reload();
}

void SpcMeso::closeEventCustom() {
    objectAnimate.stopAnimateNoDownload();
    autoUpdate.stopNoDownload();
}
