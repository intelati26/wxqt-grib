// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "NhcStorm.h"
#include "misc/ImageViewer.h"
#include "objects/FutureBytes.h"
#include "objects/FutureText.h"
#include "objects/ObjectDateTime.h"
#include "objects/WString.h"
#include "util/To.h"
#include "util/UtilityList.h"
#include "vis/GoesViewer.h"

NhcStorm::NhcStorm(Window * parent, const NhcStormDetails& stormData)
    : Window{parent}
    , parent{parent}
    , stormData{stormData}
    , sw{this, boxImages, boxText}
    , comboboxProduct{this, stormTextProducts}
    , goesButton{this, None, "GOES"}
    , text{this}
    , goesUrl{stormData.goesUrl}
    , product{"MIATCP" + stormData.binNumber}
    , shortcut{QKeySequence{"C"}, this}
{
    setTitle("NHC Storm " + stormData.forTopHeader());
    goesButton.connect([this] { launchGoes(); });
    textProductUrl = stormData.advisoryNumber;
    if (WString::startsWith(textProductUrl, "HFO")) {
        office = "HFO";
        textProducts = {
            "HFOTCP: Public Advisory",
            "HFOTCM: Forecast Advisory",
            "HFOTCD: Forecast Discussion",
            "HFOPWS: Wind Speed Probababilities"
        };
    } else {
        office = "MIA";
        textProducts = {
            "MIATCP: Public Advisory",
            "MIATCM: Forecast Advisory",
            "MIATCD: Forecast Discussion",
            "MIAPWS: Wind Speed Probabilities"};
    }

    comboboxProduct.setList(textProducts);
    comboboxProduct.setIndex(0);
    comboboxProduct.connect([this] { changeProduct(); });
    boxText.addWidget(goesButton);
    boxText.addWidget(comboboxProduct);
    boxText.addWidget(text);
    boxText.addStretch();

    for ([[maybe_unused]] const auto& unused : urls) {
        images.emplace_back(this);
        images.back().imageSize = 250;
        boxImages.addWidget(images.back());
    }
    for (auto index : range(urls.size())) {
        auto url = stormData.baseUrl;
        if (urls[index] == "WPCQPF_sm2.gif" || urls[index] == "WPCERO_sm2.gif") {
            url = WString::replace(url, ObjectDateTime::getYearString(), ObjectDateTime::getYearShortString());
        }
        images[index].connect([this, index] { new ImageViewer{this, images[index].bytes}; });
        new FutureBytes{this, url + urls[index], [this, index] (const auto& ba) { images[index].setBytes(ba); }};
    }
    boxImages.addStretch();
    reload();

    shortcut.connect([this] { new GoesViewer{this, goesUrl}; });
    for (auto index : range(urls.size() + 1)) {
        shortcuts.emplace_back(QKeySequence{QString::fromStdString(To::string(index))}, this);
        shortcuts.back().connect([this, parent, index] { new ImageViewer{parent, images[index - 1].bytes}; });
    }
}

void NhcStorm::reload() {
    new FutureText{this, textProductUrl, [this] (const auto& s) { text.setText(s); }};
}

void NhcStorm::changeProduct() {
    textProductUrl = WString::split(comboboxProduct.getValue(), ":")[0] + stormData.binNumber;
    reload();
}

void NhcStorm::launchGoes() {
    new GoesViewer{parent, goesUrl};
}
