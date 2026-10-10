// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "SpcSwoStateGraphics.h"
#include "common/GlobalArrays.h"
#include "misc/ImageViewer.h"
#include "objects/FutureBytes.h"
#include "objects/WString.h"
#include "settings/Location.h"
#include "spc/SpcSwoDay1.h"
#include "spc/UtilitySpcSwo.h"
#include "util/To.h"
#include "util/UtilityList.h"

SpcSwoStateGraphics::SpcSwoStateGraphics(Window * parent, int day)
    : Window{parent}
    , sw{this, box}
    , comboBox{this, GlobalArrays::states}
    , day{day}
    , state{Location::state()}
{
    setTitle("SPC Convective Outlooks for " + state + " Day  " + To::string(day));
    const auto fullStateIndex = findex(state + ":", GlobalArrays::states);
    comboBox.setIndex(fullStateIndex);
    comboBox.connect([this] { changeState(); });
    box.addWidget(comboBox);
    urls = UtilitySpcSwo::getSwoStateUrl(state, day);
    box.addImageRows(this, urls, images, numberAcross, getWindowWidth());
    for (auto index : range(urls.size())) {
        images[index].connect([this, index] { new ImageViewer{this, images[index].bytes}; });
        shortcuts.push_back(Shortcut{QKeySequence{QString::fromStdString(To::string(index + 1))}, this});
        shortcuts.back().connect([this, index] { new ImageViewer{this, images[index].bytes}; });
    }
    box.addStretch();
    reload();
    ready = true;
}

void SpcSwoStateGraphics::reload() {
    urls = UtilitySpcSwo::getSwoStateUrl(state, day);
    for (const auto index : range(urls.size())) {
        const auto url = urls[index];
        images[index].setNumberAcross(numberAcross, getWindowWidth());
        new FutureBytes{this, url, [this, index] (const auto& ba) { images[index].setBytes(ba); }};
    }
}

void SpcSwoStateGraphics::changeState() {
    state = WString::split(GlobalArrays::states[comboBox.getIndex()], ":")[0];
    reload();
}

void SpcSwoStateGraphics::launchImage(int index) {
    ImageViewer{this, images[index].bytes};
}

void SpcSwoStateGraphics::resizeEventCustom() {
    // showing the window while the pictures are still being added sends a resize at once: images is half built then (and a crash if it is walked)
    if (!ready) {
        return;
    }
    for (size_t index = 0; index < images.size(); index++) {
        images[index].resizeToWidth(static_cast<float>(width()) / numberAcross);
    }
}
