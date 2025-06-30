// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "SpcSwoSummary.h"
#include "objects/FutureBytes.h"
#include "spc/SpcSwoDay1.h"
#include "spc/UtilitySpcSwo.h"
#include "util/To.h"
#include "util/UtilityList.h"

SpcSwoSummary::SpcSwoSummary(Window * parent)
    : Window{parent}
    , sw{this, box}
{
    setTitle("SPC Convective Outlooks");
    const auto numberAcross = 4;
    for (const auto& day : day1to3List) {
        urls.push_back(UtilitySpcSwo::getImageUrls(day)[0]);
    }
    for (const auto& day : day4To8List) {
        urls.push_back(UtilitySpcSwo::getImageUrlsDays48(day));
    }
    box.addImageRows(this, urls, images, numberAcross);
    box.addStretch();
    for (auto index : range(urls.size())) {
        images[index].connect([this, index] { new SpcSwoDay1{this, static_cast<int>(index) + 1}; });
        images[index].setNumberAcross(numberAcross, getWindowWidth());
        new FutureBytes{this, urls[index], [this, index] (const auto& ba) { images[index].setBytes(ba); }};
    }
    for (auto index : range(urls.size() + 1)) {
        shortcuts.emplace_back(QKeySequence{QString::fromStdString(To::string(index))}, this);
        shortcuts.back().connect([this, index] { new SpcSwoDay1{this, static_cast<int>(index)}; });
    }
}
