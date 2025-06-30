// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "CardDashAlertItem.h"
#include "misc/AlertsDetail.h"
#include "objects/Route.h"
#include "objects/WString.h"

CardDashAlertItem::CardDashAlertItem(Window * parent, const ObjectWarning& warning)
    : buttonDetails{parent, None, "Details"}
    , buttonRadar{parent, Radar, "Radar"}
    , text1{parent, warning.event + " (" + warning.sender + ")"}
    , text2{parent, warning.sender + " " + WString::replace(warning.title, "\\n", " ")}
    , text3{parent, warning.area}
    , text4{parent, WString::replace(warning.effective, "T", " ")}
    , text5{parent, WString::replace(warning.expires, "T", " ")}
{
    text1.setBlue();
    text3.setGray();

    boxText.addWidget(text1);
    boxText.addWidget(text2);
    boxText.addWidget(text3);
    boxText.addWidget(text4);
    boxText.addWidget(text5);
    boxText.addStretch();

    const auto url = warning.getUrl();
    buttonDetails.connect([parent, url] { new AlertsDetail{parent, url}; });

    const auto radarSite = warning.getClosestRadar();
    buttonRadar.setText("Radar - " + radarSite);
    buttonRadar.connect([parent, radarSite] { Route::nexradRadarSpecificSite(parent, radarSite); });

    // addLayout(boxButtons, Qt::AlignTop);
    addLayout(boxButtons);
    addLayout(boxText, Qt::AlignTop);
    // addLayout(boxText);
    // addStretch();

    boxButtons.addWidget(buttonRadar);
    boxButtons.addWidget(buttonDetails);
    boxButtons.addStretch();
}
