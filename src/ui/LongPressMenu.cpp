// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "LongPressMenu.h"
#include "util/UtilityList.h"

LongPressMenu::LongPressMenu(Window * parent)
    : parent{parent}
    , contextMenu{new QMenu{parent}}
{}

void LongPressMenu::add(const CMenuItem& cmenuItem) {
    cmenuItems.push_back(cmenuItem);
    actions.emplace_back(cmenuItem.label, parent);
    contextMenu->addAction(actions.back().get());
}

void LongPressMenu::show(const QPoint& posGlobal) {
    const auto selectedItem = contextMenu->exec(posGlobal);
    for (auto index : range(actions.size())) {
        if (selectedItem == actions[index].get()) {
            cmenuItems[index].fn();
        }
    }
}
