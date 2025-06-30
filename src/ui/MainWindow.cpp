// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "MainWindow.h"
#include <QApplication>
#include "common/GlobalVariables.h"
#include "objects/FutureBytes.h"
#include "objects/FutureText.h"
#include "objects/FutureVoid.h"
#include "objects/PolygonWatch.h"
#include "objects/Route.h"
#include "misc/TextViewerStatic.h"
#include "misc/UsAlerts.h"
#include "spc/SpcMcdWatchMpdViewer.h"
#include "spc/SpcStormReports.h"
#include "settings/Location.h"
#include "util/DownloadImage.h"
#include "util/UtilityIO.h"
#include "util/UtilityList.h"
#include "util/UtilityUI.h"

MainWindow::MainWindow(QWidget * parent)
    : Window{parent}
    , sw{this, box}
    , comboBox{this, Location::listOfNames()}
    , toolbar{this, [this] { reload(); }}
    , cardCurrentConditions{this, currentConditions}
    , sevenDayCollection{this, &boxSevenDay, &sevenDay}
    , cardHazards{this, hazards}
    , timer{"DOWNLOAD_TIMER_MAIN_WINDOW"}
    , shortcutClose{{"Q"}, this}
    , shortcutVis{{"C"}, this}  // was QKeySequence("Ctrl+C")
    , shortcutWfoText{{"A"}, this}
    , shortcutHourly{{"H"}, this}
    , shortcutRadar{{"R"}, this}
    , shortcutRadarSinglePane{{"1"}, this}
    , shortcutRadarDualPane{{"2"}, this}
    , shortcutRadarQuadPane{{"4"}, this}
    , shortcutSevereDash{{"D"}, this}
    , shortcutNcep{{"N"}, this}
    , shortRadarMosaic{{"M"}, this}
    , shortcutNhc{{"O"}, this}
    , shortcutSettings{{"P"}, this}
    , shortcutSwo{{"S"}, this}
    , shortcutNationalImages{{"I"}, this}
    , shortcutSpcMeso{{"Z"}, this}
    , shortcutSpcFire{{"F"}, this}
    , shortcutLightning{{"L"}, this}
    , shortcutReload{{"U"}, this}
    , shortcutKeyboard{{"/"}, this}
    , shortcutWpcText{{"T"}, this}
    , shortcutRainfallOutlook{{"G"}, this}
    , shortcutRtma{{"B"}, this}
    , shortcutUsAlerts{{"K"}, this}
{
    QFont font{};
    font.setPointSize(UIPreferences::fontSize);
    QApplication::setFont(font);

    watchesByType.insert({Watch, SevereNotice{Watch}});
    watchesByType.insert({Mcd, SevereNotice{Mcd}});
    watchesByType.insert({Mpd, SevereNotice{Mpd}});

    comboBox.setIndex(Location::getCurrentLocation());
    comboBox.connect([this] { locationChange(); });
    Location::comboBox = &comboBox;

    boxSevenDay.setSpacing(0);

    addWidgets();
    box.addLayout(boxSevereDashboard);
    box.addLayout(boxH);
    boxH.addLayout(toolbar);
    boxH.addLayout(imageLayout);
    boxH.addLayout(forecastLayout);
    boxH.addLayout(rightMostLayout);

    forecastLayout.addWidget(comboBox);
    forecastLayout.addLayout(boxCc);
    boxCc.addLayout(cardCurrentConditions);
    forecastLayout.addLayout(boxHazards);
    forecastLayout.addLayout(boxSevenDay);
    forecastLayout.addStretch();

    reload();

    // QScroller::grabGesture(vbox.get(), QScroller::TouchGesture);
    shortcutClose.connect([this] { close(); });
    shortcutVis.connect([this] { Route::vis(this); });
    shortcutWfoText.connect([this] { toolbar.launchWfoText(); });
    shortcutHourly.connect([this] { toolbar.launchHourly(); });
    shortcutRadar.connect([this] { toolbar.launchNexrad(1); });
    shortcutRadarSinglePane.connect([this] { toolbar.launchNexrad(1); });
    shortcutRadarDualPane.connect([this] { toolbar.launchNexrad(2); });
    shortcutRadarQuadPane.connect([this] { toolbar.launchNexrad(4); });
    shortcutSevereDash.connect([this] { toolbar.launchSevereDashboard(); });
    shortcutNcep.connect([this] { toolbar.launchModelViewerGeneric("NCEP"); });
    shortRadarMosaic.connect([this] { toolbar.launchRadarMosaicViewer(); });
    shortcutNhc.connect([this] { toolbar.launchNhc(); });
    shortcutSettings.connect([this] { toolbar.launchSettings(); });
    shortcutSwo.connect([this] { toolbar.launchSpcSwoSummary(); });
    shortcutNationalImages.connect([this] { toolbar.launchNationalImages(); });
    shortcutSpcMeso.connect([this] { toolbar.launchSpcMeso(); });
    shortcutSpcFire.connect([this] { toolbar.launchSpcFireWeatherOutlookSummary(); });
    shortcutLightning.connect([this] { Route::lightning(this); });
    shortcutReload.connect([this] { toolbar.autoUpdate.toggleAutoUpdate(); });
    shortcutKeyboard.connect([this] { new TextViewerStatic{this, GlobalVariables::mainScreenShortcuts, "Shortcuts", 700, 600}; });
    shortcutWpcText.connect([this] { toolbar.launchNationalText(); });
    shortcutRtma.connect([this] { toolbar.launchRtma(); });
    shortcutRainfallOutlook.connect([this] { toolbar.launchRainfallOutlookSummary(); });
    shortcutUsAlerts.connect([this] { toolbar.launchUsAlerts(); });
}

// bool MainWindow::event(QEvent * event) {
//     switch (event->type()) {
//         case QEvent::WindowActivate:
//             qDebug() << "Widget gained focus";
//             reload();
//             break;
//         case QEvent::WindowDeactivate:
//             break;
//     };
//     return QMainWindow::event(event);
// }

void MainWindow::reload() {
    // if (timer.isRefreshNeeded()) {
        setTitle("wX " + toolbar.autoUpdate.titleAdd);
        configChangeCheck();

        new FutureVoid{this, [this] { getCc(); }, [this] { updateCc(); }};
        new FutureVoid{this, [this] { getHazards(); }, [this] { updateHazards(); }};
        new FutureVoid{this, [this] { get7day(); }, [this] { update7day(); }};

        for (const auto& item : UIPreferences::homeScreenItemsText) {
            if (item.isEnabled()) {
                const auto t = item.getPrefToken();
                new FutureText{this, item.getPrefToken(), [this, t] (const auto& s) { textWidgets.at(t).setText(s); }};
            }
        }
        for (const auto& item : UIPreferences::homeScreenItemsImage) {
            if (item.isEnabled()) {
                const auto url = DownloadImage::byProduct(item.getPrefToken());
                const auto token = item.getPrefToken();
                new FutureBytes{this, url, [this, token] (const auto& ba) { imageWidgets.at(token).setToWidth(ba, UIPreferences::mainScreenImageSize); }};
            }
        }
        if (UIPreferences::nexradMainScreen) {
            const auto pane = 0;
            nexradList[pane]->nexradState.setRadar(Location::radarSite());
            nexradList[pane]->nexradState.reset();
            nexradList[pane]->nexradState.zoom = 0.6;
            nexradList[pane]->nexradDraw.initGeom();

            for (auto nw : nexradList) {
                new FutureVoid{this, [nw] { nw->downloadData(); }, [nw] { nw->update(); }};
            }
        }
        if (UIPreferences::mainScreenSevereDashboard) {
            new FutureVoid{this, [this] { downloadWatch(); }, [this] { updateWatch(); }};
        } else {
            boxSevereDashboard.removeChildren();
        }
    // }
}

void MainWindow::downloadWatch() {
    bytesList.clear();
    urls.clear();
    urls.push_back(DownloadImage::byProduct("USWARN"));
    urls.push_back(DownloadImage::byProduct("STRPT"));
    for (auto type : {Watch, Mcd, Mpd}) {
        PolygonWatch::byType[type]->download();
        watchesByType.at(type).getBitmaps();
        addAll(urls, watchesByType.at(type).urls);
    }
    for (auto index : range(urls.size())) {
        bytesList.push_back(UtilityIO::downloadAsByteArray(urls[index]));
    }
}

void MainWindow::updateWatch() {
    boxSevereDashboard.removeChildren();
    images.clear();
    for (auto index : range(urls.size())) {
        images.emplace_back(this);
        images.back().imageSize = 150;
        images.back().setBytes(bytesList[index]);
        images.back().connect([this, index] { launch(index); });
        boxSevereDashboard.addWidget(images.back());
    }
}

bool MainWindow::launch(int indexFinal) {
    if (indexFinal == 0) {
        new UsAlerts{this};
    } else if (indexFinal == 1) {
        new SpcStormReports{this, "today"};
    } else if (indexFinal > 1) {
        new SpcMcdWatchMpdViewer{this, urls[indexFinal]};
    }
    return true;
}

void MainWindow::configChangeCheck() {
    if (tokenString != computeTokenString() || UIPreferences::mainScreenImageSize != imageSize) {
        addWidgets();
    }
    toolbar.refresh();
}

void MainWindow::locationChange() {
    auto index = comboBox.getIndex();
    Location::setCurrentLocation(index);
    reload();
}

void MainWindow::updateCc() {
    cardCurrentConditions.update(currentConditions);
}

void MainWindow::update7day() {
    sevenDayCollection.update();
}

void MainWindow::updateHazards() {
    cardHazards.removeLabels();
    cardHazards = CardHazards{this, hazards};
    boxHazards.addLayout(cardHazards);
}

void MainWindow::getCc() {
    currentConditions.process(Location::getLatLonCurrent(), 0);
    currentConditions.timeCheck();
}

void MainWindow::get7day() {
    sevenDay.process(Location::getLatLonCurrent());
}

void MainWindow::getHazards() {
    hazards.process(Location::getLatLonCurrent());
}

void MainWindow::addWidgets() {
    imageLayout.removeChildren();
    rightMostLayout.removeChildren();
    imageWidgets.clear();
    textWidgets.clear();
    boxSevereDashboard.removeChildren();
    tokenString = "";
    nexradList.clear();

    if (UIPreferences::nexradMainScreen) {
        nexradList.push_back(
            new NexradWidget{
                this,
                0,
                1,
                true,
                Location::radarSite(),
                UIPreferences::mainScreenImageSize,
                UIPreferences::mainScreenImageSize,
                [] ([[maybe_unused]] int pane, [[maybe_unused]] const string& prod) {},
                [] ([[maybe_unused]] int pane, [[maybe_unused]] const string& sector) {},
                [] ([[maybe_unused]] double z, [[maybe_unused]] int pane) {},
                [] ([[maybe_unused]] double x, [[maybe_unused]] double y, [[maybe_unused]] int pane) {},
                [] {}
            });
        nexradList[0]->setFixedHeight(UIPreferences::mainScreenImageSize);
        nexradList[0]->setFixedWidth(UIPreferences::mainScreenImageSize);
        imageLayout.addWidgetReal(nexradList[0]);
        tokenString += "NEXRAD_MAIN";
    }
    //
    // image setup
    //
    for (const auto& item : UIPreferences::homeScreenItemsImage) {
        if (item.isEnabled()) {
            imageWidgets.insert({item.getPrefToken(), Image{this}});
            const auto tokenFinal = item.getPrefToken();
            imageWidgets.at(item.getPrefToken()).connect([this, tokenFinal] { launchImageScreen(tokenFinal); });
            imageLayout.addWidget(imageWidgets.at(item.getPrefToken()));
            tokenString += item.getPrefToken();
        }
        imageSize = UIPreferences::mainScreenImageSize;
    }
    imageLayout.addStretch();
    //
    // Textual right sidebar (hourly)
    //
    for (const auto& item : UIPreferences::homeScreenItemsText) {
        if (item.isEnabled()) {
            textWidgets.insert({item.getPrefToken(), Text{this}});
            textWidgets.at(item.getPrefToken()).setFixedWidth();
            rightMostLayout.addWidget(textWidgets.at(item.getPrefToken()));
            tokenString += item.getPrefToken();
        }
    }
    rightMostLayout.addStretch();
}

string MainWindow::computeTokenString() {
    string tokenString;
    if (UIPreferences::nexradMainScreen) {
        tokenString += "NEXRAD_MAIN";
    }
    for (const auto& item : UIPreferences::homeScreenItemsImage) {
        if (item.isEnabled()) {
            tokenString += item.getPrefToken();
        }
    }
    for (const auto& item : UIPreferences::homeScreenItemsText) {
        if (item.isEnabled()) {
            tokenString += item.getPrefToken();
        }
    }
    return tokenString;
}

void MainWindow::launchImageScreen(const string& token) {
    if (token == "VISIBLE_SATELLITE") {
        Route::vis(this);
    } else if (token == "RADAR_MOSAIC") {
        toolbar.launchRadarMosaicViewer();
    } else if (token == "ANALYSIS_RADAR_AND_WARNINGS") {
        toolbar.launchNationalImages();
    } else if (token == "USWARN") {
        toolbar.launchUsAlerts();
    } else if (token == "RTMA_TEMP") {
        toolbar.launchRtma();
    } else if (token == "SPC_MESO_MSLP") {
        toolbar.launchSpcMeso("pmsl");
    } else if (token == "SPC_MESO_500MB") {
        toolbar.launchSpcMeso("500mb");
    }
}
