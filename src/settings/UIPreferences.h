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

// A user-arranged order of home screen tokens, saved as a comma-joined pref.
// Always holds every token (shown or hidden), so hiding an item keeps its
// place; tokens added since the order was saved are appended in their
// built-in order.
class HomeScreenOrder {
public:
    HomeScreenOrder(const string& prefToken, const vector<string>& defaults);
    void load();
    // swaps with the neighbour, wrapping at each end (like Toolbar Order)
    void move(int from, int to);
    // takes a whole new order (from dragging in Settings): unknown tokens are ignored, any missing ones keep their place at the end
    void set(const vector<string>& order);
    const vector<string>& getTokens() const;

private:
    string prefToken;
    vector<string> defaults;
    vector<string> tokens;
};

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
    static bool mainScreenSevereDashboard;
    static bool hourlyGraph;        // the hourly graph on the home screen
    static bool forecastPoint;      // the week at a glance and the outlooks of the point (the NWS forecast points page) on the home screen
    static bool hourlyGraphAbove;   // ... above the seven day forecast, else below it
    static bool homeCaptions;   // a short caption under each home screen picture
    static bool mapScrollWheelMotion;
    static bool rememberGOES;
    static bool rememberMosaic;
    static vector<PrefBool> homeScreenItemsImage;
    static vector<PrefBool> homeScreenItemsText;
    // Settings > Home Screen Order: the columns right of the toolbar, and the
    // items within the image column (homeScreenItemsImage tokens plus
    // the pictures) and the text column
    static const string homeColumnImages;
    static const string homeColumnForecast;
    static const string homeColumnText;
    static HomeScreenOrder homeScreenColumnOrder;
    static HomeScreenOrder homeScreenImageOrder;
    static HomeScreenOrder homeScreenTextOrder;
    static string homeScreenLabel(const string& token);
};

#endif  // UIPREFERENCES_H
