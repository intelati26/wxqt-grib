// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "ImageViewer.h"
#include "objects/FutureBytes.h"

ImageViewer::ImageViewer(Window * parent, const QByteArray& data, string title)
    : Window{parent}
    , photo{this, FullWithHeight, [this] { return getPhotoHeight(); }}
{
    setTitle(title);
    photo.setBytes(data);
    box.addWidgetAndCenter(photo);
    box.getAndShow(this);
}

ImageViewer::ImageViewer(Window * parent, const string& url, string title)
    : Window{parent}
    , photo{this, FullWithHeight, [this] { return getPhotoHeight(); }}
{
    setTitle(title);
    box.addWidgetAndCenter(photo);
    box.getAndShow(this);
    new FutureBytes{this, url, [this] (const auto& ba) { photo.setBytes(ba); }};
}

void ImageViewer::resizeEventCustom() {
    photo.setToHeight(getWindowHeight());
}

