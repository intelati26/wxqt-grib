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
    , photo{this, FullWithHeight, [this] { return getPhotoHeight(); }}
    , comboBox{this, UtilityOpcImages::labels}
    , backForward{this, [this] { moveBack(); }, [this] { moveForward(); }}
{
    const auto index = Utility::readPrefInt(prefToken, 0);
    comboBox.setIndexByValue(UtilityOpcImages::labels[index]);
    comboBox.connect([this] { reload(); });
    boxH.addWidget(comboBox);
    boxH.addLayout(backForward);
    box.addLayout(boxH);
    box.addWidgetAndCenter(photo);
    box.getAndShow(this);
    reload();
}

void Opc::reload() {
    const auto index = comboBox.getIndex();
    const auto& url = UtilityOpcImages::urls[index];
    setTitle("OPC - " + UtilityOpcImages::labels[index]);
    Utility::writePrefInt(prefToken, index);
    new FutureBytes{this, url, [this] (const auto& ba) { photo.setBytes(ba); } };
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
