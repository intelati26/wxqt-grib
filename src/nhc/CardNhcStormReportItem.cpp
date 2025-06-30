// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "CardNhcStormReportItem.h"
#include "misc/ImageViewer.h"
#include "nhc/NhcStorm.h"
#include "objects/WString.h"
#include "util/To.h"
#include "util/UtilityMath.h"

CardNhcStormReportItem::CardNhcStormReportItem(Window * parent, const NhcStormDetails& stormData)
    : stormData{stormData}
    , button{parent, None, "Show Details - " + stormData.name}
    , image{parent}
    , text1{parent, stormData.name + " (" + stormData.classification + ") " + stormData.center}
    , text2{parent, "Moving " + UtilityMath::bearingToDirection(To::Int(stormData.movementDir)) + " at " + stormData.movementSpeed + " mph"}
    , text3{parent, "Min pressure: " + stormData.pressure + " mb"}
    , text4{parent, "Max sustained: " + UtilityMath::knotsToMph(stormData.intensity) + " mph"}
    , text5{parent, stormData.status + " " + stormData.binNumber + " " + WString::toUpper(stormData.stormId)}
{
    button.connect([stormData, parent] { new NhcStorm{parent, stormData}; });

    image.imageSize = 250;
    image.connect([stormData, parent] { new ImageViewer{parent, stormData.coneBytes}; });
    image.setBytes(stormData.coneBytes);

    text1.setBold();
    text1.setBlue();

    textLayout.addWidget(button);
    textLayout.addWidget(text1);
    textLayout.addWidget(text2);
    textLayout.addWidget(text3);
    textLayout.addWidget(text4);
    textLayout.addWidget(text5);
    textLayout.addStretch();

    addWidget(image);
    addLayout(textLayout, 1);
}
