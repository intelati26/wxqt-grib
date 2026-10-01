// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef GOESVIEWER_H
#define GOESVIEWER_H

#include <string>
#include <QLabel>
#include "objects/AutoUpdate.h"
#include "objects/UrlAnimation.h"
#include "ui/BackForward.h"
#include "ui/ComboBox.h"
#include "ui/HBox.h"
#include "ui/ZoomImage.h"
#include "ui/VBox.h"
#include "ui/Window.h"

using std::string;
using std::vector;

class GoesViewer : public Window {
public:
    GoesViewer(Window *, const string& url, const string& product = "", const string& sector = "", bool = true);

private:
    void reload();
    void showLatest(const QByteArray&);
    void moveBack();
    void moveForward();
    void changeSector();
    void changeProduct();
    void changeCount();
    void changeSize();
    void loadSizes();
    void loadImage();
    void resizeEventCustom() override;
    void closeEventCustom() override;
    AutoUpdate autoUpdate;
    HBox boxH;
    VBox box;
    ZoomImage image;
    ComboBox comboboxSector;
    ComboBox comboboxProduct;
    ComboBox comboboxCount;
    ComboBox comboboxSize;
    QLabel notice;   // visible when the requested image could not be loaded
    string sizeChoice;   // empty = the app's default size for the sector
    UrlAnimation objectAnimate;
    BackForward backForward;
    vector<string> productLabels;   // what the product list offers, and the folder name of each: a storm floater only has some of them
    vector<string> productCodeList;
    bool goesFloater;
    string goesFloaterUrl;
    Shortcut shortcutAutoUpdate;
    bool savePref;
};

#endif  // GOESVIEWER_H
