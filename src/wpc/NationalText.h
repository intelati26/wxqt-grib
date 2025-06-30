// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef NATIONALTEXT_H
#define NATIONALTEXT_H

#include <string>
#include <vector>
#include "ui/BackForward.h"
#include "ui/HBox.h"
#include "ui/PopoverMenu.h"
#include "ui/ScrolledWindow.h"
#include "ui/Text.h"
#include "ui/VBox.h"
#include "ui/Window.h"

using std::string;
using std::vector;

class NationalText : public Window {
public:
    NationalText(Window *, const string& = "");

private:
    void reload();
    void moveBack();
    void moveForward();
    void changeProductByCode(const string&);
    VBox box;
    HBox hbox;
    ScrolledWindow sw;
    Text text;
    BackForward backForward;
    vector<PopoverMenu> popoverMenus;
    string product;
    const string prefTokenProduct{"WPCTEXT_PARAM_LAST_USED"};
    bool savePref;
    int index;
};

#endif  // NATIONALTEXT_H
