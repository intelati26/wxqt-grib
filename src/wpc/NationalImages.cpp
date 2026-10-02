// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "NationalImages.h"
#include <algorithm>
#include "common/GlobalVariables.h"
#include "objects/FutureBytes.h"
#include "objects/WString.h"
#include "util/Utility.h"
#include "util/UtilityList.h"
#include "wpc/UtilityWpcImages.h"

NationalImages::NationalImages(Window * parent)
    : Window{parent}
    , image{this}
    , objectAnimate{this, &image, [] (string, string sector, int) {
          // the loop is the menu group the chart is in: the sector carries the chart's number
          vector<string> urls;
          for (const auto i : UtilityWpcImages::seriesOf(std::atoi(sector.c_str()))) {
              urls.push_back(UtilityWpcImages::imageUrl(i));
          }
          return urls;
      }}
    , backForward{this, [this] { moveBack(); }, [this] { moveForward(); }}
    , index{Utility::readPrefInt(prefToken, 0)}
{
    objectAnimate.labeler = [this] (const string&, size_t i) {
        const auto group = UtilityWpcImages::seriesOf(index);
        return i < group.size() ? UtilityWpcImages::labels[static_cast<size_t>(group[i])] : string{};
    };
    hbox.addLayout(backForward);
    box.addLayout(hbox);
    objectAnimate.addTo(box);
    box.addWidgetReal(&image, 1, Qt::Alignment{});
    box.getAndShow(this);

    auto itemsSoFar = 0;
    for (auto& menu : UtilityWpcImages::titles) {
        menu.setList(UtilityWpcImages::labels, itemsSoFar);
        itemsSoFar += menu.count;
    }
    for (auto& objectMenuTitle : UtilityWpcImages::titles) {
        popoverMenus.emplace_back(this, objectMenuTitle.title, objectMenuTitle.get(), [this] (const auto& s) { changeProductByCode(s); });
        hbox.addWidget(popoverMenus.back());
    }
    reload();
}

void NationalImages::reload() {
    objectAnimate.stopAnimateNoDownload();
    Utility::writePrefInt(prefToken, index);
    setTitle(UtilityWpcImages::labels[index]);
    objectAnimate.product = UtilityWpcImages::seriesName(index);
    objectAnimate.sector = std::to_string(index);
    new FutureBytes{this, UtilityWpcImages::imageUrl(index), [this] (const auto& ba) { showLatest(ba); }};
    objectAnimate.refresh();
}

void NationalImages::showLatest(const QByteArray& bytes) {
    if (bytes.isEmpty()) {
        return;
    }
    image.setBytesKeepView(bytes);
    objectAnimate.setCurrentBytes(bytes);
}

void NationalImages::moveBack() {
    index -= 1;
    index = std::max(index, 0);
    reload();
}

void NationalImages::moveForward() {
    index += 1;
    index = std::min(index, static_cast<int>(UtilityWpcImages::urls.size()) - 1);
    reload();
}

void NationalImages::changeProductByCode(const string& s) {
    index = findex(s, UtilityWpcImages::labels);
    reload();
}

void NationalImages::closeEventCustom() {
    objectAnimate.stopAnimateNoDownload();
}
