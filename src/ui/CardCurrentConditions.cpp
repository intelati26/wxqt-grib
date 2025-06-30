// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "CardCurrentConditions.h"
#include "settings/Location.h"
#include "util/UtilityTimeSunMoon.h"

CardCurrentConditions::CardCurrentConditions(Window * parent, const CurrentConditions& cc)
    : text1{parent}
    , text2{parent}
    , text3{parent}
    , text4{parent, UtilityTimeSunMoon::getSunTimes(Location::getLatLonCurrent())}
    , text5{parent, UtilityTimeSunMoon::getMoonTimes(Location::getLatLonCurrent())}
    , photo{parent}
{
    boxImage.addMargins();

    text1.setBold();
    text2.setGray();
    text3.setGray();
    text4.setGray();
    text5.setGray();
    text2.setWordWrap(false);
    text3.setWordWrap(false);

    boxImage.addWidget(photo);
    boxText.addWidget(text1);
    boxText.addWidget(text2);
    boxText.addWidget(text3);
    boxText.addWidget(text4);
    boxText.addWidget(text5);

    addLayout(boxImage);
    addLayout(boxText);

    update(cc);
}

void CardCurrentConditions::update(const CurrentConditions& cc) {
    text1.setText(cc.topLine);
    text2.setText(cc.middleLine);
    text3.setText(cc.bottomLine);
    photo.setNwsIcon(cc.iconUrl);
}
