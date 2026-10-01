// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "NhcStorm.h"
#include <QImage>
#include "util/CrashLog.h"
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

    // the pictures NHC's own graphics page lists for the storm (cone first); the older fixed names only if that page gave none
    if (!stormData.graphicUrls.empty()) {
        fullUrls = stormData.graphicUrls;
    } else {
        for (const auto& suffix : urls) {
            auto url = stormData.baseUrl;
            if (suffix == "WPCQPF_sm2.gif" || suffix == "WPCERO_sm2.gif") {
                url = WString::replace(url, ObjectDateTime::getYearString(), ObjectDateTime::getYearShortString());
            }
            fullUrls.push_back(url + suffix);
        }
    }
    for ([[maybe_unused]] const auto& unused : fullUrls) {
        images.emplace_back(this);
        images.back().imageSize = 250;
        boxImages.addWidget(images.back());
    }
    for (auto index : range(fullUrls.size())) {
        images[index].connect([this, index] { new ImageViewer{this, images[index].bytes}; });
        new FutureBytes{this, fullUrls[index], [this, index] (const auto& ba) {
            CrashLog::write("NHC picture " + fullUrls[index] + " -> " + std::to_string(ba.size()) + " bytes, " + std::to_string(QImage::fromData(ba).width()) + " px wide");
            images[index].setBytes(ba);
        }};
    }
    if (stormData.coneUrl.empty() && !stormData.graphicUrls.empty()) {
        setTitle("NHC Storm " + stormData.forTopHeader() + " - NHC publishes no cone forecast for this storm");
    }
    boxImages.addStretch();
    reload();

    shortcut.connect([this] { new GoesViewer{this, goesUrl}; });
    for (auto index : range(fullUrls.size() + 1)) {
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
