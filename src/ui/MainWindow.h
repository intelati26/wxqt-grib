// *****************************************************************************
// * Updated MainWindow.h with WeatherGraph integration
// * Added WeatherGraph widget to the forecast section of the MainWindow
// *****************************************************************************

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <string>
#include <unordered_map>
#include <vector>
#include "objects/DownloadTimer.h"
#include "misc/SevereNotice.h"
#include "radar/PolygonType.h"
#include "settings/UIPreferences.h"
#include "ui/CardCurrentConditions.h"
#include "ui/CardHazards.h"
#include "ui/ComboBox.h"
#include "ui/FlowBox.h"
#include "ui/HBox.h"
#include "ui/Image.h"
#include "ui/ScrolledWindow.h"
#include "ui/SevenDayCollection.h"
#include "ui/Text.h"
#include "ui/Toolbar.h"
#include "ui/VBox.h"
#include "ui/Window.h"
#include "ui/WeatherGraph.h"
#include "util/CurrentConditions.h"
#include "util/Hazards.h"
#include "util/SevenDay.h"
#include "misc/UtilityHourly.h"
#include "misc/ForecastPointViewer.h"
#include "misc/UtilityForecastPoint.h"
#include <memory>

using std::string;
using std::unordered_map;
using std::vector;

class MainWindow : public Window {
public:
    explicit MainWindow(QWidget * = nullptr);
    void openRoute(const string& id) { toolbar.launchRoute(id); }   // development aid (WXQT_OPEN): open a toolbar entry by its id

// protected:
//     bool event(QEvent *) override;

private:
    void reload();
    void getCc();
    void get7day();
    void getHazards();
    void getHourlyGraphData();
    void updateCc();
    void update7day();
    void updateHazards();
    void updateHourlyGraph();
    void configChangeCheck();
    static string computeTokenString();
    void locationChange();
    void addWidgets();
    void arrangeColumns();
    void launchImageScreen(const string&);
    void downloadWatch();
    void updateWatch();
    bool launch(int);
    void showHourlyGraph();
    void placeHourlyGraph();
    VBox box;
    HBox boxH;
    VBox boxZones;         // right of the toolbar: holds the zone grid chosen under Settings > Home Screen Order
    QWidget * zonesWidget{};
    FlowBox imageLayout;   // fixed-size thumbnails, wrapped to the width of their zone
    // the four large sections; HomeLayout says which zone each one is in
    QWidget * severeHolder{};
    QWidget * imagesHolder{};
    QWidget * forecastHolder{};
    QWidget * textHolder{};
    VBox rightMostLayout;
    VBox forecastLayout;
    VBox boxCc;
    VBox boxSevenDay;
    VBox boxHazards;
    HBox boxHourlyGraph;   // Added for hourly graph
    HBox boxForecastPoint;                                     // the forecast point card: the week at a glance and the outlooks
    CardForecastPoint * forecastPointCard{};
    std::shared_ptr<UtilityForecastPoint::Data> pointData;     // downloaded off the GUI thread, drawn on it
    FlowBox boxSevereDashboard;   // the mini severe dashboard: warnings, storm reports, watches, discussions
    ScrolledWindow sw;
    ComboBox comboBox;
    Toolbar toolbar;
    SevenDay sevenDay;
    CurrentConditions currentConditions;
    CardCurrentConditions cardCurrentConditions;
    SevenDayCollection sevenDayCollection;
    Hazards hazards;
    CardHazards cardHazards;
    string hourlyGraphJson;   // downloaded off the GUI thread, drawn on it
    WeatherGraph hourlyGraph;   // Added: Hourly weather graph widget
    unordered_map<string, Image> imageWidgets;
    unordered_map<string, Text> textWidgets;
    string tokenString;
    int imageSize{UIPreferences::mainScreenImageSize};
    vector<string> urls;
    vector<string> captionsList;   // a caption for each of urls
    vector<Image> images;
    unordered_map<PolygonType, SevereNotice> watchesByType;
    vector<QByteArray> bytesList;
    DownloadTimer timer;
    Shortcut shortcutClose;
    Shortcut shortcutVis;
    Shortcut shortcutWfoText;
    Shortcut shortcutHourly;
    Shortcut shortcutRadar;
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
