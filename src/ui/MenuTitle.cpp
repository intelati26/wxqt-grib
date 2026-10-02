// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "MenuTitle.h"
#include <algorithm>

MenuTitle::MenuTitle(const string& title, int count)
    : title{title}
    , count{count}
{}

void MenuTitle::setList(const vector<string>& items, int index) {
    // a menu that asks for more entries than the list has (a label dropped without its count) gets what is there, not a crash
    const auto size = static_cast<int>(items.size());
    const auto first = std::clamp(index, 0, size);
    const auto last = std::clamp(index + count, first, size);
    this->items = vector<string>{items.begin() + first, items.begin() + last};
}

vector<string> MenuTitle::get() const {
    return items;
}
