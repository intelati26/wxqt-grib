// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "RainfallOutlookSummary.h"
#include "objects/FutureBytes.h"
#include "util/To.h"
#include "util/UtilityList.h"
#include "wpc/RainfallOutlook.h"
#include "wpc/UtilityWpcRainfallOutlook.h"

RainfallOutlookSummary::RainfallOutlookSummary(Window * parent)
    : Window{parent}
    , urls{UtilityWpcRainfallOutlook::urls}
{
    setTitle("Excessive Rainfall Outlook");
    box.addImageRows(this, urls, images, numberAcross, getWindowWidth());
    box.addStretch();
    box.getAndShow(this);
    for (auto index : range(urls.size())) {
        images[index].connect([this, index] { new RainfallOutlook{this, static_cast<int>(index)}; });
        new FutureBytes{this, urls[index], [this, index] (const auto& ba) { images[index].setBytes(ba); }};
        shortcuts.emplace_back(QKeySequence{QString::fromStdString(To::string(index + 1))}, this);
        shortcuts.back().connect([this, index] { new RainfallOutlook{this, static_cast<int>(index)}; });
    }
}

void RainfallOutlookSummary::resizeEventCustom() {
    for (auto& image : images) {
        image.resize(width() / numberAcross - 30);
    }
}
