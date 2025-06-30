// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "NationalImages.h"
#include <algorithm>
#include "common/GlobalVariables.h"
#include "objects/FutureBytes.h"
#include "objects/WString.h"
#include "util/Utility.h"
#include "util/UtilityList.h"
#include "wpc/UtilityWpcImages.h"

NationalImages::NationalImages(Window * parent)
    : Window{parent}
    , photo{this, FullWithHeight, [this] { return getPhotoHeight(); }}
    , backForward{this, [this] { moveBack(); }, [this] { moveForward(); }}
    , index{Utility::readPrefInt(prefToken, 0)}
{
    hbox.addLayout(backForward);
    box.addLayout(hbox);
    box.addWidgetAndCenter(photo);
    box.getAndShow(this);

    auto itemsSoFar = 0;
    for (auto& menu : UtilityWpcImages::titles) {
        menu.setList(UtilityWpcImages::labels, itemsSoFar);
        itemsSoFar += menu.count;
    }
    for (auto& objectMenuTitle : UtilityWpcImages::titles) {
        popoverMenus.emplace_back(this, objectMenuTitle.title, objectMenuTitle.get(), [this] (const auto& s) { changeProductByCode(s); });
        hbox.addWidget(popoverMenus.back());
    }
    reload();
}

void NationalImages::reload() {
    Utility::writePrefInt(prefToken, index);
    auto url = UtilityWpcImages::urls[index];
    if (WString::contains(url, GlobalVariables::nwsGraphicalWebsitePrefix + "/images/conus/")) {
        url += "1_conus.png";
    }
    setTitle(UtilityWpcImages::labels[index]);
    new FutureBytes{this, url, [this] (const auto& ba) { photo.setBytes(ba); }};
}

void NationalImages::moveBack() {
    index -= 1;
    index = std::max(index, 0);
    reload();
}

void NationalImages::moveForward() {
    index += 1;
    index = std::min(index, static_cast<int>(UtilityWpcImages::urls.size()) - 1);
    reload();
}

void NationalImages::changeProductByCode(const string& s) {
    index = findex(s, UtilityWpcImages::labels);
    reload();
}

void NationalImages::resizeEventCustom() {
    photo.setToHeight(getWindowHeight());
}
