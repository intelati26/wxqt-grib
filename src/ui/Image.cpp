// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "Image.h"
#include <QObject>
#include "util/UtilityUI.h"

Image::Image(Window * parent)
    : imageSize{UtilityUI::getImageWidth(3)}
    , parent{parent}
    , image{new ClickableLabel{parent}}
{}

ClickableLabel * Image::getView() {
    return image;
}

void Image::setNumberAcross(int num, int width) {
    if (width != 0) {
        imageSize = UtilityUI::getImageWidth2(num, width);
    } else {
        imageSize = UtilityUI::getImageWidth(num);
    }
}

void Image::setToWidth(const QByteArray& imageData, int width) {
    image->setToWidth(imageData, width);
}

void Image::resize(float width) {
    if (bytes.size() > 0) {  // wxpy diff
        image->setToWidth(bytes, static_cast<int>(width));
    }
}

void Image::resizeToWidth(float width) {
    if (bytes.size() > 0) {
        image->setToWidth(bytes, static_cast<int>(width));
    }
}

void Image::setBytes(const QByteArray& ba) {
    bytes = ba;
    UtilityUI::updateImage(image, ba, imageSize);
}

void Image::connect(const function<void()>& fn) {
    QObject::connect(image, &ClickableLabel::clicked, parent, fn);
}
