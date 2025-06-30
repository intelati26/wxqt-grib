// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "misc/Observations.h"
#include <algorithm>
#include "misc/UtilityObservations.h"
#include "objects/FutureBytes.h"
#include "ui/PhotoSizeEnum.h"
#include "util/Utility.h"

Observations::Observations(Window * parent)
    : Window{parent}
    , photo{this, FullWithHeight, [this] { return getPhotoHeight(); }}
    , comboBox{this, UtilityObservations::labels}
    , backForward{this, [this] { moveBack(); }, [this] { moveForward(); }}
{
    comboBox.setIndexByPref(prefToken, 0);
    comboBox.connect([this] { reload(); });
    boxH.addWidget(comboBox);
    boxH.addLayout(backForward);
    box.addLayout(boxH);
    box.addWidgetAndCenter(photo);
    box.getAndShow(this);
    reload();
}

void Observations::reload() {
    const auto index = comboBox.getIndex();
    setTitle("Observations - " + UtilityObservations::labels[index]);
    const auto& url = UtilityObservations::urls[index];
    Utility::writePrefInt(prefToken, index);
    new FutureBytes{this, url, [this] (const auto& ba) { photo.setBytes(ba); }};
}

void Observations::moveBack() {
    auto index = comboBox.getIndex();
    index -= 1;
    index = std::max(index, 0);
    comboBox.setIndex(index);
}

void Observations::moveForward() {
    auto index = comboBox.getIndex();
    index += 1;
    index = std::min(index, static_cast<int>(UtilityObservations::labels.size()) - 1);
    comboBox.setIndex(index);
}
