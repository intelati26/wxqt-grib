// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "spc/SpcFireWeatherOutlook.h"
#include "misc/ImageViewer.h"
#include "objects/FutureBytes.h"
#include "objects/FutureText.h"
#include "spc/UtilitySpcFireOutlook.h"
#include "util/To.h"

SpcFireWeatherOutlook::SpcFireWeatherOutlook(Window * parent, int day)
    : Window{parent}
    , photo{this, Scaled}
    , text{this}
    , sw{this, photo, text}
    , shortcutImage{QKeySequence{"1"}, this}
{
    setTitle("SPC Fire Weather Outlook Day " + To::string(day + 1));
    const auto& url = UtilitySpcFireOutlook::urls[day];
    const auto& product = UtilitySpcFireOutlook::products[day];
    photo.connect([this, parent] { new ImageViewer{parent, photo.bytes}; });
    shortcutImage.connect([this, parent] { new ImageViewer{parent, photo.bytes}; });
    new FutureBytes{this, url, [this] (const auto& ba) { photo.setBytes(ba); }};
    new FutureText{this, product, [this] (const auto& s) { text.setText(s); }};
}
