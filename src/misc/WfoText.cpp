// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "WfoText.h"
#include "common/GlobalArrays.h"
#include "misc/UtilityWfoText.h"
#include "objects/FutureText.h"
#include "objects/WString.h"
#include "settings/Location.h"
#include "util/UtilityList.h"
#include "util/UtilityUI.h"
#include "util/WfoSites.h"

WfoText::WfoText(Window * parent, const string& site)
    : Window{parent}
    , sw{this, box}
    , comboboxSector{this, WfoSites::sites->nameList}
    , comboboxProduct{this, UtilityWfoText::wfoProdList}
    , sector{Location::wfo()}
{
    if (site != "") {
        sector = site;
    }

    comboboxSector.setIndexByValue(sector);
    comboboxSector.connect([this] { changeSector(); });

    comboboxProduct.setIndexByValue(product);
    comboboxProduct.connect([this] { changeProduct(); });

    boxH.addWidget(comboboxProduct);
    boxH.addWidget(comboboxSector);
    box.addMargins();
    box.addLayout(boxH);
    box.addLayout(boxText);

    for ([[maybe_unused]] auto unused : range(productCount)) {
        textList.emplace_back(this);
        textList.back().setFixedWidth();
        boxText.addWidget(textList.back());
    }
    reload();
}

void WfoText::changeProduct() {
    product = WString::split(comboboxProduct.getValue(), ":")[0];
    reload();
}

void WfoText::changeSector() {
    sector = WString::split(comboboxSector.getValue(), ":")[0];
    reload();
}

void WfoText::reload() {
    setTitle(sector + " - " + product);
    const vector<string> products{product, "HWO", "LSR"};
    for (auto i : range(1)) {
        new FutureText{this, products[i] + sector, [this, i] (const auto& s) { textList[i].setText(s); }};
    }
}
