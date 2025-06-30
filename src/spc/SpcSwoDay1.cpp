// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "SpcSwoDay1.h"
#include "misc/ImageViewer.h"
#include "objects/FutureBytes.h"
#include "objects/FutureText.h"
#include "spc/SpcSwoStateGraphics.h"
#include "spc/UtilitySpcSwo.h"
#include "util/To.h"
#include "util/UtilityList.h"

SpcSwoDay1::SpcSwoDay1(Window * parent, int day)
    : Window{parent}
    , sw{this, imageVBox, boxText}
    , text{this}
    , button{this, Icon::None, "State Graphics"}
    , launchStateShortcut{QKeySequence{"S"}, this}
{
    setTitle("SPC Convective Outlook Day " + To::string(day));
    button.connect([this, day] { new SpcSwoStateGraphics{this, day}; });
    boxText.addWidget(button);
    boxText.addWidget(text);
    boxText.addStretch();
    if (day > 3) {
        urls = UtilitySpcSwo::getImageUrls(48);
    } else {
        urls = UtilitySpcSwo::getImageUrls(day);
    }
    for ([[maybe_unused]] auto index : range(urls.size())) {
        images.emplace_back(this);
        images.back().imageSize = 350;
        imageVBox.addWidget(images.back());
    }
    imageVBox.addStretch();

    auto product = "SWODY" + To::string(day);
    if (day > 3) {
        product = "SWOD48";
    }
    new FutureText{this, product, [this] (const auto& s) { text.setText(s); }};
    for (auto index : range(urls.size())) {
        images[index].connect([this, parent, index] { new ImageViewer{parent, images[index].bytes}; });
        new FutureBytes{this, urls[index], [this, index] (const auto& ba) { images[index].setBytes(ba); }};

        shortcuts.push_back(Shortcut{QKeySequence{QString::fromStdString(To::string(index + 1))}, this});
        shortcuts.back().connect([this, parent, index] { new ImageViewer{parent, images[index].bytes}; });
    }

    launchStateShortcut.connect([this, day] { new SpcSwoStateGraphics(this, day); });
}
