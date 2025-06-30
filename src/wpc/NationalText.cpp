// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "wpc/NationalText.h"
#include <algorithm>
#include "objects/FutureText.h"
#include "objects/WString.h"
#include "util/Utility.h"
#include "util/UtilityList.h"
#include "wpc/UtilityWpcText.h"

NationalText::NationalText(Window * parent, const string& prod)
    : Window{parent}
    , sw{this, box}
    , text{parent}
    , backForward{this, [this] { moveBack(); }, [this] { moveForward(); }}
{
    box.addMargins();
    if (prod.empty()) {
        product = Utility::readPref(prefTokenProduct, "PMDSPD");
        savePref = true;
    } else {
        product = WString::toLower(prod);
        savePref = false;
    }
    hbox.addLayout(backForward);

    auto itemsSoFar = 0;
    for (auto& menu : UtilityWpcText::titles) {
        menu.setList(UtilityWpcText::labels, itemsSoFar);
        itemsSoFar += menu.count;
    }
    for (auto& objectMenuTitle : UtilityWpcText::titles) {
        popoverMenus.emplace_back(this, objectMenuTitle.title, objectMenuTitle.get(), [this] (const auto& s) { changeProductByCode(s); });
        hbox.addWidget(popoverMenus.back());
    }
    box.addLayout(hbox);
    box.addWidget(text);
    box.addStretch();
    reload();
}

void NationalText::reload() {
    if (savePref) {
        Utility::writePref(prefTokenProduct, product);
    }
    index = findex(product, UtilityWpcText::labels);
    setTitle(UtilityWpcText::labels[index]);
    new FutureText{this, product, [this] (const auto& s) { text.setText(s); }};
}

void NationalText::moveBack() {
    index -= 1;
    index = std::max(index, 0);
    product = WString::split(UtilityWpcText::labels[index], ":")[0];
    reload();
}

void NationalText::moveForward() {
    index += 1;
    index = std::min(index, static_cast<int>(UtilityWpcText::labels.size()) - 1);
    product = WString::split(UtilityWpcText::labels[index], ":")[0];
    reload();
}

void NationalText::changeProductByCode(const string& s) {
    product = WString::split(s, ":")[0];
    reload();
}
