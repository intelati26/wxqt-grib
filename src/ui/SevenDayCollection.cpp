// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "SevenDayCollection.h"
#include "objects/WString.h"

SevenDayCollection::SevenDayCollection(Window * parent, VBox * box, SevenDay * sevenDay)
    : sevenDay{sevenDay}
    , box{box}
    , parent{parent}
{}

void SevenDayCollection::update() {
    box->removeChildren();
    auto iconIndex = 0;
    for (const auto& forecast : sevenDay->detailedForecasts) {
        if (WString::contains(forecast, ":")) {
            const auto items = WString::split(forecast, ":");
            const auto day = items[0];
            const auto longForecast = items[1];
            cards.emplace_back(parent, day, WString::strip(longForecast), sevenDay->icons[iconIndex]);
            box->addLayout(cards.back());
        }
        iconIndex += 1;
    }
    box->addStretch();
}
