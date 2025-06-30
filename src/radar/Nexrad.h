// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef NEXRAD_H
#define NEXRAD_H

#include <string>
#include <vector>
#include "objects/AutoUpdate.h"
#include "objects/ObjectAnimateNexrad.h"
// #include <QGeoPositionInfo>
// #include <QGeoPositionInfoSource>
// #include "objects/Timer.h"
#include "radar/NexradLayerDownload.h"
#include "radar/NexradWidget.h"
#include "ui/Button.h"
#include "ui/ComboBox.h"
#include "ui/HBox.h"
#include "ui/Text.h"
#include "ui/VBox.h"
#include "ui/Window.h"

using std::string;
using std::vector;

class Nexrad : public Window {
public:
    Nexrad(Window *, int, bool, const string&);
    ~Nexrad() override;

private:
    void syncRadarSite(const string&, int, bool);
    void adjustControls();
    void downloadData();
    void setTitleMain();
    string radarInfoForTitle();
    void changeProduct();
    void changeProductFromChild(int, const string&);
    void changeSectorFromChild(int, const string&);
    void changeRadarSite();
    void changeTilt();
    void adjustProductComboBox();
    void changeZoom(double, int);
    void changePosition(double, double, int);
    void zoomOut();
    void zoomIn();
    void moveLeft();
    void moveRight();
    void moveUp();
    void moveDown();
    void drawAndSave();
    void save();
    void settingsCheck();
    void adjustColorLegends();
    // void positionUpdated(const QGeoPositionInfo&);
    // QGeoPositionInfoSource * source;
    void closeEventCustom() override;
    VBox box;
    HBox boxH;
    HBox nexradBox;
    HBox nexradBox2;
    bool useASpecificRadar;
    ComboBox comboboxSector;
    ComboBox comboboxProduct;
    ComboBox comboboxTilt;
    ComboBox comboboxAnimCount;
    ComboBox comboboxAnimSpeed;
    AutoUpdate autoUpdate;
    Button settingsButton;
    Button moveLeftButton;
    Button moveRightButton;
    Button moveDownButton;
    Button moveUpButton;
    Button zoomOutButton;
    Button zoomInButton;
    vector<NexradWidget *> nexradList;
    Text textFrameCount;
    Text textTilt;
    Text textAnimSpeed;
    NexradLayerDownload nexradLayerDownload;
    ObjectAnimateNexrad objectAnimateNexrad;
    const double moveIncrement{100.0};
    Shortcut shortcutReload;
    Shortcut shortcutU;
    Shortcut shortcutQ;
    Shortcut shortcutL;
    Shortcut shortcutT;
    Shortcut shortcutC;
    Shortcut shortcutAnimate;
    Shortcut shortcutZoomIn;
    Shortcut shortcutZoomOut;
    Shortcut shortcutMoveLeft;
    Shortcut shortcutMoveRight;
    Shortcut shortcutMoveUp;
    Shortcut shortcutMoveDown;
    Shortcut shortcutMoveLeft2;
    Shortcut shortcutMoveRight2;
    Shortcut shortcutMoveUp2;
    Shortcut shortcutMoveDown2;
    Shortcut shortcutKeyboard;
    Shortcut shortcutSettings;
};

#endif  // NEXRAD_H
