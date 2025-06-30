// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "SpcSoundings.h"
#include <algorithm>
#include "misc/TextViewer.h"
#include "misc/UtilityObservations.h"
#include "objects/FutureBytes.h"
#include "settings/Location.h"
#include "spc/UtilitySpcSoundings.h"
#include "util/SoundingSites.h"
#include "util/Utility.h"
#include "util/UtilityList.h"

SpcSoundings::SpcSoundings(Window * parent, const string& sector)
    : Window{parent}
    , office{SoundingSites::sites->getNearest(Location::getLatLonCurrent())}
    , index{findex(office, SoundingSites::sites->codeList)}
    , photo{this, FullWithHeight, [this] { return getPhotoHeight(); }}
    , backForward{this, [this] { moveBack(); }, [this] { moveForward(); }}
    , buttonForText{this, None, "Text Display"}
    , comboboxProduct{this, SoundingSites::sites->nameList}
{
    if (sector != "") {
        office = sector;
    }
    index = findex(office, SoundingSites::sites->codeList);
    buttonForText.connect([this] { launchText(); });
    boxH.addLayout(backForward);
    boxH.addWidget(buttonForText);
    comboboxProduct.setIndex(index);
    comboboxProduct.connect([this] { changeProduct(); });

    boxH.addWidget(comboboxProduct);
    box.addLayout(boxH);
    box.addWidgetAndCenter(photo);
    box.getAndShow(this);

    reload();
}

void SpcSoundings::moveBack() {
    index -= 1;
    index = std::max(index, 0);
    office = SoundingSites::sites->codeList[index];
    comboboxProduct.setIndex(index);
    reload();
}

void SpcSoundings::moveForward() {
    index += 1;
    index = std::min(index, static_cast<int>(SoundingSites::sites->nameList.size() - 1));
    office = SoundingSites::sites->codeList[index];
    comboboxProduct.setIndex(index);
    reload();
}

void SpcSoundings::reload() {
    office = SoundingSites::sites->codeList[index];
    setTitle("Spc Soundings: " + SoundingSites::sites->byCode[office]->fullName + " " + SoundingSites::sites->byCode[office]->latLon.printPretty());
    new FutureBytes{this, UtilitySpcSoundings::getImage(office), [this] (const auto& ba) { photo.setBytes(ba); }};
}

void SpcSoundings::changeProduct() {
    index = comboboxProduct.getIndex();
    office = SoundingSites::sites->codeList[index];
    reload();
}

void SpcSoundings::launchText() {
    new TextViewer{this, "https://www.spc.noaa.gov/exper/soundings/LATEST/" + office + ".txt", true, "Sounding " + office};
}
