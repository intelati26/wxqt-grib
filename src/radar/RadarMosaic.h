// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef RADARMOSAIC_H
#define RADARMOSAIC_H

#include <string>
#include "objects/AutoUpdate.h"
#include "objects/UrlAnimation.h"
#include "ui/ComboBox.h"
#include "ui/HBox.h"
#include "ui/ZoomImage.h"
#include "ui/VBox.h"
#include "ui/Window.h"

using std::string;

class RadarMosaic : public Window {
public:
    RadarMosaic(Window *, const string& = "");

private:
    void reload();
    void showLatest(const QByteArray&);
    void changeSector();
    void resizeEventCustom() override;
    void closeEventCustom() override;
    AutoUpdate autoUpdate;
    VBox box;
    HBox boxH;
    ZoomImage image;
    ComboBox comboboxSector;
    UrlAnimation objectAnimate;
    Shortcut shortcutAutoUpdate;
    Shortcut shortcutLocal;
    Shortcut shortcutConus;
};

#endif  // RADARMOSAIC_H
