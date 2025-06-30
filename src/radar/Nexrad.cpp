// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "Nexrad.h"
#include <QKeySequence>
#include <algorithm>
#include <cmath>
#include "common/GlobalArrays.h"
#include "common/GlobalVariables.h"
#include "objects/FutureVoid.h"
#include "objects/WString.h"
#include "misc/TextViewerStatic.h"
#include "objects/ObjectDateTime.h"
#include "radar/NexradUtil.h"
#include "radar/RadarSites.h"
#include "settings/RadarPreferences.h"
#include "settings/UIPreferences.h"
#include "settings/SettingsMain.h"
#include "util/Utility.h"
#include "util/UtilityList.h"
#include "util/UtilityLog.h"
#include "util/UtilityUI.h"

Nexrad::Nexrad(Window * parent, int numberOfPanes, bool useASpecificRadar, const string& radarSite)
    : Window{parent}
    , useASpecificRadar{useASpecificRadar}
    , comboboxSector{this, RadarSites::radars()}
    , comboboxProduct{this, NexradUtil::radarProductList}
    , comboboxTilt{this, {"0", "1", "2", "3"}}
    , comboboxAnimCount{this, {"5", "10", "15", "20", "25", "30", "40", "50"}}
    , comboboxAnimSpeed{this, {"1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16", "17", "18", "19", "20"}}
    , autoUpdate{this, "RADAR_DATA_REFRESH_INTERVAL", 3, [this] { downloadData(); }}
    , settingsButton{this, Settings, "Settings ctrl-p"}
    , moveLeftButton{this, Left, "Move left ctrl- <-"}
    , moveRightButton{this, Right, "Move right ctrl- ->"}
    , moveDownButton{this, Down, "Move down ctrl- downArrow"}
    , moveUpButton{this, Up, "Move up ctrl- upArrow"}
    , zoomOutButton{this, Minus, "Zoom out ctrl- -"}
    , zoomInButton{this, Plus, "Zoom in ctrl- +"}
    , textFrameCount{this, "Frame Count:"}
    , textTilt{this, "Tilt:"}
    , textAnimSpeed{this, "Anim Speed:"}
    , nexradLayerDownload{this, &nexradList}
    , objectAnimateNexrad{this, &nexradList, &comboboxAnimCount, &comboboxAnimSpeed}
    , shortcutReload{QKeySequence{"U"}, this}
    , shortcutU{QKeySequence{"V"}, this}
    , shortcutQ{QKeySequence{"R"}, this}
    , shortcutL{QKeySequence{"L"}, this}
    , shortcutT{QKeySequence{"T"}, this}
    , shortcutC{QKeySequence{"C"}, this}
    , shortcutAnimate{QKeySequence{"A"}, this}
    , shortcutZoomIn{Qt::Key_Equal, this}  // was Shortcut{Qt::CTRL | Qt::Key_Equal, this)
    , shortcutZoomOut{Qt::Key_Minus, this}
    , shortcutMoveLeft{Qt::Key_Left, this}
    , shortcutMoveRight{Qt::Key_Right, this}
    , shortcutMoveUp{Qt::Key_Up, this}
    , shortcutMoveDown{Qt::Key_Down, this}
    , shortcutMoveLeft2{QKeySequence{"J"}, this}
    , shortcutMoveRight2{QKeySequence{"K"}, this}
    , shortcutMoveUp2{QKeySequence{"M"}, this}
    , shortcutMoveDown2{QKeySequence{"N"}, this}
    , shortcutKeyboard{QKeySequence{"/"}, this}
    , shortcutSettings{QKeySequence{"P"}, this}
{
    setAttribute(Qt::WA_DeleteOnClose);
    //
    // Determine dimensions
    //
    const auto dimens = UtilityUI::getScreenBounds();
    auto widthW = dimens[0];
    auto heightW = dimens[1];
    auto dimen = std::max(heightW, widthW);
    if (!UIPreferences::tiledWindows) {
        setSize2(widthW, heightW - 15);
    } else {
        setSize2(static_cast<int>(widthW / 2), static_cast<int>(heightW / 2 - 30));
        widthW = static_cast<int>(widthW / 2.0);
        heightW = static_cast<int>(heightW / 2.0);
    }

    if (numberOfPanes == 4) {
        dimen = static_cast<int>(std::round(dimen / 2.0));
    }

    // Emit updates every 10 seconds if available
    // source = QGeoPositionInfoSource::createDefaultSource(this);
    // if (RadarPreferences::locdotFollowsGps) {
    //     if (source) {
    //         qDebug() << "emit every 10 sec";
    //         source->setUpdateInterval(10000);
    //         // source->positionUpdated.connect(self.positionUpdated);
    //         QObject::connect(source, &QGeoPositionInfoSource::positionUpdated, this, [this] (const QGeoPositionInfo& g) { positionUpdated(g); });
    //         source->startUpdates();
    //     } else {
    //         qDebug() << "failed to setup location";
    //     }
    // }

    for (auto index : range(numberOfPanes)) {
        nexradList.push_back(
            new NexradWidget{
                this,
                static_cast<int>(index),
                numberOfPanes,
                useASpecificRadar,
                radarSite,
                widthW,
                heightW,
                [this] (int pane, const auto& prod) { changeProductFromChild(pane, prod); },
                [this] (int pane, const auto& sector) { changeSectorFromChild(pane, sector); },
                [this] (double z, int pane) { changeZoom(z, pane); },
                [this] (double x, double y, int pane) { changePosition(x, y, pane); },
                [this] { setTitleMain(); }
            });
        if (!UIPreferences::tiledWindows) {
            nexradList.back()->setFixedHeight(dimen);
            nexradList.back()->setFixedWidth(dimen);
        } else {
            nexradList.back()->setFixedHeight(dimen / 2.0);
            nexradList.back()->setFixedWidth(dimen / 2.0);
        }
    }

    settingsButton.connect([this] {
        new SettingsMain{this, [this] { settingsCheck(); }, true, true};
    });
    //
    // sector menu
    //
    comboboxSector.setIndexByValue(nexradList[0]->nexradState.getRadarSite());
    comboboxSector.connect([this] { changeRadarSite(); });
    //
    // product menu
    //
    comboboxProduct.setIndexByValue(nexradList[0]->nexradState.getRadarProduct());
    comboboxProduct.connect([this] { changeProduct(); });

    comboboxAnimCount.setIndex(Utility::readPrefInt("NEXRAD_ANIM_FRAME_COUNT2", 1));
    comboboxAnimSpeed.setIndex(Utility::readPrefInt("ANIM_INTERVAL", 5));

    comboboxTilt.setIndex(nexradList[0]->nexradState.tiltInt);
    comboboxTilt.connect([this] { changeTilt(); });

    comboboxAnimCount.connect([this] { objectAnimateNexrad.setAnimationCount(); });
    comboboxAnimSpeed.connect([this] { objectAnimateNexrad.setAnimationSpeed(); });

    moveLeftButton.connect([this] { moveLeft(); });
    moveRightButton.connect([this] { moveRight(); });
    moveDownButton.connect([this] { moveDown(); });
    moveUpButton.connect([this] { moveUp(); });
    zoomOutButton.connect([this] { zoomOut(); });
    zoomInButton.connect([this] { zoomIn(); });

    boxH.addWidget(settingsButton);
    boxH.addWidget(comboboxSector);
    boxH.addWidget(comboboxProduct);
    for (auto nw : nexradList) {
        boxH.addWidget(*nw->radarStatusBox);
    }
    boxH.addWidget(moveLeftButton);
    boxH.addWidget(moveRightButton);
    boxH.addWidget(moveUpButton);
    boxH.addWidget(moveDownButton);
    boxH.addWidget(zoomOutButton);
    boxH.addWidget(zoomInButton);
    boxH.addWidget(autoUpdate);
    boxH.addWidget(objectAnimateNexrad);
    if (!UIPreferences::tiledWindows && numberOfPanes == 1) {
        boxH.addWidget(textFrameCount, 0, Qt::AlignCenter);
    } else {
        textFrameCount.setVisible(false);
    }
    boxH.addWidget(comboboxAnimCount);
    if (!UIPreferences::tiledWindows && numberOfPanes == 1) {
        boxH.addWidget(textTilt, 0, Qt::AlignCenter);
    } else {
        textTilt.setVisible(false);
    }
    boxH.addWidget(comboboxTilt);
    if (!UIPreferences::tiledWindows && numberOfPanes == 1) {
        boxH.addWidget(textAnimSpeed, 0, Qt::AlignCenter);
    } else {
        textAnimSpeed.setVisible(false);
    }
    boxH.addWidget(comboboxAnimSpeed);
    box.addLayout(boxH);

    box.addLayout(nexradBox);
    nexradBox.addWidgetReal(nexradList[0]);
    if (numberOfPanes > 1) {
        nexradBox.addWidgetReal(nexradList[1]);
    }
    if (numberOfPanes == 4) {
        box.addLayout(nexradBox2);
        nexradBox2.addWidgetReal(nexradList[2]);
        nexradBox2.addWidgetReal(nexradList[3]);
    }
    box.setSpacing(0);
    boxH.setSpacing(2);
    nexradBox.setSpacing(0);
    nexradBox2.setSpacing(0);
    box.getAndShow(this);

    adjustControls();
    adjustProductComboBox();
    downloadData();

    shortcutReload.connect([this] { autoUpdate.toggleAutoUpdate(); });
    shortcutU.connect([this] { changeProductFromChild(0, "N0U"); });
    shortcutQ.connect([this] { changeProductFromChild(0, "N0Q"); });
    shortcutL.connect([this] { changeProductFromChild(0, "DVL"); });
    shortcutT.connect([this] { changeProductFromChild(0, "EET"); });
    shortcutC.connect([this] { changeProductFromChild(0, "N0C"); });
    shortcutAnimate.connect([this] {
        objectAnimateNexrad.animateClicked();
        // objectAnimateNexrad.setActive();
    });
    shortcutZoomIn.connect([this] { zoomIn(); });
    shortcutZoomOut.connect([this] { zoomOut(); });

    shortcutMoveLeft.connect([this] { moveLeft(); });
    shortcutMoveRight.connect([this] { moveRight(); });
    shortcutMoveUp.connect([this] { moveUp(); });
    shortcutMoveDown.connect([this] { moveDown(); });

    shortcutMoveLeft2.connect([this] { moveLeft(); });
    shortcutMoveRight2.connect([this] { moveRight(); });
    shortcutMoveUp2.connect([this] { moveUp(); });
    shortcutMoveDown2.connect([this] { moveDown(); });

    shortcutKeyboard.connect([this] { new TextViewerStatic{this, GlobalVariables::nexradShortcuts, "Shortcuts", 700, 600}; });
    shortcutSettings.connect([this] { new SettingsMain{this, [this] { settingsCheck(); }, true, true}; });

}

Nexrad::~Nexrad() {
    for (auto nw : nexradList) {
        delete nw;
    }
}

void Nexrad::syncRadarSite(const string& radarSite, int pane, bool resetZoom) {
    objectAnimateNexrad.stopAnimateNoDownload();
    adjustControls();
    if (RadarPreferences::dualpaneshareposn) {
        for (auto nw : nexradList) {
            nw->nexradState.setRadar(radarSite);
            if (resetZoom) {
                nw->nexradState.reset();
            }
            nw->nexradDraw.initGeom();
        }
    } else {
        nexradList[pane]->nexradState.setRadar(radarSite);
        if (resetZoom) {
            nexradList[pane]->nexradState.reset();
        }
        nexradList[pane]->nexradDraw.initGeom();
    }
    adjustProductComboBox();
}

void Nexrad::adjustControls() {
    if (!RadarPreferences::showControls || UIPreferences::tiledWindows) {
        moveLeftButton.setVisible(false);
        moveRightButton.setVisible(false);
        moveUpButton.setVisible(false);
        moveDownButton.setVisible(false);
        zoomOutButton.setVisible(false);
        zoomInButton.setVisible(false);
    } else {
        moveLeftButton.setVisible(true);
        moveRightButton.setVisible(true);
        moveUpButton.setVisible(true);
        moveDownButton.setVisible(true);
        zoomOutButton.setVisible(true);
        zoomInButton.setVisible(true);
    }
}

void Nexrad::downloadData() {
    save();
    objectAnimateNexrad.stopAnimateNoDownload();
    for (auto nw : nexradList) {
        new FutureVoid{this, [nw] { nw->downloadData(); }, [nw] { nw->draw(); }};
    }
    comboboxSector.block();
    comboboxSector.setIndex(RadarSites::findRadarIndex(nexradList[0]->nexradState.getRadarSite()));
    comboboxSector.unblock();
    nexradLayerDownload.downloadLayers();
}

void Nexrad::setTitleMain() {
    setTitle(radarInfoForTitle() + autoUpdate.titleAdd);
}

string Nexrad::radarInfoForTitle() {
    const auto nexradStatusAsString = nexradList[0]->levelData.radarInfo;
    const auto nexradStatus = WString::split(nexradStatusAsString, " ");
    if (nexradStatus.size() > 3) {
        return nexradStatusAsString;
    } else {
        return "UNKNOWN";
    }
}

void Nexrad::changeProduct() {
    objectAnimateNexrad.stopAnimateNoDownload();
    nexradList[0]->nexradState.setRadarProduct(comboboxProduct.getValue());
    nexradLayerDownload.downloadLayers();
    if (RadarPreferences::colorLegend) {
        nexradList[0]->colorLegend.update(nexradList[0]->nexradState.getRadarProduct());
    }
    nexradList[0]->changeProduct();
}

void Nexrad::changeProductFromChild(int currentIndex, const string &product) {
    objectAnimateNexrad.stopAnimateNoDownload();
    nexradList[currentIndex]->nexradState.setRadarProduct(product);
    nexradLayerDownload.downloadLayers();
    if (RadarPreferences::colorLegend) {
        nexradList[currentIndex]->colorLegend.update(product);
    }
    if (currentIndex == 0) {
        adjustProductComboBox();
    }
    nexradList[currentIndex]->changeProduct();
}

void Nexrad::changeSectorFromChild(int paneNumber, const string& radarSite) {
    const auto sectorIndex = RadarSites::findRadarIndex(radarSite);
    comboboxSector.block();
    comboboxSector.setIndex(sectorIndex);
    comboboxSector.unblock();
    syncRadarSite(radarSite, paneNumber, true);
    downloadData();
}

void Nexrad::changeRadarSite() {
    objectAnimateNexrad.stopAnimateNoDownload();
    const auto site = comboboxSector.getValue();
    const auto radarSite = WString::split(site, ":")[0];
    syncRadarSite(radarSite, 0, true);
    downloadData();
}

void Nexrad::changeTilt() {
    const auto tilt = comboboxTilt.getIndex();
    if (tilt != nexradList[0]->nexradState.tiltInt) {
        for (auto nw : nexradList) {
            nw->nexradState.tiltInt = tilt;
        }
        downloadData();
    }
}

void Nexrad::adjustProductComboBox() {
    comboboxProduct.block();
    if (nexradList[0]->nexradState.isTdwrSite() && !nexradList[0]->nexradState.isTdwrProduct()) {
        nexradList[0]->nexradState.setRadarProduct("TZL");
    } else if (!nexradList[0]->nexradState.isTdwrSite() && nexradList[0]->nexradState.isTdwrProduct()) {
        nexradList[0]->nexradState.setRadarProduct("N0Q");
    }
    auto index = 0;
    if (nexradList[0]->nexradState.isTdwrProduct()) {
        comboboxProduct.setList(NexradUtil::radarProductListTdwr);
        index = findex(nexradList[0]->nexradState.getRadarProduct(), NexradUtil::radarProductListTdwr);
    } else {
        comboboxProduct.setList(NexradUtil::radarProductList);
        index = findex(nexradList[0]->nexradState.getRadarProduct(), NexradUtil::radarProductList);
    }
    comboboxProduct.setIndex(index);
    comboboxProduct.unblock();
}

void Nexrad::changeZoom(double changeAmount, int paneIndex) {
    if ((nexradList[paneIndex]->nexradState.zoom > 0.02 && changeAmount < 0.99) || changeAmount > 1.0) {
        const auto factor = changeAmount;
        if (RadarPreferences::dualpaneshareposn) {
            for (auto nw : nexradList)
            {
                const auto oldZoom = nw->nexradState.zoom;
                nw->nexradState.zoom *= factor;
                const auto newZoom = nw->nexradState.zoom;
                const auto zoomDifference = newZoom / oldZoom;
                nw->nexradState.xPos *= zoomDifference;
                nw->nexradState.yPos *= zoomDifference;
                nw->resizePolygons();
                // nw->nexradRenderTextObject.add();
            }
        }
        else {
            const auto oldZoom = nexradList[paneIndex]->nexradState.zoom;
            nexradList[paneIndex]->nexradState.zoom *= factor;
            const auto newZoom = nexradList[paneIndex]->nexradState.zoom;
            const auto zoomDifference = newZoom / oldZoom;
            nexradList[paneIndex]->nexradState.xPos *= zoomDifference;
            nexradList[paneIndex]->nexradState.yPos *= zoomDifference;
            nexradList[paneIndex]->resizePolygons();
            // nexradList[paneIndex]->nexradRenderTextObject.add();
        }
        drawAndSave();
    }
}

void Nexrad::changePosition(double x, double y, int paneIndex) {
    if (RadarPreferences::dualpaneshareposn) {
        for (auto nw : nexradList) {
            nw->nexradState.xPos += x;
            nw->nexradState.yPos += y;
        }
    } else {
        nexradList[paneIndex]->nexradState.xPos += x;
        nexradList[paneIndex]->nexradState.yPos += y;
    }
    drawAndSave();
}

void Nexrad::zoomOut() {
    changeZoom(0.77, 0);
}

void Nexrad::zoomIn() {
    changeZoom(1.33, 0);
}

void Nexrad::moveLeft() {
    changePosition(moveIncrement, 0.0, 0);
}

void Nexrad::moveRight() {
    changePosition(-1.0 * moveIncrement, 0.0, 0);
}

void Nexrad::moveUp() {
    changePosition(0.0, moveIncrement, 0);
}

void Nexrad::moveDown() {
    changePosition(0.0, -1.0 * moveIncrement, 0);
}

void Nexrad::drawAndSave() {
    for (auto nw : nexradList) {
        nw->update();
        nw->nexradRenderTextObject.add();
        nw->nexradState.writePreferences();
    }
}

void Nexrad::save() {
    for (auto nw : nexradList) {
        nw->nexradState.writePreferences();
    }
}

// KEEP
// void Nexrad::positionUpdated(const QGeoPositionInfo& info) {
//     auto coord = info.coordinate();
//     auto lat = coord.latitude();
//     auto lon = coord.longitude();
//     qDebug() << "updated GPS: " << lat << " " << lon;
//     for (auto nw : nexradList) {
//         nw->updateGps(lat, lon);
//         nw->update();
//     }
// }

void Nexrad::settingsCheck() {
    for (auto nw : nexradList) {
        nw->nexradDraw.initGeom();
        nw->nexradRenderTextObject.initialize();
    }
    syncRadarSite(nexradList[0]->nexradState.getRadarSite(), 0, false);
    adjustColorLegends();
    // if auto update is on, toggle it in case of refresh interval changes
    // this will force an update as well
    if (autoUpdate.isActive()) {
        autoUpdate.restart();
    } else {
        downloadData();
    }
}

void Nexrad::adjustColorLegends() {
    // # TODO FIXME
    // # if RadarPreferences.colorLegend and len(self.colorLegends) == 0:
    // #     for nw in self.nexradList:
    // #         self.colorLegends.append(NexradColorLegend(nw.nexradState.getRadarProduct()))
    // #     self.nexradBox.addWidgetFirst(self.colorLegends[-1].get())
    // # if not RadarPreferences.colorLegend and len(self.colorLegends) > 0:
    // #     for cl in self.colorLegends:
    // #         cl.setVisible(False)
}

void Nexrad::closeEventCustom() {
    if (!useASpecificRadar) {
        save();
    }
    objectAnimateNexrad.stopAnimateNoDownload();
    autoUpdate.stopNoDownload();
}
