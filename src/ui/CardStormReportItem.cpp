// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "CardStormReportItem.h"
#include <QDesktopServices>
#include <QUrl>

CardStormReportItem::CardStormReportItem(Window * parent, const StormReport& stormReport)
    : text1{parent, stormReport.state + ", " + stormReport.city + " " + stormReport.time}
    , text2{parent, stormReport.address}
    , text3{parent, stormReport.magnitude + " - " + stormReport.damageReport}
    , button{parent, None, stormReport.latLon.printPretty()}
{
    text1.setBold();
    text2.setGray();
    text3.setGray();

    box.addWidget(text1);
    box.addWidget(text2);
    box.addWidget(text3);

    button.connect([stormReport] { launchMap(stormReport.lat, stormReport.lon); });

    addWidget(button);
    addLayout(box, Qt::AlignTop);
}

void CardStormReportItem::launchMap(const string& lat, const string& lon) {
    // https://www.openstreetmap.org/?mlat=47.5433&mlon=-52.8734&zoom=12#map=12/47.5433/-52.8734;
    const auto url = QUrl{QString::fromStdString("https://www.openstreetmap.org/?mlat=" + lat + "&mlon=" + lon + "&zoom=12//map=12/" + lat + "/" + lon)};
    QDesktopServices::openUrl(url);
}
