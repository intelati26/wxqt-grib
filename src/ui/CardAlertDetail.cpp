// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "CardAlertDetail.h"
#include "misc/AlertsDetail.h"
#include "objects/Route.h"

CardAlertDetail::CardAlertDetail(Window * parent, const CapAlertXml& cap)
    : buttonDetails{parent, None, "Details"}
    , buttonRadar{parent, Radar, "Radar"}
    , text1{parent, cap.title}
    , text2{parent, cap.area}
    , text3{parent, "Start: " + cap.effective}
    , text4{parent, "End: " + cap.expires}
{
    text1.setBold();
    text1.setWordWrap(false);
    text2.setGray();
    boxText.addWidget(text1);
    boxText.addWidget(text2);
    boxText.addWidget(text3);
    boxText.addWidget(text4);
    boxText.addStretch();

    const auto radarSite = cap.getClosestRadar();
    if (!radarSite.empty()) {
        buttonRadar.setText("Radar - " + radarSite);
        buttonRadar.connect([parent, radarSite] { Route::nexradRadarSpecificSite(parent, radarSite); });
    } else {
        buttonRadar.setVisible(false);
    }
    buttonDetails.connect([parent, cap] { new AlertsDetail{parent, cap.url}; });

    addLayout(layoutVertical, Qt::AlignLeft);
    addLayout(boxText, Qt::AlignTop);
    addStretch();

    layoutVertical.addWidget(buttonRadar);
    layoutVertical.addWidget(buttonDetails);
    layoutVertical.addStretch();
}
