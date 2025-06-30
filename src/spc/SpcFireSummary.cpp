// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "SpcFireSummary.h"
#include "objects/FutureBytes.h"
#include "spc/SpcFireWeatherOutlook.h"
#include "spc/UtilitySpcFireOutlook.h"
#include "util/To.h"
#include "util/UtilityList.h"

SpcFireSummary::SpcFireSummary(Window * parent)
    : Window{parent}
    , urls{UtilitySpcFireOutlook::urls}
{
    setTitle("SPC Fire Weather Outlooks");
    box.addImageRow(this, urls, images, getWindowWidth());
    box.getAndShow(this);
    for (auto index : range(urls.size())) {
        images[index].connect([this, index] { new SpcFireWeatherOutlook{this, static_cast<int>(index)}; });
        new FutureBytes{this, urls[index], [this, index] (const auto& ba) { images[index].setBytes(ba); }};
    }
    for (auto index : range(urls.size())) {
        shortcuts.emplace_back(QKeySequence{QString::fromStdString(To::string(index + 1))}, this);
        shortcuts.back().connect([this, index] { new SpcFireWeatherOutlook{this, static_cast<int>(index)}; });
    }
}
