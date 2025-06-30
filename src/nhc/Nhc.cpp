// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "Nhc.h"
#include "misc/ImageViewer.h"
#include "nhc/NhcOceanEnum.h"
#include "nhc/NhcRegionSummary.h"
#include "nhc/NhcStorm.h"
#include "objects/FutureBytes.h"
#include "objects/FutureVoid.h"
#include "objects/WString.h"
#include "util/To.h"
#include "util/UtilityList.h"
#include "wpc/NationalText.h"

Nhc::Nhc(Window * parent)
    : Window{parent}
    , parent{parent}
    , sw{this, box}
    , comboBoxText{this, textProducts}
    , comboBoxImages{this, imageLabels}
{
    setTitle("NHC");
    new FutureVoid{this, [this ] { objectNhc.getTextData(); }, [this] { updateText(); }};
    for (auto region : {ATL, EPAC, CPAC}) {
        addAll(urls, NhcRegionSummary{region}.urls);
        addAll(imageTitles, NhcRegionSummary{region}.titles);
    }
    comboBoxText.setIndex(0);
    comboBoxImages.setIndex(0);
    comboBoxText.connect([this] { new NationalText{this, WString::split(comboBoxText.getValue(), ":")[0]}; });
    comboBoxImages.connect([this] { new ImageViewer{this, imageUrls[comboBoxImages.getIndex()], comboBoxImages.getValue()}; });
    box.addWidget(comboBoxText);
    box.addWidget(comboBoxImages);
    box.addLayout(boxText);
    box.addImageRows(this, urls, images, 3, getWindowWidth());
    for (auto index : range(urls.size())) {
        images[index].connect([this, index] { launchImage(index); });
        new FutureBytes{this, urls[index], [this, index] (const auto& ba) { images[index].setBytes(ba); }};
    }
}

void Nhc::updateText() {
    objectNhc.showTextData();
    auto i = 0;
    for (const auto& storm : objectNhc.stormDataList) {
        stormCards.push_back(std::make_unique<CardNhcStormReportItem>(this, storm));
        boxText.addLayoutReal(stormCards.back()->getView());
        shortcuts.push_back(Shortcut{QKeySequence{QString::fromStdString(To::string(i + 1))}, this});
        shortcuts.back().connect([this, storm] { new NhcStorm{parent, storm}; });
        i += 1;
    }
}

void Nhc::launchImage(size_t index) {
    if (objectNhc.stormDataList.size() > index) {
        new ImageViewer{this, images[index].bytes, imageTitles[index]};
    }
}
