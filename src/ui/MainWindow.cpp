// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "MainWindow.h"
#include <QApplication>
#include <QGridLayout>
#include <QVBoxLayout>
#include "common/GlobalVariables.h"
#include "objects/FutureBytes.h"
#include "objects/FutureText.h"
#include "objects/FutureVoid.h"
#include "objects/PolygonWatch.h"
#include "settings/HomeLayout.h"
#include "util/HomeThumbnails.h"
#include <memory>
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
    , hourlyGraph{this}   // Initialize the hourly weather graph widget
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

    box.addLayout(boxH);
    boxH.addLayout(toolbar);
    boxH.addLayout(boxZones);

    // each large section lives in a holder widget so arrangeColumns can put it in any zone
    severeHolder = new QWidget{this};
    severeHolder->setLayout(boxSevereDashboard.getView());
    imagesHolder = new QWidget{this};
    imagesHolder->setLayout(imageLayout.getView());
    forecastHolder = new QWidget{this};
    forecastHolder->setLayout(forecastLayout.getView());
    textHolder = new QWidget{this};
    textHolder->setLayout(rightMostLayout.getView());

    forecastLayout.addWidget(comboBox);
    forecastLayout.addLayout(boxCc);
    boxCc.addLayout(cardCurrentConditions);
    forecastLayout.addLayout(boxHazards);
    forecastLayout.addLayout(boxSevenDay);
    boxHourlyGraph.addWidget(hourlyGraph);   // Add hourly graph to the forecast layout
    forecastLayout.addLayout(boxHourlyGraph);
    forecastLayout.addStretch();

    addWidgets();   // also places the columns right of the toolbar, in the user's order

    sw.enableMiddleDrag();
    shortcutClose.connect([this] { close(); });
    shortcutVis.connect([this] { Route::vis(this); });
    shortcutWfoText.connect([this] { toolbar.launchWfoText(); });
    shortcutHourly.connect([this] { showHourlyGraph(); });   // Show/hide hourly graph
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
    shortcutRainfallOutlook.connect([this] { toolbar.launchRainfallOutlookSummary(); });
    shortcutRtma.connect([this] { toolbar.launchRtma(); });
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
                const auto token = item.getPrefToken();
                const auto bytes = std::make_shared<QByteArray>();
                new FutureVoid{this,
                    [token, bytes] { *bytes = HomeThumbnails::fetch(token); },
                    [this, token, bytes] {
                        const auto found = imageWidgets.find(token);   // the layout may have been rebuilt meanwhile
                        const auto * entry = HomeThumbnails::find(token);
                        if (found != imageWidgets.end() && !bytes->isEmpty()) {
                            found->second.setToWidth(*bytes, UIPreferences::mainScreenImageSize, entry != nullptr && entry->white);
                        }
                    }};
            }
        }
        if (UIPreferences::nexradMainScreen) {
            const auto pane = 0;
            nexradList[pane]->nexradState.setRadar(Location::radarSite());
            nexradList[pane]->nexradState.reset();
            nexradList[pane]->nexradState.zoom = 0.6;
            nexradList[pane]->nexradDraw.initGeom();

            for (auto nw : nexradList) {
                nw->runJob([nw] { nw->downloadData(); }, [nw] { nw->update(); });
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
        nexradList[0]->onClick = [this] { toolbar.launchNexrad(1); };   // click the home screen radar to open the radar window
        nexradList[0]->setFixedHeight(UIPreferences::mainScreenImageSize);
        nexradList[0]->setFixedWidth(UIPreferences::mainScreenImageSize);
    }
    //
    // image setup
    //
    for (const auto& token : UIPreferences::homeScreenImageOrder.getTokens()) {
        if (token == UIPreferences::homeScreenNexradToken) {
            if (!nexradList.empty()) {
                imageLayout.addWidgetReal(nexradList[0]);
            }
            continue;
        }
        for (const auto& item : UIPreferences::homeScreenItemsImage) {
            if (item.getPrefToken() == token && item.isEnabled()) {
                imageWidgets.insert({token, Image{this}});
                imageWidgets.at(token).connect([this, token] { launchImageScreen(token); });
                imageLayout.addWidget(imageWidgets.at(token));
            }
        }
    }
    imageSize = UIPreferences::mainScreenImageSize;
    imageLayout.addStretch();
    //
    // Textual right sidebar (hourly)
    //
    for (const auto& token : UIPreferences::homeScreenTextOrder.getTokens()) {
        for (const auto& item : UIPreferences::homeScreenItemsText) {
            if (item.getPrefToken() == token && item.isEnabled()) {
                textWidgets.insert({token, Text{this}});
                textWidgets.at(token).setFixedWidth();
                rightMostLayout.addWidget(textWidgets.at(token));
            }
        }
    }
    rightMostLayout.addStretch();
    tokenString = computeTokenString();
    arrangeColumns();
}

// (re)builds the zone grid right of the toolbar from the template and assignments saved by HomeLayout (Settings >
// Home Screen Order): each zone stacks its sections top to bottom. The section holders are moved, not rebuilt, so
// their contents are kept.
void MainWindow::arrangeColumns() {
    const auto holderFor = [this] (const string& section) -> QWidget * {
        if (section == HomeLayout::sectionSevere) {
            return severeHolder;
        }
        if (section == HomeLayout::sectionImages) {
            return imagesHolder;
        }
        if (section == HomeLayout::sectionForecast) {
            return forecastHolder;
        }
        return textHolder;
    };
    for (const auto& section : HomeLayout::sections()) {
        holderFor(section)->setParent(this);   // out of the old grid before it is deleted
    }
    if (zonesWidget != nullptr) {
        boxZones.getView()->removeWidget(zonesWidget);
        delete zonesWidget;
    }
    zonesWidget = new QWidget{this};
    auto * grid = new QGridLayout{zonesWidget};
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setSpacing(UIPreferences::boxPadding * 4);
    const auto& layoutTemplate = HomeLayout::templates()[HomeLayout::templateIndex()];
    for (size_t column = 0; column < layoutTemplate.columnStretch.size(); column += 1) {
        grid->setColumnStretch(static_cast<int>(column), layoutTemplate.columnStretch[column]);
    }
    grid->setRowStretch(layoutTemplate.rows, 1);   // free height goes below the zones, not between them
    for (size_t zone = 0; zone < layoutTemplate.zones.size(); zone += 1) {
        const auto& place = layoutTemplate.zones[zone];
        auto * zoneWidget = new QWidget{zonesWidget};
        auto * stack = new QVBoxLayout{zoneWidget};
        stack->setContentsMargins(0, 0, 0, 0);
        stack->setSpacing(UIPreferences::boxPadding * 4);
        for (const auto& section : HomeLayout::sectionsIn(static_cast<int>(zone))) {
            stack->addWidget(holderFor(section), 0, Qt::AlignTop);
            holderFor(section)->show();
        }
        stack->addStretch();
        grid->addWidget(zoneWidget, place.row, place.col, place.rowSpan, place.colSpan, Qt::AlignTop);
    }
    boxZones.addWidgetReal(zonesWidget, 0, Qt::AlignTop | Qt::AlignLeft);
}

string MainWindow::computeTokenString() {
    string tokenString;
    tokenString += HomeLayout::signature() + ",";
    for (const auto& token : UIPreferences::homeScreenImageOrder.getTokens()) {
        if (token == UIPreferences::homeScreenNexradToken) {
            if (UIPreferences::nexradMainScreen) {
                tokenString += token + ",";
            }
            continue;
        }
        for (const auto& item : UIPreferences::homeScreenItemsImage) {
            if (item.getPrefToken() == token && item.isEnabled()) {
                tokenString += token + ",";
            }
        }
    }
    for (const auto& token : UIPreferences::homeScreenTextOrder.getTokens()) {
        for (const auto& item : UIPreferences::homeScreenItemsText) {
            if (item.getPrefToken() == token && item.isEnabled()) {
                tokenString += token + ",";
            }
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
    } else if (token == "SPC_SOUNDING") {
        Route::spcSoundingBySector(this, "");   // the native sounding viewer, at the nearest site
    } else if (token == "SPC_MESO_MSLP") {
        toolbar.launchSpcMeso("pmsl");
    } else if (token == "SPC_MESO_500MB") {
        toolbar.launchSpcMeso("500mb");
    } else if (const auto * entry = HomeThumbnails::find(token); entry != nullptr && !entry->routeId.empty()) {
        toolbar.launchRoute(entry->routeId);   // any other thumbnail opens its own tool
    }
}
