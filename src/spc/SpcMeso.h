// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef SPCMESO_H
#define SPCMESO_H

#include <string>
#include <vector>
#include "objects/AutoUpdate.h"
#include "objects/ObjectAnimate.h"
#include "ui/BackForward.h"
#include "ui/Button.h"
#include "ui/ComboBox.h"
#include "ui/HBox.h"
#include "ui/Photo.h"
#include "ui/PopoverMenu.h"
#include "ui/Shortcut.h"
#include "ui/VBox.h"
#include "ui/Window.h"

using std::string;
using std::vector;

class SpcMeso : public Window {
public:
    SpcMeso(Window *, const string& = "", const string& = "", bool = true);

private:
    void reload();
    void moveBack();
    void moveForward();
    void changeProductForFav(int);
    void changeProductByCode(const string&);
    void changeSector();
    void closeEventCustom() override;
    HBox boxH;
    VBox box;
    VBox boxFav;
    HBox imageLayout;
    AutoUpdate autoUpdate;
    Photo photo;
    ComboBox comboboxSector;
    ObjectAnimate objectAnimate;
    BackForward backForward;
    int index;
    bool savePrefs;
    vector<Button> buttons;
    vector<PopoverMenu> popoverMenus;
    Shortcut shortcutAnimate;
    Shortcut shortcutAutoUpdate;
    vector<Shortcut> shortcuts;
    const string prefTokenProduct{"SPCMESO1_PARAM_LAST_USED"};
    const string prefTokenSector{"SPCMESO1_SECTOR_LAST_USED"};
};

#endif  // SPCMESO_H
