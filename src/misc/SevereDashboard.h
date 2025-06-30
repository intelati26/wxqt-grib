// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef SEVEREDASHBOARD_H
#define SEVEREDASHBOARD_H

#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include "misc/SevereNotice.h"
#include "misc/SevereWarning.h"
#include "objects/AutoUpdate.h"
#include "radar/PolygonType.h"
#include "ui/CardBlackHeaderText.h"
#include "ui/CardDashAlertItem.h"
#include "ui/HBox.h"
#include "ui/Image.h"
#include "ui/ScrolledWindow.h"
#include "ui/VBox.h"
#include "ui/Window.h"

using std::string;
using std::unordered_map;
using std::vector;

class SevereDashboard : public Window {
public:
    explicit SevereDashboard(Window *);

private:
    void reload();
    void downloadWatch();
    void updateWatch();
    void updateWarnings(PolygonType);
    void launch(size_t);
    void updateTitle();
    // void resizeEventCustom() override;
    void closeEventCustom() override;
    const int imagesAcross{4};
    AutoUpdate autoUpdate;
    VBox box;
    VBox boxWarningsMain;
    VBox boxImages;
    HBox boxH;
    ScrolledWindow sw;
    vector<HBox> boxRows;
    vector<string> urls;
    vector<Image> images;
    vector<CardBlackHeaderText> headerTextList;
    vector<CardDashAlertItem> dashAlertItems;
    unordered_map<PolygonType, SevereNotice> severeNotices;
    unordered_map<PolygonType, SevereWarning> warningsByType;
    unordered_map<PolygonType, VBox> boxWarnings;
    vector<Shortcut> shortcuts;
    Shortcut shortcutAutoUpdate;
    std::mutex mtx;
    const vector<PolygonType> warningTypes{Tor, Tst, Ffw};
};

#endif  // SEVEREDASHBOARD_H
