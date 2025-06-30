// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "MenuTitle.h"

MenuTitle::MenuTitle(const string& title, int count)
    : title{title}
    , count{count}
{}

void MenuTitle::setList(const vector<string>& items, int index) {
    this->items = vector<string>{items.begin() + index, items.begin() + index + count};
}

vector<string> MenuTitle::get() const {
    return items;
}
