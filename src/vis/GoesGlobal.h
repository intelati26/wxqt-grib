// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef GOESGLOBAL_H
#define GOESGLOBAL_H

#include <string>
#include "objects/AutoUpdate.h"
#include "objects/ObjectAnimate.h"
#include "ui/BackForward.h"
#include "ui/ComboBox.h"
#include "ui/HBox.h"
#include "ui/Photo.h"
#include "ui/Shortcut.h"
#include "ui/Window.h"
#include "ui/VBox.h"

using std::string;

class GoesGlobal : public Window {
public:
    explicit GoesGlobal(Window *);

private:
    void reload();
    void moveBack();
    void moveForward();
    void changeProduct();
    void changeSector();
    void changeCount();
    void closeEventCustom() override;
    HBox boxH;
    VBox box;
    Photo photo;
    ComboBox comboboxProduct;
    ObjectAnimate objectAnimate;
    BackForward backForward;
    const string prefToken{"GOESFULLDISK_IMG_FAV_URL"};
    int index;
    Shortcut shortcutAnimate;
};

#endif  // GOESGLOBAL_H
