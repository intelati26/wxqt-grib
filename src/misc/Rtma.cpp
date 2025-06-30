// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "Rtma.h"
#include <algorithm>
#include "misc/UtilityRtma.h"
#include "objects/FutureBytes.h"
#include "objects/ObjectDateTime.h"
#include "settings/Location.h"
#include "ui/PhotoSizeEnum.h"
#include "util/Utility.h"

Rtma::Rtma(Window * parent)
    : Window{parent}
    , photo{this, FullWithHeight, [this] { return getPhotoHeight(); }}
    , comboboxProduct{this, UtilityRtma::codes}
    , comboboxSector{this, UtilityRtma::sectors}
    , comboboxTimes{this, UtilityRtma::getTimes()} // distinct
    , backForward{this, [this] { moveBack(); }, [this] { moveForward(); }}
    , index{Utility::readPrefInt(prefToken, 0)}
{
    comboboxProduct.setIndexByValue(UtilityRtma::codes[index]);
    comboboxProduct.connect([this] { changeProduct(); });
    comboboxSector.setIndexByValue(UtilityRtma::getNearest(Location::getLatLonCurrent()));
    comboboxSector.connect([this] { changeSector(); });
    comboboxTimes.setIndex(0);
    comboboxTimes.connect([this] { changeTime(); });
    hbox.addWidget(comboboxProduct);
    hbox.addWidget(comboboxSector);
    hbox.addWidget(comboboxTimes);
    hbox.addLayout(backForward);
    box.addLayout(hbox);
    box.addWidgetAndCenter(photo);
    box.getAndShow(this);
    reload();
}

void Rtma::reload() {
    const auto url = UtilityRtma::getUrl(index, comboboxSector.getIndex(), comboboxTimes.getValue());
    Utility::writePrefInt(prefToken, index);
    const auto utcTime = comboboxTimes.getValue();
    auto objectDateTime = ObjectDateTime::parse(utcTime, "yyyyMMdd HH' UTC'");
    objectDateTime.utcToLocal();
    const auto timeString = objectDateTime.format("yyyyMMdd HH");
    setTitle(timeString + " - " + UtilityRtma::labels[index]);
    new FutureBytes{this, url, [this] (const auto& ba) { photo.setBytes(ba); }};
}

void Rtma::moveBack() {
    index -= 1;
    index = std::max(index, 0);
    comboboxProduct.setIndex(index);
}

void Rtma::moveForward() {
    index += 1;
    index = std::min(index, static_cast<int>(UtilityRtma::labels.size()) - 1);
    comboboxProduct.setIndex(index);
}

void Rtma::changeProduct() {
    index = comboboxProduct.getIndex();
    reload();
}

void Rtma::changeSector() {
    reload();
}

void Rtma::changeTime() {
    reload();
}
