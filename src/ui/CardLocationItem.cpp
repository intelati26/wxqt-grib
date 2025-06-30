// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "CardLocationItem.h"
#include "settings/Location.h"
#include "util/UtilityTimeSunMoon.h"

CardLocationItem::CardLocationItem(Window * parent, int index)
    : latLon{Location::getLatLon(index)}
    , text1{parent, Location::getName(index) + " (" + latLon.printPretty() + ")"}
    , text2{parent, Location::getWfo(index) + ", " + Location::getRadarSite(index) + " " + UtilityTimeSunMoon::getSunTimes(latLon)}
    , text3{parent, UtilityTimeSunMoon::getMoonTimes(latLon)}
{
    text1.setWordWrap(false);
    text1.setBlue();
    text1.setBold();
    text2.setWordWrap(false);
    text3.setWordWrap(false);

    box.addWidget(text1);
    box.addWidget(text2);
    box.addWidget(text3);
    addLayout(box);
}
