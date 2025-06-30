// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "Photo.h"
#include "settings/UIPreferences.h"
#include "util/UtilityForecastIcon.h"
#include "util/UtilityUI.h"

Photo::Photo(Window * parent)
    : image{new ClickableLabel{parent}}
{}

Photo::Photo(Window * parent, const QPixmap & pix)
    : image{new ClickableLabel{parent}}
{
    image->setPixmap(pix);
}

Photo::Photo(Window * parent, PhotoSizeEnum size)
    : image{new ClickableLabel{parent}}
    , size{size}
    , width{UtilityUI::getImageWidth(3)}
{}

Photo::Photo(Window * parent, PhotoSizeEnum size, std::function<int()> getPhotoHeight)
    : image{new ClickableLabel{parent}}
    , size{size}
    , width{UtilityUI::getImageWidth(3)}
    , getPhotoHeight{getPhotoHeight}
{}

// void Photo::set(const QPixmap & pix) {
//     image->setPixmap(pix);
//     image->adjustSize();
// }

void Photo::setPix(const Pix& pix) {
    getView()->setPixmap(pix.pix);
}

void Photo::setFullScreenWithHeight() {
    // const auto pixmap = QPixmap::fromImage(QImage::fromData(bytes));
    // if (getPhotoHeight) {
    //     const auto pixmapResized = pixmap.scaledToHeight(getPhotoHeight() - 100, Qt::SmoothTransformation);
    //     set(pixmapResized);
    // }

    if (getPhotoHeight) {
        auto pix = Pix::fromBytes(bytes);
        pix.scaleToHeight(getPhotoHeight() - 100);
        setPix(pix);
    }
}

// void Photo::setFullScreen(const QByteArray& ba) {
//     const auto pixmap = QPixmap::fromImage(QImage::fromData(reinterpret_cast<const uchar *>(ba.data()), static_cast<int>(ba.size())));
//     const auto dimensions = UtilityUI::getScreenBounds();
//     if (dimensions[1] > dimensions[0]) {
//         const auto pixmapResized = pixmap.scaledToWidth(UtilityUI::getScreenBounds()[0], Qt::SmoothTransformation);
//         set(pixmapResized);
//     } else {
//         const auto pixmapResized = pixmap.scaledToHeight(UtilityUI::getScreenBounds()[1] - 100, Qt::SmoothTransformation);
//         set(pixmapResized);
//     }
// }

// void Photo::setSizeByHeight(float height) {
//     const auto pixmapResized = pixmap.scaledToHeight(static_cast<int>(height), Qt::SmoothTransformation);
//     set(pixmapResized);
// }

// void Photo::setSizeByWidth(float width) {
//     const auto pixmap = QPixmap::fromImage(QImage::fromData(bytes));
//     const auto pixmapResized = pixmap.scaledToWidth(static_cast<int>(width), Qt::SmoothTransformation);
//     set(pixmapResized);
// }

void Photo::resize(float width) {  // FIXME TODO remove in wxpy as well
    // pixmapResized: QPixmap = self.pixmap.scaledToHeight(int(height) - 100, Qt.TransformationMode.SmoothTransformation)
    // self.setPix(pixmapResized)
    image->setToWidth(bytes, static_cast<int>(width));
}

void Photo::setNoScale() {
    // const auto pixmap = QPixmap::fromImage(QImage::fromData(ba));
    // set(pixmap);

    auto pix = Pix::fromBytes(bytes);
    setPix(pix);
    adjustSize();
}

void Photo::setToWidth(int width) {
    // const auto pixmap = QPixmap::fromImage(QImage::fromData(ba));
    // const auto pixmapResized = pixmap.scaled(width, width, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    // set(pixmapResized);

    auto pix = Pix::fromBytes(bytes);
    pix.scaleToWidth(static_cast<int>(width) - 100);
    setPix(pix);
}

// void Photo::setToWidthEven(const QByteArray& ba) {
//     UtilityUI::updateImage(getView(), ba, width);
// }

void Photo::setToHeight(float height) {
    // const auto pixmap = QPixmap::fromImage(QImage::fromData(bytes));
    // const auto pixmapResized = pixmap.scaledToHeight(static_cast<int>(height) - 100, Qt::SmoothTransformation);
    // set(pixmapResized);

    auto pix = Pix::fromBytes(bytes);
    pix.scaleToHeight(static_cast<int>(height) - 100);
    setPix(pix);
}

void Photo::adjustSize() {
    getView()->adjustSize();
}

// void Photo::setAlignment(Qt::Alignment alignment) {
//     image->setAlignment(alignment);
// }

void Photo::setBytes(const QByteArray& ba) {
    bytes = ba;
    if (size == FullWithHeight) {
        setFullScreenWithHeight();
    } else if (size == Scaled) {
        setToWidth(width);
    } else if (size == ScaledToHeight) {
        setToHeight(height);
    } else {
        setNoScale();
    }
}

void Photo::setNwsIcon(const string& url) {
    // const auto width = UIPreferences::imageSizeNwsForecast;
    // const auto pixmap = QPixmap::fromImage(UtilityForecastIcon::getIcon(url));
    // const auto pixmapResized = pixmap.scaled(width, width, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    // getView()->setPixmap(pixmapResized);

    auto pix = UtilityForecastIcon::getIcon(url);
    pix.scale(UIPreferences::nwsIconSize, UIPreferences::nwsIconSize);
    setPix(pix);
}

void Photo::connect(const function<void()>& fn) {
    image->connect(fn);
}

QLabel * Photo::getView() {
    return image;
}
