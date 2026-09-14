// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "ui/Pix.h"
#include <QImage>
#include "common/GlobalVariables.h"
#include "util/UtilityUI.h"

Pix::Pix(const string& fileName)
    : pix{QPixmap::fromImage(QImage{QString::fromStdString(GlobalVariables::imageDir + fileName)})}
{}

Pix Pix::fromBytes(const QByteArray& imageData) {
    auto pix = QPixmap::fromImage(QImage::fromData(imageData));
    auto pixObject = Pix("");
    pixObject.pix = pix;
    return pixObject;
}

Pix Pix::fromImage(const QImage& image) {
    auto pix = QPixmap::fromImage(image);
    auto pixObject = Pix("");
    pixObject.pix = pix;
    return pixObject;
}

Pix Pix::blank(int width) {
    auto pixObject = Pix("");
    pixObject.pix = QPixmap{width, width};
    pixObject.pix.fill(Qt::transparent);
    return pixObject;
}

void Pix::scale(int width, int height) {
    pix = pix.scaled(width, height, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

void Pix::scaleToHeight(int height) {
    pix = pix.scaledToHeight(height, Qt::SmoothTransformation);
}

void Pix::scaleToWidth(int width) {
    pix = pix.scaledToWidth(width, Qt::SmoothTransformation);
}

vector<int> Pix::getScreenBounds() {
    return UtilityUI::getScreenBounds();
}

int Pix::getImageWidth(int numberOfImages) {
    auto dim = UtilityUI::getScreenBounds();
    return static_cast<int>(dim[0] / static_cast<double>(numberOfImages)) - 10;
}

QPixmap Pix::get() {
    return pix;
}
