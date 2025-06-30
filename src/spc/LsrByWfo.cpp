// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "LsrByWfo.h"
#include "objects/FutureVoid.h"
#include "objects/WString.h"
#include "settings/Location.h"
#include "ui/DividerLine.h"
#include "util/DownloadText.h"
#include "util/UtilityIO.h"
#include "util/UtilityList.h"
#include "util/UtilityString.h"
#include "util/To.h"
#include "util/WfoSites.h"

LsrByWfo::LsrByWfo(Window * parent)
    : Window{parent}
    , sw{this, box}
    , comboboxSector{this, WfoSites::sites->nameList}
    , wfo{Location::wfo()}
{
    setTitle("Local Storm Reports by Office");
    comboboxSector.setIndexByValue(wfo);
    comboboxSector.connect([this] { changeSector(); });

    boxH.addWidget(comboboxSector);
    box.addLayout(boxH);
    box.addLayout(boxText);
    box.addMargins();
    reload();
}

void LsrByWfo::changeSector() {
    wfo = WString::split(comboboxSector.getValue(), ":")[0];
    reload();
}

void LsrByWfo::getLsrFromWfo() {
    lsrList.clear();
    const auto url = "https://forecast.weather.gov/product.php?site=" + wfo + "&issuedby=" + wfo + "&product=LSR&format=txt&version=1&glossary=0";
    const auto html = UtilityIO::getHtml(url);
    const auto numberLSR = UtilityString::parseMultiLineLastMatch(html, "product=LSR&format=TXT&version=(.*?)&glossary");
    if (numberLSR.empty()) {
        lsrList.emplace_back("None issued by this office recently.");
    } else {
        auto maxVers = To::Int(numberLSR);
        if (maxVers > 30) {
            maxVers = 30;
        }
        for (auto version : range3(1, maxVers, 2)) {
            lsrList.push_back(DownloadText::getTextProductWithVersion("LSR" + wfo, version));
        }
    }
}

void LsrByWfo::reload() {
    new FutureVoid{this, [this] { getLsrFromWfo(); }, [this] { update(); }};
}

void LsrByWfo::update() {
    boxText.removeChildren();
    textList.clear();
    auto d{DividerLine{this}};
    for (const auto& lsr : lsrList) {
        textList.emplace_back(this);
        textList.back().setFixedWidth();
        textList.back().setText(lsr);
        boxText.addWidget(textList.back());
        boxText.addWidget(d);
    }
}
