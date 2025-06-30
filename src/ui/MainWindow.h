// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <string>
#include <unordered_map>
#include <vector>
#include "objects/DownloadTimer.h"
#include "misc/SevereNotice.h"
#include "radar/PolygonType.h"
#include "radar/NexradWidget.h"
#include "settings/UIPreferences.h"
#include "ui/CardCurrentConditions.h"
#include "ui/CardHazards.h"
#include "ui/ComboBox.h"
#include "ui/HBox.h"
#include "ui/Image.h"
#include "ui/ScrolledWindow.h"
#include "ui/SevenDayCollection.h"
#include "ui/Text.h"
#include "ui/Toolbar.h"
#include "ui/VBox.h"
#include "ui/Window.h"
#include "util/CurrentConditions.h"
#include "util/Hazards.h"
#include "util/SevenDay.h"

using std::string;
using std::unordered_map;
using std::vector;

class MainWindow : public Window {
public:
    explicit MainWindow(QWidget * = nullptr);

// protected:
//     bool event(QEvent *) override;

private:
    void reload();
    void getCc();
    void get7day();
    void getHazards();
    void updateCc();
    void update7day();
    void updateHazards();
    void configChangeCheck();
    static string computeTokenString();
    void locationChange();
    void addWidgets();
    void launchImageScreen(const string&);
    void downloadWatch();
    void updateWatch();
    bool launch(int);
    VBox box;
    HBox boxH;
    VBox imageLayout;
    VBox rightMostLayout;
    VBox forecastLayout;
    VBox boxCc;
    VBox boxSevenDay;
    VBox boxHazards;
    HBox boxSevereDashboard;
    ScrolledWindow sw;
    ComboBox comboBox;
    Toolbar toolbar;
    SevenDay sevenDay;
    CurrentConditions currentConditions;
    CardCurrentConditions cardCurrentConditions;
    SevenDayCollection sevenDayCollection;
    Hazards hazards;
    CardHazards cardHazards;
    unordered_map<string, Image> imageWidgets;
    unordered_map<string, Text> textWidgets;
    string tokenString;
    int imageSize{UIPreferences::mainScreenImageSize};
    vector<string> urls;
    vector<Image> images;
    unordered_map<PolygonType, SevereNotice> watchesByType;
    vector<QByteArray> bytesList;
    vector<NexradWidget *> nexradList;
    DownloadTimer timer;
    Shortcut shortcutClose;
    Shortcut shortcutVis;
    Shortcut shortcutWfoText;
    Shortcut shortcutHourly;
    Shortcut shortcutRadar;
    Shortcut shortcutRadarSinglePane;
    Shortcut shortcutRadarDualPane;
    Shortcut shortcutRadarQuadPane;
    Shortcut shortcutSevereDash;
    Shortcut shortcutNcep;
    Shortcut shortRadarMosaic;
    Shortcut shortcutNhc;
    Shortcut shortcutSettings;
    Shortcut shortcutSwo;
    Shortcut shortcutNationalImages;
    Shortcut shortcutSpcMeso;
    Shortcut shortcutSpcFire;
    Shortcut shortcutLightning;
    Shortcut shortcutReload;
    Shortcut shortcutKeyboard;
    Shortcut shortcutWpcText;
    Shortcut shortcutRainfallOutlook;
    Shortcut shortcutRtma;
    Shortcut shortcutUsAlerts;
};

#endif  // MAINWINDOW_H
