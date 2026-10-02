// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "misc/Opc.h"
#include <algorithm>
#include "objects/FutureBytes.h"
#include "misc/UtilityOpcImages.h"
#include "util/Utility.h"

Opc::Opc(Window * parent)
    : Window{parent}
    , image{this}
    , objectAnimate{this, &image, [] (string, string sector, int) {
          // the loop is the run of charts this one belongs to: the sector carries the chart's number
          vector<string> urls;
          for (const auto i : UtilityOpcImages::seriesOf(std::atoi(sector.c_str()))) {
              urls.push_back(UtilityOpcImages::urls[static_cast<size_t>(i)]);
          }
          return urls;
      }}
    , comboBox{this, UtilityOpcImages::labels}
    , backForward{this, [this] { moveBack(); }, [this] { moveForward(); }}
{
    const auto index = Utility::readPrefInt(prefToken, 0);
    comboBox.setIndexByValue(UtilityOpcImages::labels[index]);
    comboBox.connect([this] { reload(); });
    objectAnimate.labeler = [this] (const string&, size_t i) {
        const auto run = UtilityOpcImages::seriesOf(comboBox.getIndex());
        return i < run.size() ? UtilityOpcImages::labels[static_cast<size_t>(run[i])] : string{};
    };
    boxH.addWidget(comboBox);
    boxH.addLayout(backForward);
    box.addLayout(boxH);
    objectAnimate.addTo(box);
    box.addWidgetReal(&image, 1, Qt::Alignment{});
    box.getAndShow(this);
    reload();
}

void Opc::reload() {
    objectAnimate.stopAnimateNoDownload();
    const auto index = comboBox.getIndex();
    const auto& url = UtilityOpcImages::urls[index];
    setTitle("OPC - " + UtilityOpcImages::labels[index]);
    Utility::writePrefInt(prefToken, index);
    objectAnimate.product = UtilityOpcImages::seriesName(index);
    objectAnimate.sector = std::to_string(index);
    new FutureBytes{this, url, [this] (const auto& ba) { showLatest(ba); } };
    objectAnimate.refresh();
}

void Opc::showLatest(const QByteArray& bytes) {
    if (bytes.isEmpty()) {
        return;
    }
    image.setBytesKeepView(bytes);
    objectAnimate.setCurrentBytes(bytes);
}

void Opc::moveBack() {
    auto index = comboBox.getIndex();
    index -= 1;
    index = std::max(index, 0);
    comboBox.setIndex(index);
    reload();
}

void Opc::moveForward() {
    auto index = comboBox.getIndex();
    index += 1;
    index = std::min(index, static_cast<int>(UtilityOpcImages::labels.size()) - 1);
    comboBox.setIndex(index);
    reload();
}

void Opc::closeEventCustom() {
    objectAnimate.stopAnimateNoDownload();
}
