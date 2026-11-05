// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "SpcTstormOutlooks.h"
#include "misc/ImageViewer.h"
#include "objects/FutureBytes.h"
#include "spc/UtilitySpc.h"
#include "util/UtilityList.h"

SpcTstormOutlooks::SpcTstormOutlooks(Window * parent)
    : Window{parent}
{
    setTitle("SPC Thunderstorm Outlooks");
    const auto urls = UtilitySpc::getTstormOutlookUrls();
    box.addImageRow(this, urls, images, getWindowWidth());
    box.getAndShow(this);
    for (auto index : range(urls.size())) {
        images[index].connect([this, index] { new ImageViewer{this, images[index].bytes}; });
        new FutureBytes{this, urls[index], [this, index] (const auto& ba) { images[index].setBytes(ba); }};
    }
}

void SpcTstormOutlooks::resizeEventCustom() {
    for (auto& image : images) {
        image.resizeToWidth(width() / images.size() - 10);
    }
}
