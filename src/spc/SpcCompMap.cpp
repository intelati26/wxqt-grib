// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "SpcCompMap.h"
#include <algorithm>
#include "objects/FutureBytes.h"
#include "spc/UtilitySpcCompmap.h"
#include "util/Utility.h"
#include "util/UtilityList.h"

SpcCompMap::SpcCompMap(Window * parent)
    : Window{parent}
    , photo{this, FullWithHeight, [this] { return getPhotoHeight(); }}
    , comboBox{this, UtilitySpcCompmap::labels}
    , backForward{this, [this] { moveBack(); }, [this] { moveForward(); }}
    , product{Utility::readPref(prefToken, "0")}
    , index{indexOf(UtilitySpcCompmap::urlIndices, product)}
{
    comboBox.setIndex(index);
    comboBox.connect([this] { changeProduct(); });
    boxH.addWidget(comboBox);
    boxH.addLayout(backForward);
    box.addLayout(boxH);
    box.addWidgetAndCenter(photo);
    box.getAndShow(this);
    reload();
}

void SpcCompMap::reload() {
    Utility::writePref(prefToken, product);
    const auto url = UtilitySpcCompmap::getImage(product);
    setTitle("SPC Compmap - " + comboBox.getValue());
    new FutureBytes{this, url, [this] (const auto& ba) { photo.setBytes(ba); }};
}

void SpcCompMap::moveBack() {
    index -= 1;
    index = std::max(index, 0);
    comboBox.setIndex(index);
}

void SpcCompMap::moveForward() {
    index += 1;
    index = std::min(index, static_cast<int>(UtilitySpcCompmap::urlIndices.size()) - 1);
    comboBox.setIndex(index);
}

void SpcCompMap::changeProduct() {
    index = comboBox.getIndex();
    product = UtilitySpcCompmap::urlIndices[index];
    reload();
}
