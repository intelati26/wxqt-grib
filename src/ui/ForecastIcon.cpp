// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "ForecastIcon.h"
#include "common/GlobalVariables.h"

const int ForecastIcon::dimensions{86};
const int ForecastIcon::numHeight{20};
const int ForecastIcon::halfWidth{42};
const QFont ForecastIcon::font{"Helvetica", 0, QFont::Bold};
const QPen ForecastIcon::penForText{QColor{38, 97, 139}};
const QBrush ForecastIcon::brushForBar{QColor{255, 255, 255, 200}, Qt::SolidPattern};

ForecastIcon::ForecastIcon(const string& weatherCondition) {
    image = QImage{QString::fromStdString(GlobalVariables::imageDir + weatherCondition + ".png")};
    painter.begin(&image);
}

ForecastIcon::ForecastIcon(const string& leftWeatherCondition, const string& rightWeatherCondition) {
    image = QImage{QString::fromStdString(GlobalVariables::imageDir + leftWeatherCondition + ".png")};
    QImage image2{QString::fromStdString(GlobalVariables::imageDir + rightWeatherCondition + ".png")};
    painter.begin(&image);
    painter.drawImage(43, 0, image2);
}

void ForecastIcon::drawLeftText(const string& number) {
    if (!number.empty() && number != "0") {
        painter.setPen(Qt::NoPen);
        painter.setBrush(brushForBar);
        painter.drawRect(QRect{0, dimensions - numHeight, dimensions / 2, numHeight});
        painter.setFont(font);
        painter.setPen(penForText);
        painter.drawText(image.rect(), Qt::AlignBottom | Qt::AlignLeft, QString::fromStdString(number) + "%");
    }
}

void ForecastIcon::drawRightText(const string& number) {
    if (!number.empty() && number != "0") {
        painter.setPen(Qt::NoPen);
        painter.setBrush(brushForBar);
        painter.drawRect(QRect{dimensions / 2, dimensions - numHeight, dimensions / 2, numHeight});
        painter.setFont(font);
        painter.setPen(penForText);
        painter.drawText(image.rect(), Qt::AlignBottom | Qt::AlignRight, QString::fromStdString(number) + "%");
    }
}

void ForecastIcon::drawVerticalBar() {
    painter.setPen(QPen{Qt::white});
    painter.setBrush(QBrush{Qt::white, Qt::SolidPattern});
    painter.drawRect(QRect{halfWidth, 0, 2, dimensions});
}

void ForecastIcon::drawSingleText(const string& number) {
    painter.setPen(Qt::NoPen);
    painter.setBrush(brushForBar);
    painter.drawRect(QRect{0, dimensions - numHeight, dimensions, numHeight});
    painter.setPen(penForText);
    painter.setFont(font);
    painter.drawText(image.rect(), Qt::AlignBottom | Qt::AlignRight, QString::fromStdString(number) + "%");
}

Pix ForecastIcon::get() {
    return Pix::fromImage(image);
}

Pix ForecastIcon::blankBitmap() {
    // return QPixmap{dimensions, dimensions}.toImage();
    return Pix::blank(dimensions);
}
