// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef NATIONALIMAGES_H
#define NATIONALIMAGES_H

#include <string>
#include <vector>
#include "ui/BackForward.h"
#include "ui/HBox.h"
#include "ui/Photo.h"
#include "ui/PopoverMenu.h"
#include "ui/VBox.h"
#include "ui/Window.h"

using std::string;
using std::vector;

class NationalImages : public Window {
public:
    explicit NationalImages(Window *);

private:
    void reload();
    void moveBack();
    void moveForward();
    void changeProductByCode(const string&);
    void resizeEventCustom() override;
    VBox box;
    HBox hbox;
    Photo photo;
    BackForward backForward;
    int index;
    const string prefToken{"WPCIMG_PARAM_LAST_USED"};
    vector<PopoverMenu> popoverMenus;
};

#endif  // NATIONALIMAGES_H
