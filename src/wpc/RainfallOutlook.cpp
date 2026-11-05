// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "RainfallOutlook.h"
#include "misc/ImageViewer.h"
#include "objects/FutureBytes.h"
#include "objects/FutureText.h"
#include "util/To.h"
#include "wpc/UtilityWpcRainfallOutlook.h"

RainfallOutlook::RainfallOutlook(Window * parent, int day)
    : Window{parent}
    , sw{this, vbox0, vbox1}
    , photo{this, Scaled}
    , text{this}
    , shortcutImage{QKeySequence{"1"}, this}
{
    setTitle("Excessive Rainfall Outlook Day " + To::string(day + 1));
    const auto& url = UtilityWpcRainfallOutlook::urls[day];
    const auto& product = UtilityWpcRainfallOutlook::codes[day];
    vbox0.addWidget(photo);
    vbox1.addWidget(text);
    photo.connect([this] { new ImageViewer{this, photo.bytes}; });
    shortcutImage.connect([this] { new ImageViewer{this, photo.bytes}; });
    new FutureBytes{this, url, [this] (const auto& ba) { photo.setBytes(ba); }};
    new FutureText{this, product, [this] (const auto& s) { text.setText(s); }};
}
