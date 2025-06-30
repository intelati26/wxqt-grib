// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef GOESVIEWER_H
#define GOESVIEWER_H

#include <string>
#include "objects/AutoUpdate.h"
#include "objects/ObjectAnimate.h"
#include "ui/BackForward.h"
#include "ui/ComboBox.h"
#include "ui/HBox.h"
#include "ui/Photo.h"
#include "ui/VBox.h"
#include "ui/Window.h"

using std::string;

class GoesViewer : public Window {
public:
    GoesViewer(Window *, const string& url, const string& product = "", const string& sector = "", bool = true);

private:
    void reload();
    void moveBack();
    void moveForward();
    void changeSector();
    void changeProduct();
    void changeCount();
    void resizeEventCustom() override;
    void closeEventCustom() override;
    AutoUpdate autoUpdate;
    HBox boxH;
    VBox box;
    Photo photo;
    ComboBox comboboxSector;
    ComboBox comboboxProduct;
    ComboBox comboboxCount;
    ObjectAnimate objectAnimate;
    BackForward backForward;
    bool goesFloater;
    string goesFloaterUrl;
    Shortcut shortcutAnimate;
    Shortcut shortcutAutoUpdate;
    bool savePref;
};

#endif  // GOESVIEWER_H
