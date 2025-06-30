// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef UIPREFERENCES_H
#define UIPREFERENCES_H

#include <string>
#include <vector>
#include <QMargins>
#include "objects/PrefBool.h"

using std::string;
using std::vector;

class UIPreferences {
public:
    static void initialize();
    static const int boxPadding;
    static const int padding;
    static int fontSize;
    static bool unitsM;
    static bool unitsF;
    static int mainScreenImageSize;
    static int nwsIconSize;
    static int comboBoxSize;
    static int toolbarIconSize;
    static bool tiledWindows;
    static QMargins textPadding;
    static const bool useNwsApi;
    static const bool useNwsApiForHourly;
    static bool nexradMainScreen;
    static bool mainScreenSevereDashboard;
    static bool nexradScrollWheelMotion;
    static bool rememberGOES;
    static bool rememberMosaic;
    static vector<PrefBool> homeScreenItemsImage;
    static vector<PrefBool> homeScreenItemsText;
};

#endif  // UIPREFERENCES_H
