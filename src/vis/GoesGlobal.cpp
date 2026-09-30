// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "GoesGlobal.h"
#include <algorithm>
#include "objects/FutureBytes.h"
#include "objects/WString.h"
#include "settings/Location.h"
#include "settings/UIPreferences.h"
#include "util/Utility.h"
#include "util/UtilityList.h"
#include "UtilityGoesFullDisk.h"

GoesGlobal::GoesGlobal(Window * parent)
    : Window{parent}
    , image{this}
    , comboboxProduct{this, UtilityGoesFullDisk::labels }
    , objectAnimate{this, &image, &UtilityGoesFullDisk::getAnimation}
    , backForward{this, [this] { moveBack(); }, [this] { moveForward(); }}
{
    index = Utility::readPrefInt(prefToken, 0);
    comboboxProduct.setIndexByValue(UtilityGoesFullDisk::labels[index]);
    comboboxProduct.connect([this] { changeProduct(); });

    boxH.addWidget(comboboxProduct);
    boxH.addLayout(backForward);
    box.addLayout(boxH);
    objectAnimate.addTo(box);
    box.addWidgetReal(&image, 1, Qt::Alignment{});
    box.getAndShow(this);
    reload();

}

void GoesGlobal::reload() {
    setTitle("GOES Global " + UtilityGoesFullDisk::labels[index]);
    objectAnimate.product = UtilityGoesFullDisk::urls[index];
    if (UtilityGoesFullDisk::canAnimate(objectAnimate.product)) {
        objectAnimate.refresh();
    } else {
        objectAnimate.clear();
    }
    Utility::writePrefInt(prefToken, index);
    new FutureBytes{this, objectAnimate.product, [this] (const auto& ba) {
        image.setBytesKeepView(ba);
        objectAnimate.setCurrentBytes(ba);
    }};
}

void GoesGlobal::moveBack() {
    index -= 1;
    index = std::max(index, 0);
    comboboxProduct.setIndex(index);
}

void GoesGlobal::moveForward() {
    index += 1;
    index = std::min(index, static_cast<int>(UtilityGoesFullDisk::labels.size()) - 1);
    comboboxProduct.setIndex(index);
}

void GoesGlobal::changeProduct() {
    index = comboboxProduct.getIndex();
    reload();
}

void GoesGlobal::closeEventCustom() {
    objectAnimate.stopAnimateNoDownload();
}
