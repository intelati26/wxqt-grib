// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "ModelViewer.h"
#include "ui/ActivityLabel.h"
#include <QBuffer>
#include <QImage>
#include <QInputDialog>
#include <QPainter>
#include <QMenu>
#include "misc/ImageViewer.h"
#include "objects/UtilityAnimationExport.h"
#include "objects/NetManager.h"
#include <QPushButton>
#include "models/CamsViewer.h"
#include "models/ChartBuilder.h"
#include <algorithm>
#include <map>
#include <memory>
#include "gfs/GfsModels.h"
#include "gfs/GfsRender.h"
#include "models/ObjectModelGet.h"
#include "models/RefsPointGraph.h"
#include "models/UtilityModels.h"
#include "objects/FutureBytes.h"
#include "objects/FutureVoid.h"
#include "gfs/GfsChart.h"
#include "objects/WString.h"
#include "util/Utility.h"
#include "util/UtilityList.h"
#include "util/UtilityString.h"

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
    , comboTilesShow{this, {"Compare: charts", "Compare: models", "Compare: runs"}}
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
    boxH.addWidget(comboboxSector);
    boxH.addWidget(buttonSector);
    buttonSector.connect([this] { showSectorPicker(); });
    boxH.addWidget(comboboxProduct);
    boxH.addWidget(buttonProducts);
    buttonProducts.connect([this] { showPicker(); });
    boxH.addWidget(comboboxTime);
    boxH.addLayout(backForward);
    {   // the models that are fetched as pictures, in a second menu beside the GRIB ones: each opens its own screen
        auto * more = new QPushButton{"Image models  \xE2\x96\xBE", this};
        auto * menu = new QMenu{more};
        const auto add = [this, menu] (const QString& text, std::function<void()> open) {
            QObject::connect(menu->addAction(text), &QAction::triggered, this, [open] { open(); });
        };
        Window * opener = parent;
        const auto screen = [this, opener] (const string& type) { return [this, opener, type] { new ModelViewer{opener ? opener : this, type}; }; };
        add("NSSL WRF (WRF, FV3, HRRRv3)", screen("NSSLWRF"));
        add("NSSL CAMs (MPAS, WRF, HRRR, RRFS)", [this, opener] { new CamsViewer{opener ? opener : this}; });
        menu->addSeparator();
        add("SPC HRRR", screen("SPCHRRR"));
        add("SPC HREF", screen("SPCHREF"));
        menu->addSeparator();
        add("ESRL HRRR / RAP", screen("ESRL"));
        add("WPC GEFS", screen("WPCGEFS"));
        more->setMenu(menu);
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
        comboTiles.getView()->setToolTip("Several charts of the same hour side by side. They zoom, pan and read out together.");
        comboTilesShow.getView()->setToolTip("What the other tiles show: other charts of this model, the same chart from other models, or the same valid time from older runs.");
        comboTiles.setIndex(static_cast<size_t>(std::clamp(Utility::readPrefInt("MODEL_TILES", 0), 0, 3)));
        comboTilesShow.setIndex(static_cast<size_t>(std::clamp(Utility::readPrefInt("MODEL_TILES_SHOW", 0), 0, 2)));
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
        buttonBuild = new QPushButton{"Build a chart \xE2\x96\xBE", this};
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
        buttonMax = new QPushButton{"Maximum \xE2\x96\xBE", this};
        auto * maxMenu = new QMenu{buttonMax};
        QObject::connect(maxMenu->addAction("Of the 24 hours ending at this hour"), &QAction::triggered, this, [this] { showMax(false); });
        QObject::connect(maxMenu->addAction("Day 1 (12z to 12z)"), &QAction::triggered, this, [this] { showMax(true); });
        buttonMax->setMenu(maxMenu);
        buttonMax->setToolTip("The largest value of the chart's fill over a day (hail, gusts, rain rate ...). Choosing another hour goes back to the plain chart.");
        boxH2.addWidgetReal(buttonMax);
        buttonViews = new QPushButton{"Saved views \xE2\x96\xBE", this};
        menuViews = new QMenu{buttonViews};
        buttonViews->setMenu(menuViews);
        boxH2.addWidgetReal(buttonViews);
        loadViews();
        boxH2.addWidget(soundingPick.button());
        buttonMembers = new QPushButton{"Members graph", this};
        buttonMembers->setToolTip("REFS charts: each of the 5 members' value at the clicked point over the forecast hours, with the ensemble mean (and the threshold of a probability chart)");
        QObject::connect(buttonMembers, &QPushButton::clicked, this, [this] { openMemberGraph(); });
        boxH2.addWidgetReal(buttonMembers);
        auto * save = new QPushButton{"Save...", this};
        save->setToolTip("Save the picture, or the hours drawn so far as a loop");
        QObject::connect(save, &QPushButton::clicked, this, [this] { saveLoop(); });
        boxH2.addWidgetReal(save);
        boxH2.addStretch();
        box.addLayout(boxH2);
    }
    image.setCrosshairMode(true);   // click a point, then Sounding
    QObject::connect(&image, &ZoomImage::doubleClicked, this, [this] {
        if (!shownBytes.isEmpty()) {
            new ImageViewer{this, shownBytes, objectModel.model + " " + objectModel.param};
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
            QObject::connect(pictures[from], &ZoomImage::hovered, this, [this, from, hoverOf] (double fx, double fy) {
                for (size_t to = 0; to <= static_cast<size_t>(tileCount()); to++) {
                    if (to != from && hoverOf(to)) {
                        hoverOf(to)->show(fx, fy, 12, 12);
                    }
                }
            });
            QObject::connect(pictures[from], &ZoomImage::hoverEnded, this, [this, from, hoverOf] {
                for (size_t to = 0; to <= static_cast<size_t>(tileCount()); to++) {
                    if (to != from && hoverOf(to)) {
                        hoverOf(to)->hide();
                    }
                }
            });
        }
        applyTileLayout();
    }
    box.addWidgetReal(compareArea, 1, Qt::Alignment{});
    strip = new TimeStrip{this};
    strip->onSelect = [this] (int index) { selectHour(index); };
    strip->onPlay = [this] (bool on) { startPlaying(on); };
    box.addWidgetReal(strip);
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

    getRun();
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
    objectModel.model = objectModel.models[index];
    objectModel.setModelVars(objectModel.model);
    getRun();
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
    reload();
}

void ModelViewer::changeTime(size_t index) {
    if (variant.kind == GfsRender::Variant::Kind::Max) {
        variant = {};
        variantKey.clear();
    }
    objectModel.setTimeIdx(index);
    reload();
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
    string key = objectModel.model + "|" + objectModel.param + "|" + objectModel.sector + "|" + objectModel.run + "|" + std::to_string(hour);
    for (const auto& id : overlays) {
        key += "|" + id;
    }
    return key + variantKey + (member.empty() ? string{} : "|member=" + member);
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
        setTitle(objectModel.model + ": not enough hours in that range for a maximum");
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
        const auto picture = compositePicture();
        const auto stem = QString::fromStdString(objectModel.model + "_compare_" + objectModel.sector + "_" + objectModel.run);
        UtilityAnimationExport::saveWithDialog(this, std::vector<QByteArray>{}, strip->intervalMs(), picture, stem, QByteArray{}, false, stem);
        return;
    }
    std::vector<std::pair<int, QByteArray>> drawn;
    for (const auto& t : objectModel.times) {
        const int hour = std::atoi(t.c_str());
        const auto found = frames.find(frameKey(hour));
        if (found != frames.end()) {
            drawn.emplace_back(hour, found->second.bytes);
        }
    }
    std::vector<QByteArray> loop;
    for (const auto& [hour, bytes] : drawn) {
        loop.push_back(bytes);
    }
    const auto name = QString::fromStdString(objectModel.model + "_" + objectModel.param + "_" + objectModel.sector + "_" + objectModel.run);
    UtilityAnimationExport::saveWithDialog(this, loop.size() >= 2 ? loop : std::vector<QByteArray>{}, strip->intervalMs(), shownBytes, name, QByteArray{}, false, name);
}

void ModelViewer::showFrame(const Frame& frame) {
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
    mainCaption->setText(QString::fromStdString(caption(makeJob(0, std::atoi(objectModel.getTime().c_str()), false))));
    renderTiles();
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
    strip->setLoaded(ready, windowReady < wanted && ahead > 0 ? QString{"Drawing %1 of %2"}.arg(windowReady).arg(wanted) : QString{"%1 hours ready"}.arg(have));
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

void ModelViewer::refreshTimeStrip() {
    frames.clear();   // a refreshed run: draw the frames again
    frameOrder.clear();
    frameBytes = 0;
    failedAhead.clear();
    std::vector<string> labels;
    for (const auto& t : objectModel.times) {
        labels.push_back(WString::split(t, " ")[0]);
    }
    strip->setTimes(labels, static_cast<int>(comboboxTime.getIndex()));
    strip->setVisible(GfsRender::drawsModel(objectModel.model));
    refreshLoaded();
}

void ModelViewer::reload() {
    if (strip) {
        strip->setCurrent(static_cast<int>(comboboxTime.getIndex()));
    }
    objectModel.writePrefs();
    setTitle(objectModel.model + " " + objectModel.sector + " " + objectModel.times[comboboxTime.getIndex()]);
    if (GfsRender::handles(objectModel.model, objectModel.param)) {
        // the GFS charts are drawn here from the GRIB data
        const int mine = ++drawing;
        NetManager::cancelQueued(NetManager::Priority::Ahead);   // the hours being read ahead for the view that was: dropped, so what is wanted now is not behind them
        failedAhead.clear();
        if (!gfsSession) {
            gfsSession = std::make_shared<GfsRender::Session>();   // its folder goes when this screen does
        }
        auto session = gfsSession;
        const auto model = objectModel.model, param = objectModel.param, sector = objectModel.sector, run = objectModel.run;
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
                               setTitle("GFS: " + result->second);
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

void ModelViewer::getRun() {
    new FutureVoid{this, [this] { getRunStatus(); }, [this] { updateRunStatus(); }};
}

void ModelViewer::getRunStatus() {
    ObjectModelGet::runStatus(objectModel);
    objectModel.run = objectModel.runTimeData.mostRecentRun;
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
        labels.push_back(t == utc ? run + "  newest" : days == 0 ? run : days == 1 ? run + "  yesterday" : run + "  " + t.toString("MMM d").toStdString());
    }
    return labels;
}

void ModelViewer::updateRunStatus() {
    comboboxTime.block();
    comboboxRun.block();
    comboboxSector.block();
    comboboxProduct.block();
    comboboxModel.block();
    comboboxTime.setList(objectModel.times);
    if (objectModel.model == "GLCFS") {
        // pass
    } else if (objectModel.model != "HRRR" && objectModel.model != "HREF" && objectModel.model != "ESRL") {
        for (auto index : range(objectModel.times.size())) {
            auto timeStr = objectModel.times[index];
            auto newValue = WString::split(timeStr, " ")[0] + " " + UtilityModels::convertTimeRuntoTimeString(WString::replace(objectModel.runTimeData.timeStringConversion, "Z", ""), WString::split(timeStr, " ")[0]);
            objectModel.setTimeArr(index, newValue);
        }
    } else if (objectModel.prefModel == "SPCHRRR" || objectModel.prefModel == "ESRL") {
        objectModel.runs = objectModel.runTimeData.listRun;
        objectModel.times = UtilityModels::updateTime(UtilityString::getLastXChars(objectModel.run, 2), objectModel.run, objectModel.times, "");
    } else {
        objectModel.runs = objectModel.runTimeData.listRun;
        objectModel.times = UtilityModels::updateTime(UtilityString::getLastXChars(objectModel.run, 3), objectModel.run, objectModel.times, "");
    }
    comboboxModel.setIndexByValue(objectModel.model);

    comboboxSector.setList(objectModel.sectors);
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

    comboboxTime.setList(objectModel.times);
    comboboxTime.setIndexByValue(objectModel.getTime());

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
    buttonSector.setText(objectModel.sector + "  \xE2\x96\xBE");
    buttonProducts.setVisible(grib);
    refreshBuildMenu();
    refreshMembers();
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
    // where an area goes in the picker's tree
    string sectorGroup(const string& id) {
        static const std::map<string, string> groups{
            {"CONUS", "United States"}, {"NORTHEAST", "United States"}, {"MID-ATLANTIC", "United States"}, {"SOUTHEAST", "United States"}, {"GREAT-LAKES", "United States"},
            {"OHIO-VALLEY", "United States"}, {"S-PLAINS", "United States"}, {"N-PLAINS", "United States"}, {"ROCKIES", "United States"}, {"SOUTHWEST", "United States"},
            {"PACIFIC-NW", "United States"}, {"CALIFORNIA", "United States"}, {"GULF-COAST", "United States"}, {"ALASKA", "United States"}, {"HAWAII", "United States"},
            {"NAMER", "North America and the Caribbean"}, {"CENT-AMER", "North America and the Caribbean"}, {"CARIBBEAN", "North America and the Caribbean"},
            {"GULF-MEXICO", "North America and the Caribbean"},
            {"SAMER", "Continents"}, {"AFRICA", "Continents"}, {"EUROPE", "Continents"}, {"ASIA", "Continents"}, {"INDIA", "Continents"}, {"E-ASIA", "Continents"},
            {"SE-ASIA", "Continents"}, {"MIDDLE-EAST", "Continents"}, {"AUSTRALIA", "Continents"},
            {"WEST-ATL", "Oceans"}, {"ATLANTIC", "Oceans"}, {"NORTH-ATL", "Oceans"}, {"EAST-PAC", "Oceans"}, {"NORTH-PAC", "Oceans"}, {"SOUTH-PAC", "Oceans"},
            {"INDIAN-OCEAN", "Oceans"}, {"US-SAMOA", "Oceans"},
            {"GLOBAL", "World and poles"}, {"TROPICS", "World and poles"}, {"NORTHERN-HEMI", "World and poles"}, {"SOUTHERN-HEMI", "World and poles"},
            {"POLAR", "World and poles"}, {"ARTIC", "World and poles"}};
        const auto found = groups.find(id);
        return found == groups.end() ? "Other" : found->second;
    }
}

namespace {
    // where a model goes in the picker's tree (after the model sites' own layout: global, regional, convection allowing, ensembles, climate)
    string modelGroup(const string& id) {
        static const std::map<string, string> groups{
            {"GFS", "Global"}, {"AIGFS", "Global"}, {"IFS", "Global"}, {"AIFS", "Global"},
            {"NAM", "Regional"}, {"RAP", "Regional"}, {"NBM", "Regional"}, {"FIREWX", "Regional"},
            {"HRRR", "Convection allowing"}, {"RRFS", "Convection allowing"}, {"NAM-HIRES", "Convection allowing"}, {"HRW-ARW", "Convection allowing"},
            {"HRW-ARW2", "Convection allowing"}, {"HRW-FV3", "Convection allowing"},
            {"GEFS", "Ensembles"}, {"REFS", "Ensembles"}, {"HREF", "Ensembles"}, {"NAEFS", "Ensembles"},
            {"GEFS-MEAN-SPRD", "Ensembles"}, {"GEFS-SPAG", "Ensembles"},
            {"GFS-WAVE", "Waves and ocean"}, {"GEFS-WAVE", "Waves and ocean"}, {"WW3", "Waves and ocean"}, {"WW3-ENP", "Waves and ocean"}, {"WW3-WNA", "Waves and ocean"},
            {"ESTOFS", "Waves and ocean"}, {"POLAR", "Waves and ocean"}};
        const auto found = groups.find(id);
        return found == groups.end() ? "Other" : found->second;
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
        entries.push_back({id, id, modelGroup(id)});
    }
    static const std::vector<string> order{"Global", "Regional", "Convection allowing", "Ensembles", "Waves and ocean", "Other"};
    std::stable_sort(entries.begin(), entries.end(), [] (const ProductPicker::Entry& a, const ProductPicker::Entry& b) {
        return std::find(order.begin(), order.end(), a.group) < std::find(order.begin(), order.end(), b.group);
    });
    modelPicker = new ProductPicker{this, "NCEP", entries, objectModel.model, {}, {}, {}};
    modelPicker->setWording("models", "Show this model");
    modelPicker->resize(320, 520);
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
        entries.push_back({id, id, sectorGroup(id)});
    }
    // the groups in a fixed order: the entries are listed as the model gives them
    std::stable_sort(entries.begin(), entries.end(), [] (const ProductPicker::Entry& a, const ProductPicker::Entry& b) {
        static const std::vector<string> order{"United States", "North America and the Caribbean", "Continents", "Oceans", "World and poles", "Other"};
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

ModelViewer::TilesShow ModelViewer::tilesShow() const {
    const auto i = comboTilesShow.getIndex();
    return i == 1 ? TilesShow::Models : i == 2 ? TilesShow::Runs : TilesShow::Charts;
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
    } else if (show == TilesShow::Models) {
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
        } else if (show == TilesShow::Models) {
            spec.model = k < others.size() ? others[k] : string{};
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

// what the tile at `index` (0 the screen's own chart) draws at the hour shown by the first tile
ModelViewer::Job ModelViewer::makeJob(int index, int mainHour, bool ahead) const {
    Job job;
    job.hour = mainHour;
    if (index == 0) {
        job.model = objectModel.model;
        job.param = objectModel.param;
        job.sector = objectModel.sector;
        job.cycle = objectModel.run;
        job.overlays = overlays;
        job.variant = ahead && variant.kind != GfsRender::Variant::Kind::Change ? GfsRender::Variant{} : variant;
        job.variant.member = member;
        job.key = frameKey(mainHour);
    } else {
        const auto& spec = tileSpecs[static_cast<size_t>(index) - 1];
        const auto show = tilesShow();
        job.model = spec.model;
        job.param = spec.param;
        job.sector = objectModel.sector;
        job.cycle = objectModel.run;
        if (spec.model.empty()) {
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
        } else if (show == TilesShow::Models) {
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
        if (spec.model == objectModel.model) {   // the same model's tiles show the member too (another model's charts have none)
            v.member = member;
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
    return job.model + "   " + (product ? product->label : job.param) + "   " + run + (job.variant.kind == GfsRender::Variant::Kind::Change ? "   (change)" : "");
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
    if (show == TilesShow::Models) {
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
            choices = GfsChart::memberChoices(*product);
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
        member = memberValues.empty() ? string{} : memberValues.front();
        comboMember.block();
        comboMember.setList(labels);
        comboMember.setIndex(0);
        comboMember.unblock();
    }
    comboMember.setVisible(!memberValues.empty());
}
