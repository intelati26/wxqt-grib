// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "ModelViewer.h"
#include "spcrefs/SpcRefsLoader.h"
#include "ui/ActivityLabel.h"
#include <QBuffer>
#include <QTimeZone>
#include <QFile>
#include <QImage>
#include <QInputDialog>
#include <QPainter>
#include <QMenu>
#include "misc/ImageViewer.h"
#include "objects/UtilityAnimationExport.h"
#include "objects/NetManager.h"
#include <QDateTimeEdit>
#include <QSplitter>
#include <QPushButton>
#include "models/CamsViewer.h"
#include "mapkit/MasterMapViewer.h"
#include "models/ChartBuilder.h"
#include <algorithm>
#include <map>
#include <memory>
#include "gfs/GfsModels.h"
#include "gfs/ChartHours.h"
#include "hurricane/HafsIntensity.h"
#include "gfs/GfsRender.h"
#include "settings/Location.h"
#include "models/ObjectModelGet.h"
#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QVBoxLayout>
#include "models/PaletteEditor.h"
#include "settings/UIPreferences.h"
#include "models/PaletteStore.h"
#include "models/RefsPointGraph.h"
#include "models/UtilityModels.h"
#include "objects/FutureBytes.h"
#include "objects/FutureVoid.h"
#include "gfs/GfsChart.h"
#include "cfs/CfsPlumeViewer.h"
#include "history/HistoryCursor.h"
#include "models/HodographGlyph.h"
#include "models/SoundingViewer.h"
#include "objects/NetManager.h"
#include "sounding/UtilityBufkit.h"
#include "util/NetPriority.h"
#include "objects/WString.h"
#include "util/Utility.h"
#include "util/UtilityList.h"
#include "util/UtilityString.h"
#include "ui/UiStandards.h"

namespace {
    std::string sectorLabel(const std::string& id);   // defined with the sector picker below
}

ModelViewer::ModelViewer(Window * parent, const string& modelType)
    : Window{parent}
    , image{this}
    , objectModel{modelType}
    , comboboxRun{this}
    , comboboxModel{this, objectModel.models}
    , comboboxSector{this, objectModel.sectors}
    , comboboxProduct{this, objectModel.paramLabels}
    , comboboxTime{this, objectModel.times}
    , backForward{this, [this] { moveBack(); }, [this] { moveForward(); }}
    , buttonProducts{this, None, "Charts..."}
    , comboPreload{this, {"Preload: automatic", "Preload: nearby hours only", "Preload: next 12 hours", "Preload: next 24 hours", "Preload: the whole run"}}
    , comboCompare{this, {"Plain chart", "Change since run 6 h earlier", "Change since run 12 h earlier", "Change since run 24 h earlier"}}
    , comboTiles{this, {"One chart", "Two charts (1 x 2)", "Three charts (1 x 3)", "Four charts (2 x 2)"}}
    , comboTilesShow{this, {"Compare: charts", "Compare: models (same hour)", "Compare: runs", "Compare: members", "Compare: models (same valid time)", "Compare: ensemble parts"}}
    , comboMember{this, {"Ensemble mean"}}
    , soundingPick{this, [this] { return shownProbe ? shownProbe->validUtc : QDateTime{}; }, "Model screen"}
    , buttonSector{this, None, "Area..."}
    , buttonModel{this, None, "Model..."}
{
    comboboxModel.setIndexByValue(objectModel.model);
    comboboxModel.connect([this] { changeModelCb(); });
    comboboxRun.connect([this] { changeRunCb(); });

    comboboxSector.setIndexByValue(objectModel.sector);
    comboboxSector.connect([this] { changeSectorCb(); });

    comboboxProduct.setIndexByValue(objectModel.param);
    comboboxProduct.connect([this] { changeProductCb(); });

    comboboxTime.connect([this] { changeTimeCb(); });

    boxH.addWidget(comboboxModel);
    boxH.addWidget(buttonModel);
    buttonModel.connect([this] { showModelPicker(); });
    boxH.addWidget(comboboxRun);
    {   // any time of the past: its date and clock (UTC) open the run at or before it, at the hour that lands on it
        auto * holder = new QWidget{this};
        auto * row = new QHBoxLayout{holder};
        row->setContentsMargins(0, 0, 0, 0);
        timePick = new QDateTimeEdit{QDateTime::currentDateTimeUtc(), holder};
        timePick->setDisplayFormat("yyyy-MM-dd HH:mm 'Z'");
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
        timePick->setTimeZone(QTimeZone::utc());
#else
        timePick->setTimeSpec(Qt::UTC);   // (Qt 6.4 has no setTimeZone on the date box)
#endif
        timePick->setCalendarPopup(true);
        timePick->setToolTip("A day and time (UTC): Go opens the run at or before it, at the hour that is that time. The other open history screens follow");
        auto * go = UiStandards::button(holder, "Go");
        go->setFixedWidth(40);
        pickKind = UiStandards::combo(holder);
        pickKind->addItem("valid time", 0);
        pickKind->addItem("run time", 1);
        pickKind->setToolTip("Valid time: the run at or before the time, at the hour that is that time. Run time: the run (cycle) at or before the time, at its first hour, to step through from there");
        row->addWidget(pickKind);
        row->addWidget(timePick);
        row->addWidget(go);
        boxH.addWidgetReal(holder);
        QObject::connect(go, &QPushButton::clicked, this, [this] {
            const auto at = timePick->dateTime().toUTC();
            openAt(objectModel.model, at, pickKind->currentData().toInt() == 1);
            HistoryCursor::get().setTime(pinnedValid.isValid() ? pinnedValid : at, this);
        });
    }
    boxH.addWidget(comboboxSector);
    boxH.addWidget(buttonSector);
    buttonSector.connect([this] { showSectorPicker(); });
    boxH.addWidget(comboboxProduct);
    boxH.addWidget(buttonProducts);
    buttonProducts.connect([this] { showPicker(); });
    boxH.addWidget(comboboxTime);
    {   // a flow holds widgets, not layouts
        auto * stepper = new QWidget{this};
        stepper->setLayout(backForward.getView());
        boxH.addWidgetReal(stepper);
    }
    {   // the models that are fetched as pictures, in a second menu beside the GRIB ones: each opens its own screen
        auto * more = UiStandards::button(this, "Image models  \xE2\x96\xBE", "Image models (some pending)");
        auto * menu = new QMenu{more};
        const auto add = [this, menu] (const QString& text, std::function<void()> open) {
            QObject::connect(menu->addAction(text), &QAction::triggered, this, [open] { open(); });
        };
        Window * opener = parent;
        // a page of the model screen changes the page; on its own it opens a window
        const auto screen = [this, opener] (const string& type) { return [this, opener, type] { openSource ? openSource(type) : (void) new ModelViewer{opener ? opener : this, type}; }; };
        add("NSSL CAMs (MPAS, WRF, HRRR, RRFS)", [this, opener] { openSource ? openSource("CAMS") : (void) new CamsViewer{opener ? opener : this}; });
        menu->addSeparator();
        menu->addSection("Pending discontinuation (pictures from MAG / SPC)");
        add("NSSL WRF (WRF, FV3, HRRRv3)", screen("NSSLWRF"));
        add("SPC HRRR", screen("SPCHRRR"));
        add("SPC HREF", screen("SPCHREF"));
        add("WPC GEFS", screen("WPCGEFS"));
        more->setMenu(menu);
        more->setVisible(!isEmbedded());   // in the model screen its Source box offers them
        boxH.addWidgetReal(more);
    }
    box.addLayout(boxH);
    {   // the second row: compare, maxima, saved views, sounding, save
        comboCompare.getView()->setToolTip("Show how the chart changed since an earlier run: the same valid time from the run 6, 12 or 24 hours older is subtracted (blue = lower now, red = higher now). Charts of lines only have none.");
        comboCompare.connect([this] {
            GfsRender::Variant v;
            const auto i = comboCompare.getIndex();
            if (i > 0) {
                v.kind = GfsRender::Variant::Kind::Change;
                v.hoursBack = i * 6 + (i == 3 ? 6 : 0);   // 6, 12, 24
            }
            setVariant(v);
        });
        boxH2.addWidget(comboCompare);
        comboMember.getView()->setToolTip("An ensemble's charts for one member instead of the mean: the GEFS control and its 30 members, the 5 REFS members. Charts that are made from all the members (spread, chances, percentiles) have no single member.");
        comboMember.connect([this] {
            const auto i = static_cast<size_t>(std::max(comboMember.getIndex(), 0));
            member = i < memberValues.size() ? memberValues[i] : string{};
            startPlaying(false);
            strip->stop();
            reload();
        });
        boxH2.addWidget(comboMember);
        comboMember.setVisible(false);
        buttonPlace = UiStandards::button(this, "Mark the place");
        buttonPlace->setCheckable(true);
        buttonPlace->setChecked(Utility::readPref("MODEL_PLACE_MARK", "true").compare(0, 1, "t") == 0);
        buttonPlace->setVisible(false);
        buttonPlace->setToolTip("The ring and the name of the saved location an area is centered on. Off leaves the map clean, in the window and in what is saved or exported.");
        QObject::connect(buttonPlace, &QPushButton::toggled, this, [this] (bool on) {
            Utility::writePref("MODEL_PLACE_MARK", on ? "true" : "false");
            frames.clear();   // every picture drawn so far has the mark (or has not)
            frameOrder.clear();
            frameBytes = 0;
            failedAhead.clear();
            refreshLoaded();
            reload();
        });
        boxH2.addWidgetReal(buttonPlace);
        buttonOnMap = UiStandards::button(this, "On the map...");
        buttonOnMap->setToolTip("This chart on the master map (scroll and zoom it, put radar, satellite, storms or rivers with it): the same model, chart, area and valid time");
        QObject::connect(buttonOnMap, &QPushButton::clicked, this, [this] {
            if (GfsRender::drawsModel(objectModel.model)) {
                MasterMapViewer::openModels(this, objectModel.model, objectModel.param, objectModel.sector, shownProbe ? shownProbe->validUtc : QDateTime{});
            }
        });
        boxH2.addWidgetReal(buttonOnMap);
        // the hurricane models (HAFS-A, HAFS-B): the wind field of the chart, and the intensity forecast of the storm
        checkRadii = new QCheckBox{"Wind field (34 / 50 / 64 kt)", this};
        checkRadii->setChecked(Utility::readPref("HAFS_RADII", "true").compare(0, 1, "t") == 0);
        checkRadii->setVisible(false);
        checkRadii->setToolTip("The model's own wind radii (34, 50 and 64 kt, by quadrant) round the storm on the chart.");
        QObject::connect(checkRadii, &QCheckBox::toggled, this, [this] (bool on) {
            Utility::writePref("HAFS_RADII", on ? "true" : "false");
            frames.clear();   // every picture drawn so far has the field (or has not)
            frameOrder.clear();
            frameBytes = 0;
            failedAhead.clear();
            refreshLoaded();
            reload();
        });
        boxH2.addWidgetReal(checkRadii);
        buttonIntensity = UiStandards::button(this, "Intensity...");
        buttonIntensity->setVisible(false);
        buttonIntensity->setToolTip("The forecast maximum wind and minimum pressure of this storm against forecast hour, from HAFS-A and HAFS-B together.");
        QObject::connect(buttonIntensity, &QPushButton::clicked, this, [this] {
            if (Hafs::isStormId(objectModel.sector)) {
                HafsIntensityViewer::open(this, objectModel.sector, [this] (const string& text) { setTitle(text.empty() ? objectModel.model + " " + sectorLabel(objectModel.sector) : text); });
            }
        });
        boxH2.addWidgetReal(buttonIntensity);
        buttonStations = UiStandards::button(this, "Stations");
        buttonStations->setCheckable(true);
        buttonStations->setChecked(Utility::readPref("MODEL_BUFKIT_STATIONS", "false").compare(0, 1, "t") == 0);
        buttonStations->setToolTip("A button on the map at each station of the BUFKIT model soundings (airports and sounding sites): click one for that station's RRFS sounding, every forecast hour "
                                   "from one small file (about 0.4 MB). The buttons are not part of the map, so saved and exported pictures do not have them.");
        QObject::connect(buttonStations, &QPushButton::toggled, this, [this] (bool on) {
            Utility::writePref("MODEL_BUFKIT_STATIONS", on ? "true" : "false");
            stationPinKey.clear();
            refreshStationPins();
        });
        image.setSaveMenu(false);   // the Save button here is the one that adds the glyphs and names the file
        QObject::connect(&image, &ZoomImage::pinClicked, this, [this] (const QString& id) {
            new SoundingViewer{this, "RRFS", id.toStdString(), true};
        });
        boxH2.addWidgetReal(buttonStations);
        buttonHodos = UiStandards::button(this, "Hodographs");
        buttonHodos->setCheckable(true);
        buttonHodos->setChecked(Utility::readPref("MODEL_BUFKIT_HODOS", "false").compare(0, 1, "t") == 0);
        buttonHodos->setToolTip("A small hodograph of the RRFS sounding at each station in view, for the hour on the map: wind from the surface to 9 km (pink 0-1 km, red to 3, green to 6, yellow above), "
                                "rings at 20 and 40 kt and the right-mover storm motion (orange dot); the 0-3 km helicity is in the tooltip. Pictures you save or open full size have them too. Each station is one small file (about 0.4 MB, every hour), read as it comes "
                                "into view; zoom in for more stations. Click one for its sounding. Not part of saved pictures.");
        QObject::connect(this, &QObject::destroyed, [owner = static_cast<const void *>(&bufkitOwner)] { NetManager::cancelOwner(owner); });   // the screen closes: its station files stop too
        hodoTimer.setSingleShot(true);
        QObject::connect(&hodoTimer, &QTimer::timeout, this, [this] { refreshHodographs(); });
        QObject::connect(buttonHodos, &QPushButton::toggled, this, [this] (bool on) {
            Utility::writePref("MODEL_BUFKIT_HODOS", on ? "true" : "false");
            refreshHodographs();
        });
        QObject::connect(&image, &ZoomImage::viewChanged, this, [this] { hodoTimer.start(250); });
        boxH2.addWidgetReal(buttonHodos);
        comboLocalKm = UiStandards::combo(this);
        for (const int km : {150, 300, 600, 1200, 2400}) {
            comboLocalKm->addItem(QString::number(km) + " km square", km);
        }
        comboLocalKm->setToolTip("How big the area round the saved location is (the area list has one entry for each place)");
        comboLocalKm->setVisible(false);
        QObject::connect(comboLocalKm, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this] (int index) {
            double lat = 0, lon = 0, km = 0;
            string name;
            if (index < 0 || !GfsChart::parseLocalId(objectModel.sector, lat, lon, km, name)) {
                return;
            }
            const int wanted = comboLocalKm->itemData(index).toInt();
            if (static_cast<int>(km) == wanted) {
                return;
            }
            Utility::writePrefInt("MODEL_LOCAL_KM", wanted);
            objectModel.sector = GfsChart::localId(lat, lon, wanted, name);
            refreshSectorList();
            reload();
        });
        boxH2.addWidgetReal(comboLocalKm);
        buttonSigma = UiStandards::button(this, "Std deviations");
        buttonSigma->setCheckable(true);
        buttonSigma->setVisible(false);
        buttonSigma->setToolTip("The anomaly in standard deviations from the 1991-2020 mean (the day's own spread of the reanalysis) instead of in decameters or millibars. The legend says how big one deviation is over the map (it differs by place), and the read-out shows both values at the point. The deviations are downloaded once, from Settings, Model Sources, Climatology.");
        QObject::connect(buttonSigma, &QPushButton::toggled, this, [this] (bool on) {
            standardDeviations = on;
            startPlaying(false);
            strip->stop();
            reload();
        });
        boxH2.addWidgetReal(buttonSigma);
        comboTiles.getView()->setToolTip("Several charts of the same hour side by side. They zoom, pan and read out together.");
        comboTilesShow.getView()->setToolTip("What the other tiles show: other charts of this model, the same chart from other models, or the same valid time from older runs. 'Ensemble parts' (with the ENS-ALL pool): the same chart from the members of the ECMWF, the GEFS and the GEPS alone.");
        comboTiles.setIndex(static_cast<size_t>(std::clamp(Utility::readPrefInt("MODEL_TILES", 0), 0, 3)));
        comboTilesShow.setIndex(static_cast<size_t>(std::clamp(Utility::readPrefInt("MODEL_TILES_SHOW", 0), 0, 5)));
        comboTiles.connect([this] {
            Utility::writePref("MODEL_TILES", std::to_string(comboTiles.getIndex()));
            applyTileLayout();
            resetTileSpecs();
            renderTiles();
            prefetch(prefetchGeneration);
        });
        comboTilesShow.connect([this] {
            Utility::writePref("MODEL_TILES_SHOW", std::to_string(comboTilesShow.getIndex()));
            resetTileSpecs();
            renderTiles();
            prefetch(prefetchGeneration);
        });
        boxH2.addWidget(comboTiles);
        boxH2.addWidget(comboTilesShow);
        buttonBuild = UiStandards::button(this, "Build a chart \xE2\x96\xBE");
        buttonBuild->setToolTip("Maps made to order: the chance of rain over a limit in a period of your choosing, of a temperature under or over a limit, of gusts over one.");
        menuBuild = new QMenu{buttonBuild};
        buttonBuild->setMenu(menuBuild);
        boxH2.addWidgetReal(buttonBuild);
        {   // how far ahead the hours are drawn on their own
            const auto mode = Utility::readPref("MODEL_PRELOAD", "auto");
            comboPreload.setIndex(mode == "around" ? 1 : mode == "12" ? 2 : mode == "24" ? 3 : mode == "all" ? 4 : 0);
            comboPreload.getView()->setToolTip("The hours after the one shown are drawn in the background so that stepping and play find them ready. Automatic: 24 hours for the GFS, 12 for the 84 hour RRFS runs, the whole run when it is under a day (the 18 hour hourly RRFS runs).");
            comboPreload.connect([this] {
                static const char * modes[] = {"auto", "around", "12", "24", "all"};
                Utility::writePref("MODEL_PRELOAD", modes[std::clamp(comboPreload.getIndex(), 0, 4)]);
                prefetch(prefetchGeneration);
                refreshLoaded();
            });
            boxH2.addWidget(comboPreload);
        }
        buttonMax = UiStandards::button(this, "Maximum \xE2\x96\xBE");
        auto * maxMenu = new QMenu{buttonMax};
        QObject::connect(maxMenu->addAction("Of the 24 hours ending at this hour"), &QAction::triggered, this, [this] { showMax(false); });
        QObject::connect(maxMenu->addAction("Day 1 (12z to 12z)"), &QAction::triggered, this, [this] { showMax(true); });
        buttonMax->setMenu(maxMenu);
        buttonMax->setToolTip("The largest value of the chart's fill over a day (hail, gusts, rain rate ...). Choosing another hour goes back to the plain chart.");
        boxH2.addWidgetReal(buttonMax);
        buttonViews = UiStandards::button(this, "Saved views \xE2\x96\xBE");
        menuViews = new QMenu{buttonViews};
        buttonViews->setMenu(menuViews);
        boxH2.addWidgetReal(buttonViews);
        loadViews();
        boxH2.addWidget(soundingPick.button());
        buttonMembers = UiStandards::button(this, "Members graph...");
        buttonMembers->setToolTip("REFS charts: each of the 5 members' value at the clicked point over the forecast hours, with the ensemble mean (and the threshold of a probability chart)");
        QObject::connect(buttonMembers, &QPushButton::clicked, this, [this] { openMemberGraph(); });
        boxH2.addWidgetReal(buttonMembers);
        buttonPlume = UiStandards::button(this, "Plume...");
        buttonPlume->setCheckable(true);
        buttonPlume->setToolTip("A second pane beside the chart with the forecast plume at the point clicked (or the first saved location): the CFSv2 runs for the CFS, the 31 members for the GEFS, the GFS runs for the others. It has all the plume window's choices");
        QObject::connect(buttonPlume, &QPushButton::clicked, this, [this] (bool on) {
            Utility::writePref("MODEL_PLUME_PANE", on ? "on" : "off");
            updatePlumePanel();
        });
        boxH2.addWidgetReal(buttonPlume);
        buttonOverlay = UiStandards::button(this, "Overlay models...");
        buttonOverlay->setToolTip("Draw the lines (heights, pressure) of other models over this chart's for the same valid time, each model in its own color. Save... writes it as a loop.");
        QObject::connect(buttonOverlay, &QPushButton::clicked, this, [this] { chooseOverlays(); });
        boxH2.addWidgetReal(buttonOverlay);
        buttonColors = UiStandards::button(this, "Colors...");
        buttonColors->setToolTip("Edit the color scale of this kind of chart: the stops, their colors and transparency, flat bands or smooth. Kept for every chart that uses the same scale.");
        QObject::connect(buttonColors, &QPushButton::clicked, this, [this] { editColors(); });
        boxH2.addWidgetReal(buttonColors);
        auto * save = UiStandards::button(this, "Save...");
        save->setToolTip("Save the picture, or the hours drawn so far as a loop");
        QObject::connect(save, &QPushButton::clicked, this, [this] { saveLoop(); });
        boxH2.addWidgetReal(save);
        boxH2.addStretch();
        box.addLayout(boxH2);
    }
    image.setCrosshairMode(true);   // click a point, then Sounding
    image.setSaveLoopHandler([this] { saveLoop(); });   // right-click > more formats, or the loop: the same save as the Save... button
    QObject::connect(&image, &ZoomImage::doubleClicked, this, [this] {
        if (!shownBytes.isEmpty()) {
            new ImageViewer{this, withGlyphs(shownBytes, shownProbe), objectModel.model + " " + objectModel.param};
        }
    });
    QObject::connect(&image, &ZoomImage::clicked, this, [this] (double fx, double fy) {
        double lon = 0.0, lat = 0.0;
        if (shownProbe && shownProbe->locate(fx, fy, lon, lat)) {
            image.setMarker(fx, fy);
            for (auto * tile : tiles) {
                tile->image->setMarker(fx, fy);
            }
            soundingPick.pick(lon, lat);
            clickedLat = lat;
            clickedLon = lon;
            haveClicked = true;
            if (plumePanel != nullptr && plumePanel->isVisible()) {
                plumePanel->setPoint(lat, lon);   // the plume follows the point clicked
            }
        }
    });
    {   // the tiles: the first is the screen's own chart (with everything it does), the others are compared with it
        compareArea = new QWidget{this};
        grid = new QGridLayout{compareArea};
        grid->setContentsMargins(0, 0, 0, 0);
        grid->setSpacing(4);
        mainTile = new QFrame{compareArea};
        auto * mainColumn = new QVBoxLayout{mainTile};
        mainColumn->setContentsMargins(2, 2, 2, 2);
        mainColumn->setSpacing(2);
        mainCaption = new QLabel{mainTile};
        mainCaption->setStyleSheet("font-weight: bold;");
        mainColumn->addWidget(mainCaption);
        mainColumn->addWidget(&image, 1);
        for (size_t i = 0; i < tiles.size(); i++) {
            tiles[i] = new CompareTile{compareArea};
            const int index = static_cast<int>(i) + 1;
            QObject::connect(tiles[i]->change, &QPushButton::clicked, this, [this, index] { changeTile(index); });
            QObject::connect(tiles[i]->image, &ZoomImage::clicked, this, [this, index] (double fx, double fy) { pickFromTiles(index, fx, fy); });
            QObject::connect(tiles[i]->image, &ZoomImage::doubleClicked, this, [this, i] {
                if (!tiles[i]->shownBytes.isEmpty()) {
                    new ImageViewer{this, tiles[i]->shownBytes, tiles[i]->caption->text().toStdString()};
                }
            });
        }
        // one view for all: the zoom and place of the tile touched go to the others, and the read-out of the point under the pointer shows in each
        std::vector<ZoomImage *> pictures{&image};
        for (auto * tile : tiles) {
            pictures.push_back(tile->image);
        }
        const auto hoverOf = [this] (size_t i) -> ChartHover * { return i == 0 ? hover.get() : tiles[i - 1]->hover.get(); };
        for (size_t from = 0; from < pictures.size(); from++) {
            QObject::connect(pictures[from], &ZoomImage::viewChanged, this, [this, pictures, from] {
                if (syncingViews) {
                    return;
                }
                syncingViews = true;
                const auto v = pictures[from]->view();
                for (size_t to = 0; to <= static_cast<size_t>(tileCount()); to++) {
                    if (to != from) {
                        pictures[to]->setView(v);
                    }
                }
                syncingViews = false;
            });
            QObject::connect(pictures[from], &ZoomImage::hovered, this, [this, pictures, from, hoverOf] (double fx, double fy) {
                for (size_t to = 0; to <= static_cast<size_t>(tileCount()); to++) {
                    if (to != from && hoverOf(to)) {
                        hoverOf(to)->show(fx, fy, 12, 12);
                    }
                    if (to != from) {
                        pictures[to]->setLinkedCursor(fx, fy);   // the same place marked on every pane, to read the panes against each other
                    }
                }
            });
            QObject::connect(pictures[from], &ZoomImage::hoverEnded, this, [this, pictures, from, hoverOf] {
                for (size_t to = 0; to <= static_cast<size_t>(tileCount()); to++) {
                    if (to != from && hoverOf(to)) {
                        hoverOf(to)->hide();
                    }
                    if (to != from) {
                        pictures[to]->clearLinkedCursor();
                    }
                }
            });
        }
        applyTileLayout();
    }
    {   // the chart and, beside it when it is asked for, the plume: one splitter, so that the person gives each the room it wants
        panes = new QSplitter{Qt::Horizontal, this};
        panes->setChildrenCollapsible(false);
        panes->addWidget(compareArea);
        box.addWidgetReal(panes, 1, Qt::Alignment{});
    }
    strip = new TimeStrip{this};
    strip->onSelect = [this] (int index) { selectHour(index); };
    strip->onPlay = [this] (bool on) { startPlaying(on); };
    box.addWidgetReal(strip);
    plumePanel = new CfsPlumePanel{this, panes, 0.0, 0.0, "gefs", false};   // (shown, and reading, only while the pane is on)
    plumePanel->setCompact(true);   // (the GEFS pane begins with the mean and spread: all the members are a choice, 600 reads the first time)
    plumePanel->setMinimumWidth(520);
    compareArea->setMinimumWidth(420);
    plumePanel->setVisible(false);
    panes->addWidget(plumePanel);
    panes->setStretchFactor(0, 3);
    panes->setStretchFactor(1, 2);
    playTimer.setSingleShot(false);
    QObject::connect(&playTimer, &QTimer::timeout, this, [this] {
        const int n = strip->count();
        if (n < 2) {
            return;
        }
        // the next hour whose charts are all drawn, in every tile: the loop plays what is rendered (and the tiles stay on the same hour); it waits where nothing else is ready yet
        for (int step = 1; step <= n; step++) {
            const int next = (strip->current() + step) % n;
            if (hourReady(std::atoi(objectModel.times[static_cast<size_t>(next)].c_str()))) {
                if (next != strip->current()) {
                    strip->setCurrent(next);
                    selectHour(next);
                }
                break;
            }
        }
        playTimer.setInterval(strip->intervalMs());
    });
    box.addWidgetReal(new ActivityLabel{this});
    box.getAndShow(this);
    refreshProductButton();

    startRun();
    // a screen left open sees a run that is published later: the newest run is looked for again every 15 minutes
    recheckTimer.setInterval(qEnvironmentVariableIntValue("WXQT_RECHECK_SECONDS") > 0 ? qEnvironmentVariableIntValue("WXQT_RECHECK_SECONDS") * 1000 : 15 * 60 * 1000);   // (the variable is for testing)
    QObject::connect(&recheckTimer, &QTimer::timeout, this, [this] { recheckRuns(); });
    recheckTimer.start();
}

void ModelViewer::recheckRuns() {
    if (rechecking || pinnedValid.isValid() || !GfsRender::drawsModel(objectModel.model) || (GfsModels::isStormModel(objectModel.model) && !Hafs::isStormId(objectModel.sector))) {
        return;
    }
    rechecking = true;
    const auto model = objectModel.model, storm = objectModel.sector;
    auto cycle = std::make_shared<string>();
    auto date = std::make_shared<string>();
    auto found = std::make_shared<bool>(false);
    new FutureVoid{this, [model, storm, cycle, date, found] { *found = GfsRender::latestCycle(model, *cycle, GfsModels::isStormModel(model) ? storm : string{}, date.get()); }, [this, model, cycle, date, found] {
        rechecking = false;
        if (!*found || model != objectModel.model || cycle->empty()) {
            return;
        }
        auto& known = objectModel.runTimeData;
        if (known.mostRecentRun == *cycle && known.newestDate == *date) {
            return;   // nothing newer
        }
        const bool following = objectModel.run == known.mostRecentRun;   // the newest run was the one on show
        if (following) {
            frames.clear();   // the pictures drawn so far are of the run before
            frameOrder.clear();
            frameBytes = 0;
            failedAhead.clear();
            getRun();         // the list, the hours and the chart of the new run
            return;
        }
        known.mostRecentRun = *cycle;   // another run was chosen on purpose: it stays, and the list marks the new newest
        known.newestDate = *date;
        comboboxRun.block();
        comboboxRun.setList(objectModel.runs);
        comboboxRun.setIndexByValue(objectModel.run);
        comboboxRun.setLabels(runLabels());
        comboboxRun.unblock();
    }};
}

// the hour chosen by the person is the cursor's new time (the run's time plus the hour)
void ModelViewer::announceTime() {
    if (!following || !pinnedValid.isValid()) {
        return;
    }
    const auto run = runTime(objectModel.run);
    if (run.isValid()) {
        HistoryCursor::get().setTime(run.addSecs(3600LL * std::atoi(objectModel.getTime().c_str())), this);
    }
}

// the cursor moved (another screen, or the History page): the run and hour of that time, a moment after the last move (a loop on another screen moves it often)
void ModelViewer::followCursor(QObject * origin) {
    if (!following || origin == this) {
        return;
    }
    cursorTimer.start(500);
}

void ModelViewer::openAt(const string& model, const QDateTime& validUtc, bool runStart) {
    pinnedRunStart = runStart;
    if (!following) {
        following = true;
        cursorTimer.setSingleShot(true);
        QObject::connect(&cursorTimer, &QTimer::timeout, this, [this] {
            const auto at = HistoryCursor::get().time();
            if (at.isValid() && at != pinnedValid) {
                const auto model = pinnedModel;
                if (!model.empty()) {
                    pinnedValid = at;   // the same model, the run and hour of the new time
                    openAt(model, at);
                }
            }
        });
        QObject::connect(&HistoryCursor::get(), &HistoryCursor::moved, this, [this] (QObject * origin) { followCursor(origin); });
    }
    pinnedValid = validUtc.toUTC();
    pinnedModel = model;
    if (runStart) {   // the cycle at or before it, whose first hour is the time of the run
        const QDateTime run{pinnedValid.date(), QTime{pinnedRunHour(), 0}, QTimeZone::utc()};
        pinnedValid = run;
    }
    if (timePick != nullptr) {
        timePick->setDateTime(pinnedValid);
    }
    pinnedModel = model;
    pinnedApply = true;
    for (size_t i = 0; i < objectModel.models.size(); i++) {
        if (objectModel.models[i] == model) {
            comboboxModel.block();
            comboboxModel.setIndex(i);
            comboboxModel.unblock();
            objectModel.model = model;
            objectModel.setModelVars(model);
            getRun();   // getRunStatus puts the run of the past in place of the newest
            return;
        }
    }
}

void ModelViewer::selectModel(const string& id) {
    for (size_t i = 0; i < objectModel.models.size(); i++) {
        if (objectModel.models[i] == id) {
            comboboxModel.setIndex(i);   // its handler changes the model
            return;
        }
    }
}

void ModelViewer::changeModelCb() {
    changeModel(comboboxModel.getIndex());
}

void ModelViewer::changeRunCb() {
    changeRun(comboboxRun.getIndex());
}

void ModelViewer::changeSectorCb() {
    changeSector(comboboxSector.getIndex());
}

void ModelViewer::changeProductCb() {
    changeParam(comboboxProduct.getIndex());
}

void ModelViewer::changeTimeCb() {
    changeTime(comboboxTime.getIndex());
}

void ModelViewer::changeModel(int index) {
    if (objectModel.models[index] != pinnedModel) {
        pinnedValid = QDateTime{};   // another model: back to the newest runs
    }
    runSwappedFrom.clear();
    objectModel.model = objectModel.models[index];
    objectModel.setModelVars(objectModel.model);
    startRun();
}

void ModelViewer::changeParam(int index) {
    objectModel.param = objectModel.params[index];
    refreshProductButton();
    reload();
}

void ModelViewer::changeSector(int index) {
    objectModel.sector = objectModel.sectors[index];
    reload();
}

void ModelViewer::changeRun(int index) {
    objectModel.run = objectModel.runs[index];
    runSwappedFrom.clear();   // the person chose: that is the run, whatever the chart
    reload();
}

void ModelViewer::changeTime(size_t index) {
    if (variant.kind == GfsRender::Variant::Kind::Max) {
        variant = {};
        variantKey.clear();
    }
    objectModel.setTimeIdx(index);
    reload();
    announceTime();
}

void ModelViewer::moveBack() {
    objectModel.leftClick();
    comboboxTime.block();
    comboboxTime.setIndex(objectModel.timeIdx);
    comboboxTime.unblock();
    changeTime(objectModel.timeIdx);
}

void ModelViewer::moveForward() {
    objectModel.rightClick();
    comboboxTime.block();
    comboboxTime.setIndex(objectModel.timeIdx);
    comboboxTime.unblock();
    changeTime(objectModel.timeIdx);
}

bool ModelViewer::hourReady(int hour) const {
    for (int spec = 0; spec <= tileCount(); spec++) {
        const auto job = makeJob(spec, hour, false);
        if (!job.ok || failedAhead.count(job.key)) {
            continue;
        }
        if (!frames.count(job.key)) {
            return false;
        }
    }
    return true;
}

string ModelViewer::frameKey(int hour) const {
    // a run of the past carries its date (the same "18Z" of another day is another run)
    string run = objectModel.run;
    if (pinnedValid.isValid() && objectModel.model == pinnedModel) {
        const auto t = runTime(objectModel.run);
        if (t.isValid()) {
            run = t.toString("yyyyMMddHH").toStdString();
        }
    }
    string key = objectModel.model + "|" + objectModel.param + "|" + objectModel.sector + "|" + run + "|" + std::to_string(hour) + "|bs" + std::to_string(Utility::readPrefInt("BARB_SPACING", 100)) + (GfsModels::isStormModel(objectModel.model) ? "|rad" + Utility::readPref("HAFS_RADII", "true").substr(0, 1) : string{});   // (the barbs are in the picture: another spacing, another picture)
    for (const auto& id : overlays) {
        key += "|" + id;
    }
    string overlay;
    for (const auto& other : overlayModels) {
        overlay += (overlay.empty() ? "|overlay=" : ",") + other;
    }
    return key + variantKey + (member.empty() ? string{} : "|member=" + member) + (standardDeviations ? "|sd" : "") + overlay;
}

void ModelViewer::setVariant(GfsRender::Variant v) {
    variant = std::move(v);
    variantKey.clear();
    if (variant.kind == GfsRender::Variant::Kind::Change) {
        variantKey = "|change" + std::to_string(variant.hoursBack);
    } else if (variant.kind == GfsRender::Variant::Kind::Max && !variant.hours.empty()) {
        variantKey = "|max" + std::to_string(variant.hours.front()) + "-" + std::to_string(variant.hours.back());
    }
    startPlaying(false);
    strip->stop();
    reload();
}

// the 24 hours ending at the hour shown, or the model's Day 1 (12z to 12z): the chart's fill, largest of those hours
void ModelViewer::showMax(bool day1) {
    GfsRender::Variant v;
    v.kind = GfsRender::Variant::Kind::Max;
    const int now = std::atoi(objectModel.getTime().c_str());
    const int cycle = std::atoi(objectModel.run.c_str());
    int from = now - 24, to = now;
    if (day1) {
        const int start = ((12 - cycle) % 24 + 24) % 24;   // the first 12z after the run
        from = start;
        to = start + 24;
    }
    for (const auto& t : objectModel.times) {
        const int h = std::atoi(t.c_str());
        if (h >= from && h <= to) {
            v.hours.push_back(h);
        }
    }
    if (v.hours.size() < 2) {
        setTitle(objectModel.model + " - not enough hours in that range for a maximum");
        return;
    }
    comboCompare.block();
    comboCompare.setIndex(0);
    comboCompare.unblock();
    setVariant(v);
}

namespace {
    const string viewsPref{"MODEL_SAVED_VIEWS"};   // one view per line: name|model|chart|area|extras
}

void ModelViewer::loadViews() {
    views.clear();
    const auto text = QString::fromStdString(Utility::readPref(viewsPref, ""));
    for (const auto& line : text.split('\n', Qt::SkipEmptyParts)) {
        const auto parts = line.split('|');
        if (parts.size() == 5) {
            views.push_back({parts[0].toStdString(), parts[1].toStdString(), parts[2].toStdString(), parts[3].toStdString(), parts[4].toStdString()});
        }
    }
    rebuildViewsMenu();
}

void ModelViewer::rebuildViewsMenu() {
    menuViews->clear();
    QObject::connect(menuViews->addAction("Save this view..."), &QAction::triggered, this, [this] { saveView(); });
    if (!views.empty()) {
        menuViews->addSeparator();
    }
    for (const auto& view : views) {
        auto * sub = menuViews->addMenu(QString::fromStdString(view[0]));
        const auto copy = view;
        QObject::connect(sub->addAction("Show"), &QAction::triggered, this, [this, copy] { applyView(copy); });
        QObject::connect(sub->addAction("Delete"), &QAction::triggered, this, [this, copy] {
            views.erase(std::remove_if(views.begin(), views.end(), [&copy] (const auto& v) { return v[0] == copy[0]; }), views.end());
            QStringList lines;
            for (const auto& v : views) {
                lines << QString::fromStdString(v[0] + "|" + v[1] + "|" + v[2] + "|" + v[3] + "|" + v[4]);
            }
            Utility::writePref(viewsPref, lines.join('\n').toStdString());
            rebuildViewsMenu();
        });
    }
}

void ModelViewer::saveView() {
    bool ok = false;
    auto name = QInputDialog::getText(this, "Save view", "Name for this view (model, chart, area and extras):", QLineEdit::Normal,
                                      QString::fromStdString(objectModel.model + " " + objectModel.param), &ok).trimmed();
    name.remove('|').remove('\n');
    if (!ok || name.isEmpty()) {
        return;
    }
    string extras;
    for (const auto& id : overlays) {
        extras += (extras.empty() ? "" : ",") + id;
    }
    const std::array<string, 5> view{name.toStdString(), objectModel.model, objectModel.param, objectModel.sector, extras};
    const auto existing = std::find_if(views.begin(), views.end(), [&view] (const auto& v) { return v[0] == view[0]; });
    if (existing != views.end()) {
        *existing = view;
    } else {
        views.push_back(view);
    }
    QStringList lines;
    for (const auto& v : views) {
        lines << QString::fromStdString(v[0] + "|" + v[1] + "|" + v[2] + "|" + v[3] + "|" + v[4]);
    }
    Utility::writePref(viewsPref, lines.join('\n').toStdString());
    rebuildViewsMenu();
}

void ModelViewer::applyView(const std::array<string, 5>& view) {
    const auto items = comboboxModel.getItems();
    const auto found = std::find(items.begin(), items.end(), view[1]);
    if (found == items.end()) {
        return;
    }
    pendingView = view;
    if (view[1] != objectModel.model) {   // another model: its runs come first, then the rest of the view is put in
        comboboxModel.block();
        comboboxModel.setIndex(static_cast<size_t>(found - items.begin()));
        comboboxModel.unblock();
        changeModel(static_cast<int>(found - items.begin()));
        return;
    }
    objectModel.param = view[2];
    objectModel.sector = view[3];
    overlays.clear();
    for (const auto& id : QString::fromStdString(view[4]).split(',', Qt::SkipEmptyParts)) {
        overlays.push_back(id.toStdString());
    }
    pendingView = {};
    comboboxProduct.block();
    comboboxProduct.setIndexByValue(objectModel.param);
    comboboxProduct.unblock();
    comboboxSector.block();
    comboboxSector.setIndexByValue(objectModel.sector);
    comboboxSector.unblock();
    refreshProductButton();
    reload();
}

// the hours drawn so far of the chart on view, as a loop; or the picture
void ModelViewer::saveLoop() {
    if (shownBytes.isEmpty()) {
        return;
    }
    if (tileCount() > 0) {   // the tiles side by side, as one picture
        const auto picture = compositePicture();   // (the tiles' pictures go as they are)
        const auto stem = QString::fromStdString(objectModel.model + "_compare_" + objectModel.sector + "_" + objectModel.run);
        UtilityAnimationExport::saveWithDialog(this, std::vector<QByteArray>{}, strip->intervalMs(), picture, stem, QByteArray{}, false, stem);
        return;
    }
    std::vector<std::pair<int, QByteArray>> drawn;
    for (const auto& t : objectModel.times) {
        const int hour = std::atoi(t.c_str());
        const auto found = frames.find(frameKey(hour));
        if (found != frames.end()) {
            drawn.emplace_back(hour, withGlyphs(found->second.bytes, found->second.probe));
        }
    }
    std::vector<QByteArray> loop;
    for (const auto& [hour, bytes] : drawn) {
        loop.push_back(bytes);
    }
    string versus;
    for (const auto& other : overlayModels) {
        versus += "_" + other;
    }
    const auto name = QString::fromStdString(objectModel.model + (versus.empty() ? "" : "_vs" + versus) + "_" + objectModel.param + "_" + objectModel.sector + "_" + objectModel.run);
    UtilityAnimationExport::saveWithDialog(this, loop.size() >= 2 ? loop : std::vector<QByteArray>{}, strip->intervalMs(), withGlyphs(shownBytes, shownProbe), name, QByteArray{}, false, name);
}

void ModelViewer::showFrame(const Frame& frame) {
    if (objectModel.model == "SPCREFS") {
        refreshMembers();   // the members' names are known once a run has been read
    }
    shownBytes = frame.bytes;
    shownProbe = frame.probe;
    string chart = objectModel.model + "|" + objectModel.param + "|" + objectModel.sector;
    for (const auto& id : overlays) {
        chart += "|" + id;
    }
    if (chart == lastChart && image.hasImage()) {   // another hour of the chart on view: the zoom and the place stay
        image.setBytesKeepView(frame.bytes);
    } else {
        image.setBytes(frame.bytes);
    }
    lastChart = chart;
    if (!hover) {
        hover = std::make_unique<ChartHover>(&image);
    }
    hover->set(frame.probe);
    refreshStationPins();
    hodoTimer.start(120);
    mainCaption->setText(QString::fromStdString(caption(makeJob(0, std::atoi(objectModel.getTime().c_str()), false))));
    renderTiles();
}

// the station buttons for the map on view: placed again only when the map itself changed (not for another hour of it)
void ModelViewer::refreshStationPins() {
    if (buttonStations == nullptr || !buttonStations->isChecked() || !shownProbe || !shownProbe->valid()) {
        image.clearPins();
        stationPinKey.clear();
        return;
    }
    const auto& p = *shownProbe;
    const string key = QString{"%1|%2|%3|%4|%5|%6|%7|%8|%9|%10"}.arg(p.west).arg(p.east).arg(p.south).arg(p.north).arg(p.areaLeft).arg(p.areaTop).arg(p.areaWidth)
                           .arg(p.areaHeight).arg(p.imageWidth).arg(p.imageHeight).toStdString();
    if (key == stationPinKey) {
        return;
    }
    stationPinKey = key;
    std::vector<ZoomImage::Pin> pins;
    for (const auto& station : UtilityBufkit::stations()) {
        double fx = 0.0, fy = 0.0;
        if (p.place(station.longitude, station.latitude, fx, fy)) {
            pins.push_back({fx, fy, QString::fromStdString(station.id), QString::fromStdString(station.id) + "  RRFS sounding"});
        }
    }
    image.setPins(pins);
}

// the stations that get a glyph: those in `visible` (fractions of the picture of width x height pixels), the NWS offices' first, then the other US airports, then the rest, none nearer than
// `spacing` pixels to another (up to 150); every station of the feed is a candidate
std::vector<ModelViewer::GlyphPick> ModelViewer::glyphStations(const GfsChart::Probe& probe, const QRectF& visible, double width, double height, double spacing) const {
    std::vector<GlyphPick> inView, picks;
    const auto& offices = UtilityBufkit::officeStations();
    for (const auto& station : UtilityBufkit::stations()) {
        double fx = 0.0, fy = 0.0;
        if (probe.place(station.longitude, station.latitude, fx, fy) && visible.contains(QPointF{fx, fy})) {
            inView.push_back({station.id, fx, fy});
        }
    }
    const auto rank = [&offices] (const GlyphPick& pick) {
        return offices.count(pick.id) > 0 ? 0 : (pick.id.size() == 4 && pick.id[0] == 'K' ? 1 : 2);
    };
    std::stable_sort(inView.begin(), inView.end(), [&rank] (const GlyphPick& a, const GlyphPick& b) { return rank(a) < rank(b); });
    for (const auto& pick : inView) {
        if (picks.size() >= 150) {
            break;
        }
        const double x = pick.fx * width, y = pick.fy * height;
        const bool free = std::none_of(picks.begin(), picks.end(), [&] (const GlyphPick& other) {
            return std::hypot(other.fx * width - x, other.fy * height - y) < spacing;
        });
        if (free) {
            picks.push_back(pick);
        }
    }
    return picks;
}

// a picture of the map with the hodographs drawn on it (when the Hodographs button is on): those of the stations whose files have been read, for the picture's own hour
QByteArray ModelViewer::withGlyphs(const QByteArray& bytes, const std::shared_ptr<GfsChart::Probe>& probe) const {
    if (buttonHodos == nullptr || !buttonHodos->isChecked() || !probe || !probe->valid() || bytes.isEmpty()) {
        return bytes;
    }
    QImage picture = QImage::fromData(bytes);
    if (picture.isNull()) {
        return bytes;
    }
    const double size = picture.width() * 60.0 / 1100.0;   // as large, against the picture, as on a screen showing it 1100 pixels wide
    const string valid = probe->validUtc.isValid() ? probe->validUtc.toUTC().toString("yyMMdd/HHmm").toStdString() : string{};
    QPainter painter{&picture};
    // inside the chart's frame only (the same mask as on screen)
    painter.setClipRect(QRectF{probe->areaLeft / probe->imageWidth * picture.width(), probe->areaTop / probe->imageHeight * picture.height(),
                               probe->areaWidth / probe->imageWidth * picture.width(), probe->areaWidth > 0.0 ? probe->areaHeight / probe->imageHeight * picture.height() : 0.0});
    int drawn = 0;
    for (const auto& pick : glyphStations(*probe, QRectF{0, 0, 1, 1}, 1100.0, 1100.0 * probe->imageHeight / std::max(1.0, probe->imageWidth), 64.0)) {
        const auto run = bufkitRuns.find(pick.id);
        if (run == bufkitRuns.end()) {
            continue;
        }
        const auto& times = run->second->validTimes;
        const auto at = std::find(times.begin(), times.end(), valid);
        std::vector<QPointF> tips;
        QPointF storm;
        double srh = 0.0;
        if (at == times.end() || !HodographGlyph::measure(run->second->profiles[static_cast<size_t>(at - times.begin())], tips, storm, srh)) {
            continue;
        }
        const double x = pick.fx * picture.width(), y = pick.fy * picture.height();
        HodographGlyph::paint(painter, QRectF{x - size / 2.0, y - size / 2.0, size, size}, tips, storm);
        drawn++;
    }
    painter.end();
    if (drawn == 0) {
        return bytes;
    }
    QByteArray out;
    QBuffer buffer{&out};
    buffer.open(QIODevice::WriteOnly);
    picture.save(&buffer, "PNG");
    return out;
}

// the hodograph glyphs: the NWS offices' stations first, then the other airports, none closer than a glyph to another (up to 150); each station's file is read the first time it
// is wanted, four at a time, and the glyph appears when it is in
void ModelViewer::refreshHodographs() {
    const auto clear = [this] {
        for (auto& [id, glyph] : hodoGlyphs) {
            image.removeOverlay(glyph);
        }
        hodoGlyphs.clear();
    };
    if (buttonHodos == nullptr || !buttonHodos->isChecked()) {
        cancelBufkit();
    }
    if (buttonHodos == nullptr || !buttonHodos->isChecked() || !shownProbe || !shownProbe->valid() || !image.hasImage()) {
        clear();
        return;
    }
    const auto& probe = *shownProbe;
    // the glyphs stay inside the chart's own frame: not over the title, the legend or the credits
    image.setOverlayClip(QRectF{probe.areaLeft / probe.imageWidth, probe.areaTop / probe.imageHeight, probe.areaWidth / probe.imageWidth, probe.areaHeight / probe.imageHeight});
    const QRectF visible = image.visibleFraction();
    const QSize size = image.pictureSize();
    const auto picks = glyphStations(probe, visible, size.width(), size.height(), 64.0);
    // what a saved picture of the whole map will need is read too, so it has its glyphs when saved
    for (const auto& pick : glyphStations(probe, QRectF{0, 0, 1, 1}, 1100.0, 1100.0 * probe.imageHeight / std::max(1.0, probe.imageWidth), 64.0)) {
        if (bufkitRuns.count(pick.id) == 0 && bufkitAsked.insert(pick.id).second) {
            bufkitQueue.push_back(pick.id);
        }
    }
    // the picture's hour as BUFKIT writes it: yymmdd/hhmm UTC
    const string valid = probe.validUtc.isValid() ? probe.validUtc.toUTC().toString("yyMMdd/HHmm").toStdString() : string{};
    std::set<string> wanted;
    for (const auto& pick : picks) {
        const string& id = pick.id;
        wanted.insert(id);
        const auto run = bufkitRuns.find(id);
        if (run == bufkitRuns.end()) {
            if (bufkitAsked.insert(id).second) {
                bufkitQueue.push_back(id);
            }
            continue;
        }
        const auto& times = run->second->validTimes;
        const auto at = std::find(times.begin(), times.end(), valid);
        std::vector<QPointF> tips;
        QPointF storm;
        double srh = -9999.0;
        const bool have = at != times.end() &&
            HodographGlyph::measure(run->second->profiles[static_cast<size_t>(at - times.begin())], tips, storm, srh);
        auto glyph = hodoGlyphs.find(id);
        if (!have) {
            if (glyph != hodoGlyphs.end()) {
                image.removeOverlay(glyph->second);
                hodoGlyphs.erase(glyph);
            }
            continue;
        }
        if (glyph == hodoGlyphs.end()) {
            auto * created = new HodographGlyph{nullptr, QString::fromStdString(id)};
            QObject::connect(created, &HodographGlyph::clicked, this, [this] (const QString& station) {
                new SoundingViewer{this, "RRFS", station.toStdString(), true};
            });
            image.addOverlay(created, pick.fx, pick.fy);
            glyph = hodoGlyphs.emplace(id, created).first;
        } else {
            image.moveOverlay(glyph->second, pick.fx, pick.fy);
        }
        glyph->second->set(tips, storm, srh);
    }
    for (auto it = hodoGlyphs.begin(); it != hodoGlyphs.end();) {   // the stations no longer in view
        if (wanted.count(it->first) == 0) {
            image.removeOverlay(it->second);
            it = hodoGlyphs.erase(it);
        } else {
            ++it;
        }
    }
    pumpBufkit();
}

void ModelViewer::cancelBufkit() {
    if (bufkitQueue.empty() && bufkitRunning == 0) {
        return;
    }
    for (const auto& id : bufkitQueue) {
        bufkitAsked.erase(id);   // never started: asked for again when the button goes on
    }
    bufkitQueue.clear();
    if (bufkitRunning > 0) {
        NetManager::cancelOwner(&bufkitOwner);   // the files on their way: their callers get an empty answer, the tasks end, and the stations are asked for again next time
        for (auto it = bufkitAsked.begin(); it != bufkitAsked.end();) {
            it = bufkitRuns.count(*it) == 0 ? bufkitAsked.erase(it) : std::next(it);
        }
    }
}

void ModelViewer::pumpBufkit() {
    while (bufkitRunning < 4 && !bufkitQueue.empty()) {
        const string id = bufkitQueue.front();
        bufkitQueue.pop_front();
        bufkitRunning++;
        auto run = std::make_shared<UtilityBufkit::Run>();
        auto ok = std::make_shared<bool>(false);
        new FutureVoid{this,
            [run, ok, id, owner = static_cast<const void *>(&bufkitOwner)] {
                const NetPriority::Carry station{2, owner};   // background, and the Hodographs button's own: cancelOwner(owner) stops these and nothing else
                string error;
                *ok = UtilityBufkit::fetchLatest("RRFS", id, *run, error);
            },
            [this, run, ok, id] {
                bufkitRunning--;
                if (buttonHodos == nullptr || !buttonHodos->isChecked()) {
                    bufkitAsked.erase(id);
                    if (*ok) {
                        bufkitRuns[id] = run;   // it did arrive: kept for when the button goes on again
                    }
                    return;
                }
                if (*ok) {
                    bufkitRuns[id] = run;
                    hodoTimer.start(150);
                }
                pumpBufkit();
            }};
    }
}

// a time chosen on the timeline: the same as choosing it in the list
void ModelViewer::selectHour(int index) {
    if (index < 0 || index >= static_cast<int>(objectModel.times.size())) {
        return;
    }
    if (variant.kind == GfsRender::Variant::Kind::Max) {   // another hour: the plain chart again
        variant = {};
        variantKey.clear();
    }
    comboboxTime.block();
    comboboxTime.setIndex(static_cast<size_t>(index));
    comboboxTime.unblock();
    changeTime(static_cast<size_t>(index));
}

void ModelViewer::startPlaying(bool on) {
    playing = on;
    ++prefetchGeneration;
    if (on) {
        playTimer.start(strip->intervalMs());
        prefetch(prefetchGeneration);
    } else {
        playTimer.stop();
    }
}

// Draws the hours ahead of the one shown, one at a time, so that play runs from drawn frames; it stops when everything is drawn or the chart changes.
void ModelViewer::storeFrame(const string& key, const Frame& frame) {
    const auto size = [] (const Frame& f) { return static_cast<size_t>(f.bytes.size()) + (f.probe ? f.probe->memory() : 0); };
    if (const auto old = frames.find(key); old != frames.end()) {
        frameBytes -= std::min(frameBytes, size(old->second));
    } else {
        frameOrder.push_back(key);
    }
    frames[key] = frame;
    frameBytes += size(frame);
    // a loop of a whole run has to fit: the frames may hold 300 MB (a few hundred for a regional chart, fewer for a global one); the oldest go first
    while (frameBytes > 300u * 1024 * 1024 && frameOrder.size() > 1) {
        if (const auto oldest = frames.find(frameOrder.front()); oldest != frames.end()) {
            frameBytes -= std::min(frameBytes, size(oldest->second));
            frames.erase(oldest);
        }
        frameOrder.pop_front();
    }
}

// How many hours ahead of the one shown to draw without being asked, from the setting: nothing but the neighbours, 12 or 24 hours, the whole run, or (the default) what suits the model: the
// registry says how far it goes (24 for the GFS, 12 for the 84 hour RRFS) and how long the run is (a run under a day is drawn whole).
int ModelViewer::preloadAhead() const {
    const auto mode = Utility::readPref("MODEL_PRELOAD", "auto");
    if (mode == "around") return 0;
    if (mode == "12") return 12;
    if (mode == "24") return 24;
    if (mode == "all") return 100000;
    const auto * def = GfsModels::find(objectModel.model);
    const int cycle = std::atoi(objectModel.run.c_str());
    if (const auto * product = GfsChart::product(objectModel.param, objectModel.model); product != nullptr && GfsChart::needs(*product, 24).size() > 40) {
        return 4;   // a chart that reads every member (31 of them, several fields each): a few hours ahead, not the run (an hour is some seconds of work, hundreds of requests)
    }
    if (def && def->runLength && def->runLength(cycle) <= 24) {
        return 100000;
    }
    return def ? def->preloadHours : 24;
}

void ModelViewer::refreshLoaded() {
    if (!strip || strip->count() < 2 || !GfsRender::handles(objectModel.model, objectModel.param)) {
        return;
    }
    std::vector<bool> ready;
    int have = 0, wanted = 0;
    const int now = std::atoi(objectModel.getTime().c_str());
    const int ahead = preloadAhead();
    int last = now;
    for (const auto& t : objectModel.times) {
        const int hour = std::atoi(t.c_str());
        const bool isReady = hourReady(hour);
        ready.push_back(isReady);
        have += isReady;
        if (hour >= now && hour <= now + ahead) {
            wanted++;
            last = hour;
        }
    }
    int windowReady = 0;
    for (size_t i = 0; i < objectModel.times.size(); i++) {
        const int hour = std::atoi(objectModel.times[i].c_str());
        if (ready[i] && hour >= now && hour <= last) {
            windowReady++;
        }
    }
    strip->setLoaded(ready, windowReady < wanted && ahead > 0 ? QString{"Drawing %1 of %2"}.arg(windowReady).arg(wanted) : QString{"%1 of %2 frames ready"}.arg(have).arg(objectModel.times.size()));
}

// Draws the hours after the one shown (and the one before it) without being asked, a few at a time, with the network's "ahead" priority: what is on screen is always served first. While playing,
// every hour of the loop as far as it gets; otherwise the neighbours first and then as far ahead as the setting says. It stops when they are drawn or the chart changes.
void ModelViewer::prefetch(int generation) {
    if (generation != prefetchGeneration || !strip->isVisible() || variant.kind == GfsRender::Variant::Kind::Max || !GfsRender::handles(objectModel.model, objectModel.param)) {
        return;
    }
    const int n = strip->count();
    if (n < 2) {
        return;
    }
    std::vector<int> candidates;
    if (playing) {
        for (int i = 1; i <= n; i++) {
            candidates.push_back((strip->current() + i) % n);
        }
    } else {
        for (const int d : {1, -1, 2}) {
            const int index = strip->current() + d;
            if (index >= 0 && index < n) {
                candidates.push_back(index);
            }
        }
        const int ahead = preloadAhead();
        const int now = std::atoi(objectModel.getTime().c_str());
        for (int index = strip->current() + 3; index < n && ahead > 0; index++) {
            if (std::atoi(objectModel.times[static_cast<size_t>(index)].c_str()) - now > ahead) {
                break;
            }
            candidates.push_back(index);
        }
    }
    const int specs = 1 + tileCount();
    const size_t limit = std::min<size_t>(6, (playing || preloadAhead() > 0 ? 3 : 1) * static_cast<size_t>(specs));
    for (const int index : candidates) {
        const int mainHour = std::atoi(objectModel.times[static_cast<size_t>(index)].c_str());
        for (int spec = 0; spec < specs; spec++) {   // the chart of each tile
            if (inFlight.size() >= limit) {
                break;
            }
            const auto job = makeJob(spec, mainHour, true);
            const auto key = job.key;
            if (!job.ok || frames.count(key) || failedAhead.count(key) || inFlight.count(key)) {
                continue;
            }
            inFlight.insert(key);
            if (!gfsSession) {
                gfsSession = std::make_shared<GfsRender::Session>();
            }
            auto session = gfsSession;
            auto result = std::make_shared<std::pair<QByteArray, string>>();
            auto probe = std::make_shared<GfsChart::Probe>();
            new FutureVoid{this, [=] {
                               const NetManager::Scope aheadScope{NetManager::Priority::Ahead};   // behind whatever the user is waiting for
                               result->first = GfsRender::png(*session, job.model, job.param, job.sector, job.cycle, job.hour, job.overlays, result->second, probe.get(), job.variant);
                           },
                           [this, result, probe, key, generation] {
                               inFlight.erase(key);
                               if (result->first.isEmpty()) {   // no chart at this hour (or the request was dropped as the view changed): not asked for again until the view changes
                                   failedAhead.insert(key);
                                   if (playing && generation == prefetchGeneration && inFlight.empty()) {
                                       startPlaying(false);
                                       strip->stop();
                                   }
                               } else {
                                   storeFrame(key, {result->first, probe});
                               }
                               refreshLoaded();
                               prefetch(prefetchGeneration);
                           }};
        }
    }
    refreshLoaded();
}

// A run longer than ten days (the CFS: 6984 hours) is labelled by the date of each hour, MM-DD, in the strip and the list: the hours mean nothing at that length. The list also has the weekday and
// the clock, and the day of the run.
std::string ModelViewer::hourText(const std::string& timeItem, bool forList) const {
    const int hour = std::atoi(timeItem.c_str());
    const bool longRun = !objectModel.times.empty() && std::atoi(objectModel.times.back().c_str()) > 240;
    if (!longRun) {
        return forList ? timeItem : WString::split(timeItem, " ")[0];
    }
    const auto run = runTime(objectModel.run);
    if (!run.isValid()) {
        return WString::split(timeItem, " ")[0];
    }
    const auto local = run.addSecs(3600LL * hour).toLocalTime();
    const auto date = local.toString("MM-dd").toStdString();
    if (!forList) {
        return date;
    }
    return date + "  " + local.toString("ddd").toStdString() + "  " + std::to_string(local.time().hour() % 12 == 0 ? 12 : local.time().hour() % 12) + (local.time().hour() >= 12 ? "pm" : "am") + "   (day " + QString::number(hour / 24.0, 'f', hour % 24 == 0 ? 0 : 2).toStdString() + ")";
}

// The plume under the chart of the CFS and GEFS pages: at the point clicked, or the first saved location; the model of the page picks the plume's (CFS: the nine month runs, GEFS: the members).
void ModelViewer::updatePlumePanel() {
    if (plumePanel == nullptr || buttonPlume == nullptr) {
        return;
    }
    const auto& id = objectModel.model;
    const bool analysis = id == "CFS-ANL";   // (an analysis has no forecast to plume)
    const string setting = Utility::readPref("MODEL_PLUME_PANE", "auto");   // "on", "off", or "auto": on for the CFS and GEFS pages
    const bool wanted = !analysis && (setting == "on" || (setting == "auto" && (id == "CFS" || id == "GEFS")));
    buttonPlume->setEnabled(!analysis);
    buttonPlume->setChecked(wanted);
    const bool first = wanted && !plumePanel->isVisible();
    plumePanel->setVisible(wanted);
    if (first) {   // the chart keeps its room: half and half to begin with, the person moves the bar
        QTimer::singleShot(0, this, [this] { panes->setSizes({std::max(480, panes->width() / 2), std::max(520, panes->width() / 2)}); });
    }
    if (!wanted) {
        return;
    }
    if (first) {
        const auto places = Location::getListLatLons();
        const double lat = haveClicked ? clickedLat : (places.empty() ? 39.1 : places.front().lat()), lon = haveClicked ? clickedLon : (places.empty() ? -94.6 : places.front().lon());
        plumePanel->setPoint(lat, lon);
    }
    plumePanel->setModel(id == "CFS" ? "cfs" : id.compare(0, 4, "GEFS") == 0 ? "gefs" : "gfs");
    const auto run = runTime(objectModel.run);   // the chart's run: the plume follows it unless the person has it take the newest
    if (run.isValid()) {
        plumePanel->setPageRun(run.toString("yyyyMMdd").toStdString(), run.toString("HH").toStdString(), id == "CFS" || id == "GEFS");
    }
}

void ModelViewer::refreshTimeStrip() {
    frames.clear();   // a refreshed run: draw the frames again
    frameOrder.clear();
    frameBytes = 0;
    failedAhead.clear();
    std::vector<string> labels;
    for (const auto& t : objectModel.times) {
        labels.push_back(hourText(t, false));
    }
    strip->setTimes(labels, static_cast<int>(comboboxTime.getIndex()));
    updatePlumePanel();
    strip->setVisible(GfsRender::drawsModel(objectModel.model));
    refreshLoaded();
}

// The NBM's hourly runs (01Z, 13Z ...) make only the 1 hour fields: the 12 hour extremes and the 6 / 12 hour chances are in the 00, 06, 12 and 18Z runs. A chart of those moves the run to
// the newest main run at or before the one chosen, and the amber line above the timeline says what happened and how to undo it; another chart puts the run back.
void ModelViewer::applyMainRunRule() {
    if (!strip) {
        return;
    }
    const auto * product = GfsRender::drawsModel(objectModel.model) ? GfsChart::product(objectModel.param, objectModel.model) : nullptr;
    const auto hourOf = [] (const string& run) { return run.size() >= 2 ? std::atoi(run.substr(0, 2).c_str()) : -1; };
    const auto select = [this] (const string& run) {
        objectModel.run = run;
        comboboxRun.block();
        comboboxRun.setIndexByValue(run);
        comboboxRun.unblock();
    };
    const int step = product != nullptr ? product->mainCycleStep : 0;
    const string chosen = runSwappedFrom.empty() ? objectModel.run : runSwappedFrom;   // the run the person has, whatever a chart moved the screen to
    QStringList notes;
    if (step > 0 && hourOf(chosen) >= 0 && hourOf(chosen) % step != 0) {
        const auto chosenTime = runTime(chosen);
        string best;
        QDateTime bestTime;
        for (const auto& run : objectModel.runs) {
            const auto t = runTime(run);
            if (hourOf(run) % step == 0 && t.isValid() && chosenTime.isValid() && t <= chosenTime && (!bestTime.isValid() || t > bestTime)) {
                best = run;
                bestTime = t;
            }
        }
        const QString only = step == 12 ? "00 and 12Z" : "00, 06, 12 and 18Z";
        if (!best.empty()) {
            runSwappedFrom = chosen;
            select(best);
            notes << QString::fromStdString("This chart is drawn from the " + best + " run, not the " + chosen + " one: " + product->label + " exists only in the ") + only +
                         QString::fromStdString(" runs (the others carry 1 hour fields only). Choose another chart and the run goes back to " + chosen + ".");
        } else {
            notes << QString::fromStdString("No ") + (step == 12 ? "00 or 12Z" : "00, 06, 12 or 18Z") + QString::fromStdString(" run is in the list: " + product->label + " exists only in those runs.");
        }
    } else if (!runSwappedFrom.empty()) {   // a chart that does not need it: the run the person had
        if (std::find(objectModel.runs.begin(), objectModel.runs.end(), runSwappedFrom) != objectModel.runs.end()) {
            select(runSwappedFrom);
        }
        runSwappedFrom.clear();
    }
    // a chart of a period that ends at the hour has it only at some hours: the timeline moves to the next one
    const int period = product != nullptr ? product->periodHours : 0;
    if (period > 0) {
        const int shown = std::atoi(objectModel.getTime().c_str());
        const int cycle = std::max(hourOf(objectModel.run), 0);
        const int endHour = product->periodEndHourUtc;
        const auto has = [&] (int hour) { return hour >= period && hour % period == 0 && (endHour < 0 || (cycle + hour) % 24 == endHour); };
        if (!has(shown)) {
            int target = -1;   // the next hour that has it, else the last one before
            int bestBefore = -1;
            for (const auto& t : objectModel.times) {
                const int h = std::atoi(t.c_str());
                if (has(h) && h >= shown && (target < 0 || h < target)) {
                    target = h;
                }
                if (has(h) && h < shown && h > bestBefore) {
                    bestBefore = h;
                }
            }
            if (target < 0) {
                target = bestBefore;
            }
            for (size_t i = 0; target >= 0 && i < objectModel.times.size(); i++) {
                if (std::atoi(objectModel.times[i].c_str()) == target) {
                    comboboxTime.block();
                    comboboxTime.setIndex(i);
                    comboboxTime.unblock();
                    objectModel.setTimeIdx(i);
                    QString every = QString{"every %1 hours"}.arg(period);
                    if (endHour >= 0) {
                        every = QString{"the forecast hours that end at %1Z (every %2 hours)"}.arg(endHour, 2, 10, QChar{'0'}).arg(24);
                    }
                    notes << QString{"This chart is of the %1 hours that end at the forecast hour, and the %2Z run has it at %3: the timeline moved from F%4 to F%5."}
                                 .arg(period).arg(cycle, 2, 10, QChar{'0'}).arg(every).arg(shown, 3, 10, QChar{'0'}).arg(target, 3, 10, QChar{'0'});
                    break;
                }
            }
        }
    }
    strip->setNotice(notes.join("\n"));
}

// The hours of the list as far as the run on show goes: a model whose runs differ in length (the RRFS: 84 hours from 00, 06, 12 and 18Z, 18 from the others) lists only the hours
// the run has; an hour past its end moves to its last hour
void ModelViewer::fitHoursToRun() {
    const bool grib = GfsRender::drawsModel(objectModel.model);
    if (allTimes.empty()) {
        return;
    }
    std::vector<string> wanted = allTimes;
    if (grib && objectModel.run.size() >= 2 && std::isdigit(static_cast<unsigned char>(objectModel.run[0]))) {
        // the rules of the model and the chart (the length of this run, a period that ends at the hour), then what the run's data has: the same answer as the map's chart layer (ChartHours)
        std::vector<int> listed;
        for (const auto& t : allTimes) {
            listed.push_back(std::atoi(t.c_str()));
        }
        const auto rules = ChartHours::rules(objectModel.model, objectModel.param, std::atoi(objectModel.run.substr(0, 2).c_str()), listed);
        wanted.clear();
        for (const auto& t : allTimes) {
            if (std::find(rules.hours.begin(), rules.hours.end(), std::atoi(t.c_str())) != rules.hours.end()) {
                wanted.push_back(t);
            }
        }
        if (wanted.empty()) {
            wanted = allTimes;
        }
        if (runTime(objectModel.run).isValid() && GfsRender::handles(objectModel.model, objectModel.param)) {
            const auto key = hoursKey();
            if (hourBook.wantsAsking(key)) {
                askHours(wanted);
            }
            if (const auto * known = hourBook.find(key); known != nullptr && !known->hours.empty()) {
                std::vector<string> kept;
                for (const auto& t : wanted) {
                    if (std::find(known->hours.begin(), known->hours.end(), std::atoi(t.c_str())) != known->hours.end()) {
                        kept.push_back(t);
                    }
                }
                if (!kept.empty()) {
                    wanted = kept;
                }
            }
        }
    }
    if (wanted == objectModel.times) {
        return;
    }
    const int shown = std::atoi(objectModel.getTime().c_str());
    size_t index = 0;
    for (size_t i = 0; i < wanted.size(); i++) {
        if (std::atoi(wanted[i].c_str()) <= shown) {
            index = i;   // the same hour, else the last one the run has
        }
    }
    objectModel.times = wanted;
    objectModel.setTimeIdx(static_cast<int>(index));
    comboboxTime.block();
    comboboxTime.setList(objectModel.times);
    {
        vector<string> shown;
        for (const auto& t : objectModel.times) {
            shown.push_back(hourText(t, true));
        }
        comboboxTime.setLabels(shown);
    }
    comboboxTime.setIndex(index);
    comboboxTime.unblock();
    if (strip) {
        refreshTimeStrip();
    }
}

string ModelViewer::hoursKey() const {
    const auto when = runTime(objectModel.run);
    return ChartHours::Book::key(objectModel.model, when.isValid() ? when.toString("yyyyMMddHH").toStdString() : objectModel.run, objectModel.param, GfsModels::isStormModel(objectModel.model) ? objectModel.sector : string{});
}

// which of the hours the run has the chart on show for (SPC's store: its time axes; the other models: the run's indexes), asked in the background and kept in the book; the list is fitted to the
// answer when it comes, and asked again every five minutes while hours are still missing (a run being uploaded). The map's chart layer asks the same question of the same code.
void ModelViewer::askHours(const std::vector<string>& hourList) {
    const auto when = runTime(objectModel.run);
    if (!when.isValid()) {
        return;
    }
    const auto key = hoursKey();
    if (!hourBook.begin(key)) {
        return;
    }
    if (!gfsSession) {
        gfsSession = std::make_shared<GfsRender::Session>();
    }
    const auto session = gfsSession;
    const auto model = objectModel.model, param = objectModel.param, storm = GfsModels::isStormModel(model) ? objectModel.sector : string{};
    const auto runId = when.toString("yyyyMMddHH").toStdString();
    auto hours = std::make_shared<std::vector<int>>();
    std::vector<int> asked;
    for (const auto& t : hourList) {
        asked.push_back(std::atoi(t.c_str()));
    }
    new FutureVoid{this, [session, model, param, runId, storm, asked, hours] { *hours = ChartHours::present(*session, model, param, runId, asked, storm); }, [this, key, hours, asked] {
        hourBook.put(key, *hours, asked.size());
        const auto before = objectModel.getTime();
        fitHoursToRun();
        if (objectModel.getTime() != before) {
            reload();
        }
    }};
}

void ModelViewer::reload() {
    applyMainRunRule();
    fitHoursToRun();
    if (buttonPlace != nullptr) {
        buttonPlace->setVisible(objectModel.sector.compare(0, 6, "LOCAL:") == 0);
    }
    if (buttonOnMap != nullptr) {
        buttonOnMap->setVisible(GfsRender::drawsModel(objectModel.model) && GfsRender::handles(objectModel.model, objectModel.param));
    }
    if (checkRadii != nullptr) {
        const bool storm = GfsModels::isStormModel(objectModel.model);
        checkRadii->setVisible(storm);
        buttonIntensity->setVisible(storm && Hafs::isStormId(objectModel.sector));
    }
    if (comboLocalKm != nullptr) {   // the size of an area round a saved location, beside it
        double lat = 0, lon = 0, km = 0;
        string name;
        const bool local = GfsChart::parseLocalId(objectModel.sector, lat, lon, km, name);
        comboLocalKm->setVisible(local);
        if (local) {
            const int at = comboLocalKm->findData(static_cast<int>(km));
            comboLocalKm->blockSignals(true);
            if (at >= 0) {
                comboLocalKm->setCurrentIndex(at);
            } else {
                comboLocalKm->addItem(QString::number(static_cast<int>(km)) + " km square", static_cast<int>(km));
                comboLocalKm->setCurrentIndex(comboLocalKm->count() - 1);
            }
            comboLocalKm->blockSignals(false);
        }
    }
    if (strip) {
        strip->setCurrent(static_cast<int>(comboboxTime.getIndex()));
    }
    objectModel.writePrefs();
    const auto shownIndex = comboboxTime.getIndex();
    setTitle(objectModel.model + " - " + sectorLabel(objectModel.sector) + (shownIndex >= 0 && static_cast<size_t>(shownIndex) < objectModel.times.size() ? " " + objectModel.times[static_cast<size_t>(shownIndex)] : string{}) +
             (runSwappedFrom.empty() ? string{} : "  (shown from the " + objectModel.run + " run: the " + runSwappedFrom + " run does not have this chart)"));
    if (GfsRender::handles(objectModel.model, objectModel.param)) {
        // the GFS charts are drawn here from the GRIB data
        const int mine = ++drawing;
        NetManager::cancelQueued(NetManager::Priority::Ahead);   // the hours being read ahead for the view that was: dropped, so what is wanted now is not behind them
        failedAhead.clear();
        if (!gfsSession) {
            gfsSession = std::make_shared<GfsRender::Session>();   // its folder goes when this screen does
        }
        auto session = gfsSession;
        string runToDraw = objectModel.run;
        if (pinnedValid.isValid() && objectModel.model == pinnedModel) {   // a run of the past: its date as well, not "the newest 18Z"
            const auto t = runTime(objectModel.run);
            if (t.isValid()) {
                runToDraw = t.toString("yyyyMMddHH").toStdString();
            }
        }
        const auto model = objectModel.model, param = objectModel.param, sector = objectModel.sector, run = runToDraw;
        const auto overlayIds = overlays;
        const auto shownVariant = variant;
        const int hour = std::atoi(objectModel.getTime().c_str());
        if (const auto cached = frames.find(frameKey(hour)); cached != frames.end()) {   // drawn already (play, or a visit before)
            showFrame(cached->second);
            prefetch(prefetchGeneration);
            return;
        }
        const auto key = frameKey(hour);
        auto result = std::make_shared<std::pair<QByteArray, string>>();
        auto probe = std::make_shared<GfsChart::Probe>();
        new FutureVoid{this, [=] { result->first = GfsRender::png(*session, model, param, sector, run, hour, overlayIds, result->second, probe.get(), shownVariant); },
                       [this, result, mine, probe, key] {
                           if (mine == drawing && !result->first.isEmpty()) {
                               storeFrame(key, {result->first, probe});
                               showFrame(frames[key]);
                               prefetch(prefetchGeneration);
                           } else if (mine == drawing) {
                               setTitle("GFS - " + result->second);
                           }
                       }};
        return;
    }
    new FutureBytes{this, ObjectModelGet::imageUrl(objectModel), [this] (const auto& ba) {
        image.setBytes(ba);
        if (hover) {
            hover->set(nullptr);
        }
    }};
}

// A hurricane model has a run for each storm it is running: the storms are looked for first (they are the areas of the screen), then the run of the one on show
void ModelViewer::startRun() {
    if (!GfsModels::isStormModel(objectModel.model)) {
        getRun();
        return;
    }
    const int mine = ++stormLoading;
    const auto model = objectModel.model;
    setTitle(model + " - looking for the storms the model is running...");
    auto found = std::make_shared<std::pair<std::vector<string>, string>>();
    new FutureVoid{this, [model, found] { found->first = GfsRender::hafsStorms(model, found->second); }, [this, model, found, mine] {
        if (mine != stormLoading || model != objectModel.model) {
            return;
        }
        objectModel.sectors = found->first;
        if (objectModel.sectors.empty()) {
            objectModel.sector.clear();
            refreshSectorList();
            buttonSector.setText("No storm  \xE2\x96\xBE");
            if (buttonIntensity != nullptr) {
                buttonIntensity->setVisible(false);
            }
            setTitle(model + " has no storm in its newest cycle (it runs only for active storms and invests).");
            return;
        }
        if (std::find(objectModel.sectors.begin(), objectModel.sectors.end(), objectModel.sector) == objectModel.sectors.end()) {
            objectModel.sector = objectModel.sectors.front();
        }
        getRun();
    }};
}

void ModelViewer::getRun() {
    new FutureVoid{this, [this] { getRunStatus(); }, [this] { updateRunStatus(); }};
}

void ModelViewer::getRunStatus() {
    ObjectModelGet::runStatus(objectModel);
    objectModel.run = objectModel.runTimeData.mostRecentRun;
    applyPinnedRun();   // (a run of the past: the date and the cycle are given, not looked for)
}

// The run list says which runs are today's: a run later in the day than the newest one is the one from the day before (the list alone shows "14Z" with no hint that it is yesterday's)
vector<string> ModelViewer::runLabels() const {
    vector<string> labels;
    const auto newest = QDateTime::fromString(QString::fromStdString(objectModel.runTimeData.newestDate + objectModel.runTimeData.mostRecentRun.substr(0, 2)), "yyyyMMddHH");
    if (!newest.isValid() || objectModel.runTimeData.mostRecentRun.size() < 2) {
        return labels;
    }
    auto utc = newest;
    utc.setTimeSpec(Qt::UTC);
    for (const auto& run : objectModel.runs) {
        bool ok = false;
        const int hour = run.size() >= 2 ? QString::fromStdString(run.substr(0, 2)).toInt(&ok) : 0;
        if (!ok) {
            labels.push_back(run);
            continue;
        }
        auto t = QDateTime{utc.date(), QTime{hour, 0}, Qt::UTC};
        if (t > utc) {
            t = t.addDays(-1);
        }
        const int days = static_cast<int>(t.date().daysTo(utc.date()));
        labels.push_back(pinnedValid.isValid() && t == utc ? run + "  " + t.toString("yyyy-MM-dd").toStdString() + " (history)" : t == utc ? run + "  newest" : days == 0 ? run : days == 1 ? run + "  yesterday" : run + "  " + t.toString("MMM d").toStdString());
    }
    return labels;
}

// The saved locations as areas of the model's own: a square of 300, 600 and 1200 kilometers a side centered on each (moved in from the edge of a regional grid), for the models drawn from GRIB
void ModelViewer::addLocalSectors() {
    auto& sectors = objectModel.sectors;
    sectors.erase(std::remove_if(sectors.begin(), sectors.end(), [] (const string& id) { return id.compare(0, 6, "LOCAL:") == 0; }), sectors.end());
    if (!GfsRender::drawsModel(objectModel.model) || GfsModels::isStormModel(objectModel.model)) {
        return;
    }
    // one entry per place, at the size chosen beside the area box (the size of the area on show, when it is one of these)
    double km = std::clamp(Utility::readPrefInt("MODEL_LOCAL_KM", 600), 100, 5000);
    {
        double lat = 0, lon = 0, shownKm = 0;
        string name;
        if (GfsChart::parseLocalId(objectModel.sector, lat, lon, shownKm, name) && shownKm > 0) {
            km = shownKm;
        }
    }
    const auto places = Location::getListLatLons();
    const auto names = Location::listOfNames();
    for (size_t i = 0; i < places.size(); i++) {
        const string name = i < names.size() ? names[i] : string{};
        sectors.push_back(GfsChart::localId(places[i].lat(), places[i].lon(), km, name));
    }
    if (objectModel.sector.compare(0, 6, "LOCAL:") == 0 && std::find(sectors.begin(), sectors.end(), objectModel.sector) == sectors.end()) {
        sectors.push_back(objectModel.sector);   // an area kept from before (a place since renamed or moved, a saved view): still in the list
    }
}

void ModelViewer::refreshSectorList() {
    addLocalSectors();
    comboboxSector.block();
    comboboxSector.setList(objectModel.sectors);
    vector<string> labels;
    for (const auto& id : objectModel.sectors) {
        labels.push_back(sectorLabel(id));
    }
    comboboxSector.setLabels(labels);
    comboboxSector.setIndexByValue(objectModel.sector);
    comboboxSector.unblock();
}

// the hour of the run a past time is looked at from: the same hour for a model with a run every hour (its analysis), else the run before it (00 / 06 / 12 / 18Z, or the model's own step)
int ModelViewer::pinnedRunHour() const {
    const auto * def = GfsModels::find(pinnedModel);
    const int step = def == nullptr ? 6 : def->hourlyRuns ? 1 : std::max(1, def->cycleStep);
    return pinnedValid.time().hour() / step * step;
}

// the run of the past (openAt) in place of the newest one in the run data; done again when the data comes in, whichever search of the newest run finished last
void ModelViewer::applyPinnedRun() {
    if (!pinnedValid.isValid() || objectModel.model != pinnedModel) {
        return;
    }
    const QDateTime run{pinnedValid.date(), QTime{pinnedRunHour(), 0}, QTimeZone::utc()};
    objectModel.runTimeData.newestDate = run.toString("yyyyMMdd").toStdString();
    objectModel.runTimeData.mostRecentRun = run.toString("HH").toStdString() + "Z";
    objectModel.runTimeData.timeStringConversion = objectModel.runTimeData.mostRecentRun;
    objectModel.run = objectModel.runTimeData.mostRecentRun;
}

void ModelViewer::updateRunStatus() {
    applyPinnedRun();
    comboboxTime.block();
    comboboxRun.block();
    comboboxSector.block();
    comboboxProduct.block();
    comboboxModel.block();
    comboboxTime.setList(objectModel.times);
    {
        vector<string> shown;
        for (const auto& t : objectModel.times) {
            shown.push_back(hourText(t, true));
        }
        comboboxTime.setLabels(shown);
    }
    if (objectModel.model == "GLCFS") {
        // pass
    } else if (objectModel.model != "HRRR" && objectModel.model != "HREF") {
        const auto runStart = runTime(objectModel.run);   // the run's own date: a run of the past (the History page) is not today's
        for (auto index : range(objectModel.times.size())) {
            auto timeStr = objectModel.times[index];
            const auto hourText = WString::split(timeStr, " ")[0];
            string text = UtilityModels::convertTimeRuntoTimeString(WString::replace(objectModel.runTimeData.timeStringConversion, "Z", ""), hourText);
            if (runStart.isValid() && pinnedValid.isValid()) {
                const auto local = runStart.addSecs(3600LL * std::atoi(hourText.c_str())).toLocalTime();
                text = local.toString("ddd").toStdString() + "  " + std::to_string(local.time().hour() % 12 == 0 ? 12 : local.time().hour() % 12) + (local.time().hour() >= 12 ? "pm" : "am");
            }
            objectModel.setTimeArr(index, hourText + " " + text);
        }
    } else if (objectModel.prefModel == "SPCHRRR") {
        objectModel.runs = objectModel.runTimeData.listRun;
        objectModel.times = UtilityModels::updateTime(UtilityString::getLastXChars(objectModel.run, 2), objectModel.run, objectModel.times, "");
    } else {
        objectModel.runs = objectModel.runTimeData.listRun;
        objectModel.times = UtilityModels::updateTime(UtilityString::getLastXChars(objectModel.run, 3), objectModel.run, objectModel.times, "");
    }
    if (pinnedValid.isValid() && objectModel.model == pinnedModel && GfsRender::drawsModel(objectModel.model)) {   // a run of the past: the weekday and hour of each of its hours from its own date (the labels above count from today)
        const auto runStart = runTime(objectModel.run);
        if (runStart.isValid()) {
            for (auto index : range(objectModel.times.size())) {
                const auto hourText = WString::split(objectModel.times[index], " ")[0];
                const auto local = runStart.addSecs(3600LL * std::atoi(hourText.c_str())).toLocalTime();
                objectModel.setTimeArr(index, hourText + " " + local.toString("ddd").toStdString() + "  " + std::to_string(local.time().hour() % 12 == 0 ? 12 : local.time().hour() % 12) + (local.time().hour() >= 12 ? "pm" : "am"));
            }
        }
    }
    comboboxModel.setIndexByValue(objectModel.model);

    addLocalSectors();
    comboboxSector.setList(objectModel.sectors);
    {
        vector<string> labels;
        for (const auto& id : objectModel.sectors) {
            labels.push_back(sectorLabel(id));
        }
        comboboxSector.setLabels(labels);
    }
    comboboxSector.setIndexByValue(objectModel.sector);

    comboboxRun.setList(objectModel.runs);
    comboboxRun.setIndexByValue(objectModel.run);
    comboboxRun.setLabels(runLabels());

    {   // the charts made to order (the recent ones, and the one on view) are in the list with the rest
        QStringList built = QString::fromStdString(Utility::readPref("MODEL_BUILT_" + objectModel.model, "")).split(',', Qt::SkipEmptyParts);
        if (objectModel.param.compare(0, 4, "gen~") == 0) {
            built << QString::fromStdString(objectModel.param);
        }
        for (const auto& id : built) {
            const auto name = id.toStdString();
            if (std::find(objectModel.params.begin(), objectModel.params.end(), name) == objectModel.params.end()) {
                if (const auto * product = GfsChart::product(name, objectModel.model)) {
                    objectModel.params.push_back(name);
                    objectModel.paramLabels.push_back(product->label);
                }
            }
        }
    }
    comboboxProduct.setList(objectModel.paramLabels);
    auto paramIndex = findex(objectModel.param, objectModel.params);
    comboboxProduct.setIndex(paramIndex);

    allTimes = objectModel.times;
    comboboxTime.setList(objectModel.times);
    {
        vector<string> shown;
        for (const auto& t : objectModel.times) {
            shown.push_back(hourText(t, true));
        }
        comboboxTime.setLabels(shown);
    }
    comboboxTime.setIndexByValue(objectModel.getTime());
    if (pinnedApply && pinnedValid.isValid()) {   // the hour of the valid time within the run
        pinnedApply = false;
        const QDateTime run{pinnedValid.date(), QTime{pinnedRunHour(), 0}, QTimeZone::utc()};
        const int hour = pinnedRunStart && !objectModel.times.empty() ? std::atoi(objectModel.times.front().c_str()) : static_cast<int>(run.secsTo(pinnedValid) / 3600);   // a run: its first hour
        for (size_t i = 0; i < objectModel.times.size(); i++) {
            if (std::atoi(objectModel.times[i].c_str()) == hour) {
                objectModel.setTimeIdx(i);
                comboboxTime.setIndex(i);
                break;
            }
        }
    }

    comboboxTime.unblock();
    comboboxRun.unblock();
    comboboxSector.unblock();
    comboboxProduct.unblock();
    comboboxModel.unblock();

    refreshProductButton();
    refreshTimeStrip();
    resetTileSpecs();
    if (!pendingView[0].empty() && pendingView[1] == objectModel.model) {   // a saved view of this model, waiting for its runs
        const auto view = pendingView;
        applyView(view);
        return;
    }
    reload();
}

// The charts of a model drawn from GRIB are many: they are chosen in the grouped picker, and the plain list is for the models still fetched as pictures.
void ModelViewer::refreshProductButton() {
    buttonMembers->setVisible(objectModel.model == "REFS");
    const bool ncep = objectModel.prefModel == "NCEP";   // the model guidance site's list is long: the grouped picker
    comboboxModel.setVisible(!ncep);
    buttonModel.setVisible(ncep);
    buttonModel.setText(objectModel.model + "  \xE2\x96\xBE");
    const bool grib = GfsRender::drawsModel(objectModel.model);
    comboboxProduct.setVisible(!grib);
    comboboxSector.setVisible(!grib);   // the areas are many too: the grouped picker
    buttonSector.setVisible(grib);
    buttonSector.setText(sectorLabel(objectModel.sector) + "  \xE2\x96\xBE");
    buttonProducts.setVisible(grib);
    refreshBuildMenu();
    refreshMembers();
    refreshOverlays();
    if (!grib) {
        return;
    }
    string label = objectModel.param;
    for (size_t i = 0; i < objectModel.params.size() && i < objectModel.paramLabels.size(); i++) {
        if (objectModel.params[i] == objectModel.param) {
            label = objectModel.paramLabels[i];
        }
    }
    buttonProducts.setText(label + (overlays.empty() ? "" : " + " + std::to_string(overlays.size())) + "  \xE2\x96\xBE");
}

namespace {
    string sectorGroup(const string& id) {
        return ProductPicker::sectorGroup(id);
    }

    // what a sector is called in a list: the areas round a saved location say the place and the kilometers a side
    string sectorLabel(const string& id) {
        double lat = 0, lon = 0, km = 0;
        string name;
        if (GfsChart::parseLocalId(id, lat, lon, km, name)) {
            return (name.empty() ? string{"Saved location"} : name) + ", " + std::to_string(static_cast<int>(km)) + " km square";
        }
        return Hafs::label(id);
    }
}

void ModelViewer::showModelPicker() {
    if (modelPicker) {
        modelPicker->raise();
        modelPicker->activateWindow();
        return;
    }
    std::vector<ProductPicker::Entry> entries;
    for (const auto& id : objectModel.models) {
        const auto group = GfsModels::pickerGroup(id);
        const auto * def = GfsModels::find(id);   // every model says what it is, so a model is found by who makes it ("ECMWF", "NASA") as well as by its short name
        entries.push_back({id, def && def->label != id ? id + "  -  " + def->label : id, group});
    }
    static const std::vector<string> order{"Global", "Regional", "Convection allowing", "Ensembles", "Waves and ocean", "Tropical cyclones", "Other"};
    std::stable_sort(entries.begin(), entries.end(), [] (const ProductPicker::Entry& a, const ProductPicker::Entry& b) {
        return std::find(order.begin(), order.end(), a.group) < std::find(order.begin(), order.end(), b.group);
    });
    modelPicker = new ProductPicker{this, "NCEP", entries, objectModel.model, {}, {}, {}};
    modelPicker->setWording("models", "Show this model");
    modelPicker->resize(480, 560);
    modelPicker->onPick = [this] (const string& id) {
        const auto items = comboboxModel.getItems();
        const auto found = std::find(items.begin(), items.end(), id);
        if (found != items.end()) {
            comboboxModel.block();
            comboboxModel.setIndex(static_cast<size_t>(found - items.begin()));
            comboboxModel.unblock();
            changeModel(static_cast<int>(found - items.begin()));
        }
    };
    modelPicker->show();
}

void ModelViewer::showSectorPicker() {
    if (sectorPicker) {
        sectorPicker->raise();
        sectorPicker->activateWindow();
        return;
    }
    const auto model = objectModel.model;
    std::vector<ProductPicker::Entry> entries;
    for (const auto& id : objectModel.sectors) {
        entries.push_back({id, sectorLabel(id), sectorGroup(id)});
    }
    // the groups in a fixed order: the entries are listed as the model gives them
    std::stable_sort(entries.begin(), entries.end(), [] (const ProductPicker::Entry& a, const ProductPicker::Entry& b) {
        static const std::vector<string> order{"Around a saved location", "United States", "North America and the Caribbean", "Continents", "Oceans", "World and poles", "Storms in the model", "Other"};
        return std::find(order.begin(), order.end(), a.group) < std::find(order.begin(), order.end(), b.group);
    });
    std::vector<string> favorites;
    const auto stored = Utility::readPref("SECTORFAV_" + model, "");
    for (size_t at = 0; at < stored.size();) {
        auto end = stored.find(',', at);
        end = end == string::npos ? stored.size() : end;
        if (end > at) {
            favorites.push_back(stored.substr(at, end - at));
        }
        at = end + 1;
    }
    sectorPicker = new ProductPicker{this, model, entries, objectModel.sector, favorites, {}, {}};
    sectorPicker->setWording("areas", "Show this area");
    sectorPicker->resize(360, 560);
    sectorPicker->onPick = [this] (const string& id) {
        objectModel.sector = id;
        comboboxSector.block();
        comboboxSector.setIndexByValue(id);
        comboboxSector.unblock();
        refreshProductButton();
        reload();
    };
    sectorPicker->onFavorites = [model] (const std::vector<string>& ids) {
        string joined;
        for (const auto& id : ids) {
            joined += (joined.empty() ? "" : ",") + id;
        }
        Utility::writePref("SECTORFAV_" + model, joined);
    };
    sectorPicker->show();
}

void ModelViewer::showPicker() {
    if (picker) {
        picker->raise();
        picker->activateWindow();
        return;
    }
    const auto model = objectModel.model;
    std::vector<ProductPicker::Entry> entries;
    for (size_t i = 0; i < objectModel.params.size() && i < objectModel.paramLabels.size(); i++) {
        const auto * product = GfsChart::product(objectModel.params[i], model);
        entries.push_back({objectModel.params[i], objectModel.paramLabels[i], product ? GfsChart::category(*product) : string{"Other"}});
    }
    std::vector<string> favorites;
    const auto stored = Utility::readPref("MODELFAV_" + model, "");
    for (size_t at = 0; at < stored.size();) {
        auto end = stored.find(',', at);
        end = end == string::npos ? stored.size() : end;
        if (end > at) {
            favorites.push_back(stored.substr(at, end - at));
        }
        at = end + 1;
    }
    picker = new ProductPicker{this, model, entries, objectModel.param, favorites, GfsChart::overlayChoices(model), overlays};
    picker->onPick = [this] (const string& id) {
        objectModel.param = id;
        comboboxProduct.block();
        comboboxProduct.setIndexByValue(id);
        comboboxProduct.unblock();
        refreshProductButton();
        reload();
    };
    picker->onOverlays = [this] (const std::vector<string>& ids) {
        overlays = ids;
        refreshProductButton();
        reload();
    };
    picker->onFavorites = [model] (const std::vector<string>& ids) {
        string joined;
        for (const auto& id : ids) {
            joined += (joined.empty() ? "" : ",") + id;
        }
        Utility::writePref("MODELFAV_" + model, joined);
    };
    picker->show();
}

// ---- the comparison tiles ----

int ModelViewer::tileCount() const {
    return std::clamp(comboTiles.getIndex(), 0, 3);
}

// the ensembles the pooled model is made of, in the order the tiles show them
const std::vector<string>& ModelViewer::partsOfPool() {
    static const std::vector<string> parts{"IFS-ENS", "GEFS", "GEPS"};
    return parts;
}

ModelViewer::TilesShow ModelViewer::tilesShow() const {
    const auto i = comboTilesShow.getIndex();
    return i == 1 ? TilesShow::Models : i == 2 ? TilesShow::Runs : i == 3 ? TilesShow::Members : i == 4 ? TilesShow::ModelsByTime : i == 5 ? TilesShow::Parts : TilesShow::Charts;
}

void ModelViewer::applyTileLayout() {
    const int n = tileCount();
    for (auto * widget : std::vector<QWidget *>{mainTile, tiles[0], tiles[1], tiles[2]}) {
        grid->removeWidget(widget);
    }
    mainCaption->setVisible(n > 0);
    mainTile->setFrameShape(n > 0 ? QFrame::StyledPanel : QFrame::NoFrame);
    grid->addWidget(mainTile, 0, 0);
    // 1 x 2: side by side; 1 x 3: a row of three; 2 x 2: two rows of two
    for (int i = 0; i < 3; i++) {
        const bool used = i < n;
        tiles[static_cast<size_t>(i)]->setVisible(used);
        if (used) {
            const int row = n == 3 ? (i + 1) / 2 : 0;
            const int col = n == 3 ? (i + 1) % 2 : i + 1;
            grid->addWidget(tiles[static_cast<size_t>(i)], row, col);
        }
    }
    const int columns = n == 3 ? 2 : n + 1;
    for (int c = 0; c < 3; c++) {
        grid->setColumnStretch(c, c < columns ? 1 : 0);
    }
    for (int r = 0; r < 2; r++) {
        grid->setRowStretch(r, r == 0 || n == 3 ? 1 : 0);
    }
    mainTile->setVisible(true);
}

// what the other tiles show to begin with: the next charts of the list (favorites first), the other models that draw this chart, or the older runs 6, 12 and 18 hours back
void ModelViewer::resetTileSpecs() {
    const auto show = tilesShow();
    std::vector<string> others;
    if (show == TilesShow::Charts) {
        const auto stored = Utility::readPref("MODELFAV_" + objectModel.model, "");
        for (const auto& id : QString::fromStdString(stored).split(',', Qt::SkipEmptyParts)) {
            const auto name = id.toStdString();
            if (name != objectModel.param && std::find(objectModel.params.begin(), objectModel.params.end(), name) != objectModel.params.end()) {
                others.push_back(name);
            }
        }
        const auto at = std::find(objectModel.params.begin(), objectModel.params.end(), objectModel.param);
        const size_t start = at == objectModel.params.end() ? 0 : static_cast<size_t>(at - objectModel.params.begin()) + 1;
        for (size_t i = 0; i < objectModel.params.size(); i++) {
            const auto& name = objectModel.params[(start + i) % objectModel.params.size()];
            if (name != objectModel.param && std::find(others.begin(), others.end(), name) == others.end()) {
                others.push_back(name);
            }
        }
    } else if (show == TilesShow::Models || show == TilesShow::ModelsByTime) {
        for (const auto& def : GfsModels::all()) {
            if (!def.storm && def.id != objectModel.model && GfsRender::handles(def.id, objectModel.param)) {
                others.push_back(def.id);
            }
        }
    }
    for (size_t k = 0; k < tileSpecs.size(); k++) {
        Spec spec{objectModel.model, objectModel.param, objectModel.sector, 0};
        if (show == TilesShow::Charts) {
            spec.param = k < others.size() ? others[k] : objectModel.param;
        } else if (show == TilesShow::Models || show == TilesShow::ModelsByTime) {
            spec.model = k < others.size() ? others[k] : string{};
        } else if (show == TilesShow::Members) {
            spec.member.clear();   // the next members after the first tile's (tileMember)
        } else if (show == TilesShow::Parts) {
            spec.member = k < partsOfPool().size() ? partsOfPool()[k] : string{};
        } else {
            spec.shift = 6 * static_cast<int>(k + 1);
        }
        tileSpecs[k] = spec;
    }
}

// the actual date and hour of a run of the list (the list gives only the hour: a later hour than the newest run's is the day before)
QDateTime ModelViewer::runTime(const string& run) const {
    const auto& data = objectModel.runTimeData;
    if (data.newestDate.empty() || data.mostRecentRun.size() < 2 || run.size() < 2) {
        return {};
    }
    auto newest = QDateTime::fromString(QString::fromStdString(data.newestDate + data.mostRecentRun.substr(0, 2)), "yyyyMMddHH");
    bool ok = false;
    const int hour = QString::fromStdString(run.substr(0, 2)).toInt(&ok);
    if (!newest.isValid() || !ok) {
        return {};
    }
    newest.setTimeSpec(Qt::UTC);
    auto t = QDateTime{newest.date(), QTime{hour, 0}, Qt::UTC};
    return t > newest ? t.addDays(-1) : t;
}

string ModelViewer::tileMember(int index) const {
    const auto& spec = tileSpecs[static_cast<size_t>(index) - 1];
    if (spec.memberPicked && std::find(memberValues.begin(), memberValues.end(), spec.member) != memberValues.end()) {
        return spec.member;
    }
    if (memberValues.empty()) {
        return {};
    }
    const auto at = std::find(memberValues.begin(), memberValues.end(), member);
    const size_t base = at == memberValues.end() ? 0 : static_cast<size_t>(at - memberValues.begin());
    return memberValues[(base + static_cast<size_t>(index)) % memberValues.size()];
}

string ModelViewer::memberLabel(const string& value) const {
    const auto at = std::find(memberValues.begin(), memberValues.end(), value);
    return at != memberValues.end() && static_cast<size_t>(at - memberValues.begin()) < memberLabels.size() ? memberLabels[static_cast<size_t>(at - memberValues.begin())] : value;
}

// what the tile at `index` (0 the screen's own chart) draws at the hour shown by the first tile
ModelViewer::Job ModelViewer::makeJob(int index, int mainHour, bool ahead) const {
    Job job;
    job.hour = mainHour;
    if (index == 0) {
        job.model = objectModel.model;
        job.param = objectModel.param;
        job.sector = objectModel.sector;
        job.cycle = objectModel.run;
        if (pinnedValid.isValid() && objectModel.model == pinnedModel) {   // a run of the past: its date as well, not "the newest 18Z"
            const auto t = runTime(objectModel.run);
            if (t.isValid()) {
                job.cycle = t.toString("yyyyMMddHH").toStdString();
            }
        }
            job.overlays = overlays;
        job.variant = ahead && variant.kind != GfsRender::Variant::Kind::Change ? GfsRender::Variant{} : variant;
        job.variant.member = member;
        job.variant.standardDeviations = standardDeviations;
        job.variant.overlayModels = overlayModels;
        job.key = frameKey(mainHour);
    } else {
        const auto& spec = tileSpecs[static_cast<size_t>(index) - 1];
        const auto show = tilesShow();
        job.model = show == TilesShow::Members ? objectModel.model : spec.model;
        job.param = show == TilesShow::Members ? objectModel.param : spec.param;
        job.sector = objectModel.sector;
        job.cycle = objectModel.run;
        if (show == TilesShow::Members && memberValues.size() < 2) {
            job.ok = false;
            job.why = "This chart has no single members to compare: pick an ensemble chart made of one member (a GEFS chart, a REFS or SPC REFS 'Member' chart).";
        } else if (show == TilesShow::Parts && (objectModel.model != "ENS-ALL" || spec.member.empty())) {
            job.ok = false;
            job.why = "Pick the ENS-ALL model (the pool of the ECMWF, GEFS and GEPS members) to compare it with its parts.";
        } else if (job.model.empty()) {
            job.ok = false;
            job.why = "No other model draws this chart.";
        } else if (show == TilesShow::Runs) {
            const auto t = runTime(objectModel.run);
            if (!t.isValid()) {
                job.ok = false;
                job.why = "The date of the run is not known yet.";
            } else {
                job.cycle = t.addSecs(-3600LL * spec.shift).toString("yyyyMMddHH").toStdString();
                job.hour = mainHour + spec.shift;   // the same valid time
            }
        } else if (show == TilesShow::Models || show == TilesShow::ModelsByTime) {
            job.cycle = spec.model == objectModel.model ? objectModel.run : string{};   // another model: its newest run
        }
        if (job.ok && !GfsRender::handles(job.model, job.param)) {
            job.ok = false;
            job.why = job.model + " does not draw " + job.param + ".";
        }
        if (spec.model == objectModel.model) {
            job.overlays = overlays;
        }
        auto v = variant;
        if (v.kind == GfsRender::Variant::Kind::Max && !ahead) {   // the maximum of the same period on every tile: the hours that model has (another model), or the same valid times of the older run
            std::vector<int> hours;
            const auto * def = GfsModels::find(job.model);
            for (const int h : v.hours) {
                const int hourHere = show == TilesShow::Runs ? h + spec.shift : h;
                bool offered = def == nullptr;
                for (const auto& range : def ? def->hours : std::vector<GfsModels::Hours>{}) {
                    offered = offered || (hourHere >= range.from && hourHere <= range.to && (hourHere - range.from) % range.step == 0);
                }
                if (offered) {
                    hours.push_back(hourHere);
                }
            }
            if (hours.size() < 2 && job.ok) {
                job.ok = false;
                job.why = "Not enough hours of " + job.model + " in the range of the maximum.";
            }
            v.hours = hours;
        }
        if (ahead && v.kind != GfsRender::Variant::Kind::Change) {
            v = {};
        }
        if (show == TilesShow::Parts) {   // the same pooled chart from the members of one ensemble
            v.poolOnly = spec.member;
        }
        if (show == TilesShow::Members) {   // each tile its own member of the same chart
            v.member = tileMember(index);
            v.standardDeviations = standardDeviations;
        } else if (spec.model == objectModel.model) {   // the same model's tiles show the member too (another model's charts have none)
            v.member = member;
            v.standardDeviations = standardDeviations;
        }
        if (show == TilesShow::ModelsByTime && job.ok) {   // by the clock: each model's own hour for the first tile's valid time
            const auto start = runTime(objectModel.run);
            if (!start.isValid()) {
                job.ok = false;
                job.why = "The date of the run is not known yet.";
            } else {
                v.validTime = start.addSecs(3600LL * mainHour).toString("yyyyMMddHH").toStdString();
            }
        }
        job.variant = v;
        string vk;
        if (v.kind == GfsRender::Variant::Kind::Change) {
            vk = "|change" + std::to_string(v.hoursBack);
        } else if (v.kind == GfsRender::Variant::Kind::Max && !v.hours.empty()) {
            vk = "|max" + std::to_string(v.hours.front()) + "-" + std::to_string(v.hours.back());
        }
        job.key = job.model + "|" + job.param + "|" + job.sector + "|" + job.cycle + "|" + std::to_string(job.hour);
        for (const auto& id : job.overlays) {
            job.key += "|" + id;
        }
        job.key += vk;
        if (!v.member.empty()) {
            job.key += "|member=" + v.member;
        }
        if (v.standardDeviations) {
            job.key += "|sd";
        }
        if (!v.poolOnly.empty()) {
            job.key += "|only=" + v.poolOnly;
        }
        if (!v.validTime.empty()) {
            job.key += "|vt=" + v.validTime;
        }
    }
    job.chart = job.model + "|" + job.param + "|" + job.sector;
    for (const auto& id : job.overlays) {
        job.chart += "|" + id;
    }
    return job;
}

string ModelViewer::caption(const Job& job) const {
    const auto * product = GfsChart::product(job.param, job.model);
    string run;
    if (job.cycle.size() == 10) {
        const auto t = QDateTime::fromString(QString::fromStdString(job.cycle), "yyyyMMddHH");
        run = t.toString("HH").toStdString() + "Z " + t.toString("MMM d").toStdString();
    } else {
        run = job.cycle.empty() ? string{"newest run"} : job.cycle;
    }
    return (job.variant.poolOnly.empty() ? job.model : (job.variant.poolOnly == "IFS-ENS" ? string{"ECMWF"} : job.variant.poolOnly) + " only") + "   " + (product ? product->label : job.param) + "   " + run + (job.variant.member.empty() ? string{} : "   " + memberLabel(job.variant.member)) +
        (job.variant.kind == GfsRender::Variant::Kind::Change ? "   (change)" : "");
}

void ModelViewer::renderTiles() {
    for (int i = 1; i <= tileCount(); i++) {
        renderTile(i);
    }
}

void ModelViewer::renderTile(int index) {
    if (!GfsRender::handles(objectModel.model, objectModel.param)) {
        return;
    }
    auto * tile = tiles[static_cast<size_t>(index) - 1];
    const int mainHour = std::atoi(objectModel.getTime().c_str());
    const auto job = makeJob(index, mainHour, false);
    tile->caption->setText(QString::fromStdString(caption(job)));
    const int mine = ++tileGeneration[static_cast<size_t>(index) - 1];
    if (!job.ok) {
        tile->setMessage(QString::fromStdString(job.why));
        return;
    }
    if (const auto cached = frames.find(job.key); cached != frames.end()) {
        showTile(index, job, cached->second);
        return;
    }
    if (!gfsSession) {
        gfsSession = std::make_shared<GfsRender::Session>();
    }
    auto session = gfsSession;
    auto result = std::make_shared<std::pair<QByteArray, string>>();
    auto probe = std::make_shared<GfsChart::Probe>();
    new FutureVoid{this, [=] { result->first = GfsRender::png(*session, job.model, job.param, job.sector, job.cycle, job.hour, job.overlays, result->second, probe.get(), job.variant); },
                   [this, result, probe, job, index, mine] {
                       if (mine != tileGeneration[static_cast<size_t>(index) - 1] || index > tileCount()) {
                           return;
                       }
                       if (result->first.isEmpty()) {
                           tiles[static_cast<size_t>(index) - 1]->setMessage(QString::fromStdString(result->second.empty() ? string{"Could not draw this chart."} : result->second));
                           return;
                       }
                       storeFrame(job.key, {result->first, probe});
                       showTile(index, job, frames[job.key]);
                       prefetch(prefetchGeneration);
                   }};
}

void ModelViewer::showTile(int index, const Job& job, const Frame& frame) {
    auto * tile = tiles[static_cast<size_t>(index) - 1];
    tile->setChart(frame.bytes, frame.probe, job.chart);
    const auto v = image.view();
    if (!v.fitted) {
        syncingViews = true;
        tile->image->setView(v);
        syncingViews = false;
    }
}

// the button of a tile: another chart, another model or another run, as the comparison is set
void ModelViewer::changeTile(int index) {
    auto& spec = tileSpecs[static_cast<size_t>(index) - 1];
    auto * button = tiles[static_cast<size_t>(index) - 1]->change;
    const auto show = tilesShow();
    if (show == TilesShow::Charts) {
        std::vector<ProductPicker::Entry> entries;
        for (size_t i = 0; i < objectModel.params.size() && i < objectModel.paramLabels.size(); i++) {
            const auto * product = GfsChart::product(objectModel.params[i], objectModel.model);
            entries.push_back({objectModel.params[i], objectModel.paramLabels[i], product ? GfsChart::category(*product) : string{"Other"}});
        }
        std::vector<string> favorites;
        for (const auto& id : QString::fromStdString(Utility::readPref("MODELFAV_" + objectModel.model, "")).split(',', Qt::SkipEmptyParts)) {
            favorites.push_back(id.toStdString());
        }
        auto * choose = new ProductPicker{this, objectModel.model, entries, spec.param, favorites, {}, {}};
        choose->setAttribute(Qt::WA_DeleteOnClose);
        choose->onPick = [this, index] (const string& id) {
            tileSpecs[static_cast<size_t>(index) - 1].param = id;
            renderTile(index);
            prefetch(prefetchGeneration);
        };
        choose->show();
        return;
    }
    QMenu menu;
    if (show == TilesShow::Models || show == TilesShow::ModelsByTime) {
        for (const auto& def : GfsModels::all()) {
            if (def.storm || !GfsRender::handles(def.id, objectModel.param)) {
                continue;
            }
            const auto id = def.id;
            auto * action = menu.addAction(QString::fromStdString(id));
            action->setCheckable(true);
            action->setChecked(spec.model == id);
            QObject::connect(action, &QAction::triggered, this, [this, index, id] {
                tileSpecs[static_cast<size_t>(index) - 1].model = id;
                renderTile(index);
                prefetch(prefetchGeneration);
            });
        }
    } else if (show == TilesShow::Parts) {
        for (const auto& part : partsOfPool()) {
            auto * action = menu.addAction(QString::fromStdString(part + " members only"));
            action->setCheckable(true);
            action->setChecked(spec.member == part);
            QObject::connect(action, &QAction::triggered, this, [this, index, part] {
                tileSpecs[static_cast<size_t>(index) - 1].member = part;
                renderTile(index);
                prefetch(prefetchGeneration);
            });
        }
    } else if (show == TilesShow::Members) {
        for (size_t i = 0; i < memberValues.size(); i++) {
            const auto value = memberValues[i];
            auto * action = menu.addAction(QString::fromStdString(i < memberLabels.size() ? memberLabels[i] : value));
            action->setCheckable(true);
            action->setChecked(tileMember(index) == value);
            QObject::connect(action, &QAction::triggered, this, [this, index, value] {
                auto& picked = tileSpecs[static_cast<size_t>(index) - 1];
                picked.member = value;
                picked.memberPicked = true;
                renderTile(index);
                prefetch(prefetchGeneration);
            });
        }
    } else {
        for (const int hours : {6, 12, 18, 24, 36, 48}) {
            auto * action = menu.addAction(QString{"The run %1 hours older"}.arg(hours));
            action->setCheckable(true);
            action->setChecked(spec.shift == hours);
            QObject::connect(action, &QAction::triggered, this, [this, index, hours] {
                tileSpecs[static_cast<size_t>(index) - 1].shift = hours;
                renderTile(index);
                prefetch(prefetchGeneration);
            });
        }
    }
    menu.exec(button->mapToGlobal(QPoint{0, button->height()}));
}

// a click in a tile: the point is marked in all of them and the sounding is for the tile's own chart
void ModelViewer::pickFromTiles(int source, double fx, double fy) {
    const auto probe = tiles[static_cast<size_t>(source) - 1]->probe;
    double lon = 0.0, lat = 0.0;
    if (probe && probe->locate(fx, fy, lon, lat)) {
        image.setMarker(fx, fy);
        for (auto * tile : tiles) {
            tile->image->setMarker(fx, fy);
        }
        soundingPick.pick(lon, lat);
    }
}

// all the tiles in one picture, each under its caption
QByteArray ModelViewer::compositePicture() const {
    struct Cell {
        QImage image;
        QString text;
    };
    std::vector<Cell> cells;
    cells.push_back({QImage::fromData(shownBytes), mainCaption->text()});
    for (int i = 0; i < tileCount(); i++) {
        cells.push_back({QImage::fromData(tiles[static_cast<size_t>(i)]->shownBytes), tiles[static_cast<size_t>(i)]->caption->text()});
    }
    const int cellWidth = 900, head = 30;
    const int columns = tileCount() == 3 ? 2 : tileCount() + 1;
    const int rows = (static_cast<int>(cells.size()) + columns - 1) / columns;
    std::vector<int> rowHeight(static_cast<size_t>(rows), 100);
    for (size_t i = 0; i < cells.size(); i++) {
        if (!cells[i].image.isNull()) {
            cells[i].image = cells[i].image.scaledToWidth(cellWidth, Qt::SmoothTransformation);
            auto& h = rowHeight[i / static_cast<size_t>(columns)];
            h = std::max(h, cells[i].image.height());
        }
    }
    int total = 0;
    for (const int h : rowHeight) {
        total += h + head;
    }
    QImage out{cellWidth * columns, total, QImage::Format_RGB32};
    out.fill(Qt::white);
    QPainter painter{&out};
    int y = 0;
    for (size_t r = 0; r < static_cast<size_t>(rows); r++) {
        for (size_t c = 0; c < static_cast<size_t>(columns); c++) {
            const size_t i = r * static_cast<size_t>(columns) + c;
            if (i >= cells.size()) {
                break;
            }
            const int x = static_cast<int>(c) * cellWidth;
            auto font = painter.font();
            font.setBold(true);
            font.setPixelSize(16);
            painter.setFont(font);
            painter.setPen(Qt::black);
            painter.drawText(QRect{x + 6, y, cellWidth - 12, head}, Qt::AlignVCenter | Qt::AlignLeft, cells[i].text);
            painter.drawImage(x, y + head, cells[i].image);
        }
        y += rowHeight[r] + head;
    }
    painter.end();
    QByteArray bytes;
    QBuffer buffer{&bytes};
    buffer.open(QIODevice::WriteOnly);
    out.save(&buffer, "PNG");
    return bytes;
}

// ---- charts made to order ----

void ModelViewer::refreshBuildMenu() {
    if (!buttonBuild) {
        return;
    }
    const auto templates = GfsRender::drawsModel(objectModel.model) ? GfsChart::templates(objectModel.model) : std::vector<GfsChart::Template>{};
    buttonBuild->setVisible(!templates.empty());
    menuBuild->clear();
    if (templates.empty()) {
        return;
    }
    QObject::connect(menuBuild->addAction("New chart..."), &QAction::triggered, this, [this] { buildChart(); });
    const auto stored = QString::fromStdString(Utility::readPref("MODEL_BUILT_" + objectModel.model, "")).split(',', Qt::SkipEmptyParts);
    bool first = true;
    for (const auto& id : stored) {
        const auto * product = GfsChart::product(id.toStdString(), objectModel.model);
        if (!product) {
            continue;
        }
        if (first) {
            menuBuild->addSeparator();
            first = false;
        }
        const auto chart = id.toStdString();
        QObject::connect(menuBuild->addAction(QString::fromStdString(product->label)), &QAction::triggered, this, [this, chart] { showBuilt(chart); });
    }
}

void ModelViewer::buildChart() {
    auto * builder = new ChartBuilder{this, objectModel.model, GfsChart::templates(objectModel.model)};
    builder->onBuilt = [this] (const string& id) { showBuilt(id); };
    builder->show();
}

// a chart made to order goes into the model's chart list (and the recent ones of the Build menu), then is drawn
void ModelViewer::showBuilt(const string& id) {
    const auto * product = GfsChart::product(id, objectModel.model);
    if (!product) {
        return;
    }
    if (std::find(objectModel.params.begin(), objectModel.params.end(), id) == objectModel.params.end()) {
        objectModel.params.push_back(id);
        objectModel.paramLabels.push_back(product->label);
    }
    QStringList recent{QString::fromStdString(id)};
    for (const auto& old : QString::fromStdString(Utility::readPref("MODEL_BUILT_" + objectModel.model, "")).split(',', Qt::SkipEmptyParts)) {
        if (old.toStdString() != id && recent.size() < 8) {
            recent << old;
        }
    }
    Utility::writePref("MODEL_BUILT_" + objectModel.model, recent.join(',').toStdString());
    objectModel.param = id;
    comboboxProduct.block();
    comboboxProduct.setList(objectModel.paramLabels);
    comboboxProduct.setIndexByValue(objectModel.paramLabels.back());
    comboboxProduct.unblock();
    refreshProductButton();
    reload();
}

// the member plume graph (RefsPointGraph) for the REFS chart on screen: which member field a chart is made from, and the threshold of the probability charts
void ModelViewer::openMemberGraph() {
    const auto& id = objectModel.param;
    const auto has = [&id] (const char * part) { return id.find(part) != string::npos; };
    string key;
    double threshold = std::nan("");
    if (has("refc")) {
        key = "refc";
        threshold = has("prob_refc_50") ? 50.0 : has("member") ? std::nan("") : 40.0;
    } else if (has("uphl")) {
        key = "uphl25";
        threshold = has("prob_uphl") || has("combo_uphl") ? 75.0 : std::nan("");
    } else if (has("tmp2m") || has("2m_temp") || has("temp_spread")) {
        key = "tmp2m";
    } else if (has("cape")) {
        key = "cape";
    } else if (has("gust")) {
        key = "gust";
    }
    int index = -1;
    for (size_t i = 0; i < UtilityRefs::fields.size(); i++) {
        if (UtilityRefs::fields[i].key == key + "_m1") {
            index = static_cast<int>(i);
        }
    }
    UtilityRefs::MemberBasis basis;
    if (key.empty() || index < 0 || !UtilityRefs::memberBasis(index, std::nan(""), basis)) {
        setTitle("Model screen - the members graph is for the REFS reflectivity, temperature, CAPE, gust and updraft helicity charts");
        return;
    }
    double lon = 0.0, lat = 0.0;
    if (!soundingPick.point(lon, lat)) {
        setTitle("Model screen - click the map to pick a point for the members graph first");
        return;
    }
    if (!std::isnan(threshold)) {
        basis.hasThreshold = true;
        basis.threshold = threshold;
    }
    const auto run = runTime(objectModel.run);
    new RefsPointGraph{this, basis, lon, lat, run.isValid() ? run.toString("yyyyMMddHH").toStdString() : string{}};
}

// the member choices of the chart on screen: the combo shows for the ensembles whose chart can be drawn from one member, and goes back to the mean when the chart or the model changes
void ModelViewer::refreshMembers() {
    std::vector<GfsChart::MemberChoice> choices;
    if (GfsRender::drawsModel(objectModel.model)) {
        if (const auto * product = GfsChart::product(objectModel.param, objectModel.model)) {
            const auto when = runTime(objectModel.run);   // the run on show, for the names of SPC REFS members
            choices = GfsChart::memberChoices(*product, when.isValid() ? when.toString("yyyyMMddHH").toStdString() : string{});
        }
    }
    std::vector<string> values;
    std::vector<string> labels;
    for (const auto& c : choices) {
        values.push_back(c.value);
        labels.push_back(c.label);
    }
    if (values != memberValues) {
        memberValues = values;
        memberLabels = labels;
        for (auto& spec : tileSpecs) {   // a member picked for a tile of another chart is not one of this chart's
            spec.memberPicked = false;
        }
        member = memberValues.empty() ? string{} : memberValues.front();
        comboMember.block();
        comboMember.setList(labels);
        comboMember.setIndex(0);
        comboMember.unblock();
    } else if (labels != memberLabels) {   // the same members, named now (SPC REFS knows its members' names once a run has been opened)
        memberLabels = labels;
        const auto at = comboMember.getIndex();
        comboMember.block();
        comboMember.setList(labels);
        comboMember.setIndex(static_cast<size_t>(std::max(at, 0)));
        comboMember.unblock();
    }
    comboMember.setVisible(!memberValues.empty());
    const auto * shown = GfsRender::drawsModel(objectModel.model) ? GfsChart::product(objectModel.param, objectModel.model) : nullptr;
    const bool offered = shown != nullptr && GfsChart::hasStandardDeviations(*shown);
    if (!offered && standardDeviations) {
        standardDeviations = false;
        buttonSigma->blockSignals(true);
        buttonSigma->setChecked(false);
        buttonSigma->blockSignals(false);
    }
    buttonSigma->setVisible(offered);
}

void ModelViewer::editColors() {
    const auto * product = GfsRender::drawsModel(objectModel.model) ? GfsChart::product(objectModel.param, objectModel.model) : nullptr;
    if (product == nullptr) {
        setTitle("Model screen - the color editor is for the charts drawn from GRIB (not the pictures fetched from other sites)");
        return;
    }
    if (variant.kind == GfsRender::Variant::Kind::Change) {
        setTitle("Model screen - a change chart has its own blue / red scale; show the plain chart to edit its colors");
        return;
    }
    const bool magColors = Utility::readPref("MAG_COLORS", "true").compare(0, 1, "t") == 0;
    const auto ramp = GfsChart::defaultRamp(*product, magColors);
    if (ramp.stops.empty()) {
        setTitle("Model screen - this chart is lines only, it has no color scale");
        return;
    }
    new PaletteEditor{this, product->label, ramp, product->quantity, UIPreferences::unitsF, [this] {
        frames.clear();   // every picture drawn so far was drawn with the old colors
        frameOrder.clear();
        frameBytes = 0;
        failedAhead.clear();
        refreshLoaded();
        reload();
    }};
}

void ModelViewer::refreshOverlays() {
    if (buttonOverlay == nullptr) {
        return;
    }
    const bool grib = GfsRender::drawsModel(objectModel.model);
    std::vector<string> kept;
    for (const auto& other : overlayModels) {
        if (grib && other != objectModel.model && GfsChart::product(objectModel.param, other) != nullptr) {
            kept.push_back(other);
        }
    }
    overlayModels = kept;
    buttonOverlay->setVisible(grib);
    buttonOverlay->setText(overlayModels.empty() ? "Overlay models..." : "Overlay: " + QString::number(overlayModels.size()) + " model" + (overlayModels.size() == 1 ? "" : "s") + "...");
}

void ModelViewer::chooseOverlays() {
    const auto * here = GfsRender::drawsModel(objectModel.model) ? GfsChart::product(objectModel.param, objectModel.model) : nullptr;
    if (here == nullptr || here->contours.empty()) {
        setTitle("Model screen - pick a chart with lines (heights or pressure) to put other models' lines over it");
        return;
    }
    QDialog dialog{this};
    dialog.setWindowTitle("Overlay models");
    auto * layout = new QVBoxLayout{&dialog};
    auto * about = new QLabel{"Lines of " + QString::fromStdString(here->label) + " from these models are drawn over " + QString::fromStdString(objectModel.model) +
                              "'s, for the same valid time, each in its own color (at most four). The fill is left out so the lines can be read. Models are listed when they draw this chart.", &dialog};
    about->setWordWrap(true);
    layout->addWidget(about);
    std::vector<QCheckBox *> boxes;
    for (const auto& def : GfsModels::all()) {
        if (def.id == objectModel.model || def.storm || GfsChart::product(objectModel.param, def.id) == nullptr) {
            continue;
        }
        auto * box = new QCheckBox{QString::fromStdString(def.id + "   " + def.label), &dialog};
        box->setProperty("model", QString::fromStdString(def.id));
        box->setChecked(std::find(overlayModels.begin(), overlayModels.end(), def.id) != overlayModels.end());
        layout->addWidget(box);
        boxes.push_back(box);
    }
    if (boxes.empty()) {
        layout->addWidget(new QLabel{"No other model draws this chart.", &dialog});
    }
    auto * buttons = new QDialogButtonBox{QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog};
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    std::vector<string> chosen;
    for (auto * box : boxes) {
        if (box->isChecked() && chosen.size() < 4) {
            chosen.push_back(box->property("model").toString().toStdString());
        }
    }
    if (chosen == overlayModels) {
        return;
    }
    overlayModels = chosen;
    refreshOverlays();
    startPlaying(false);
    strip->stop();
    frames.clear();
    frameOrder.clear();
    frameBytes = 0;
    failedAhead.clear();
    refreshLoaded();
    reload();
}
