// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "ModelViewer.h"
#include "ui/ActivityLabel.h"
#include <QInputDialog>
#include <QMenu>
#include "misc/ImageViewer.h"
#include "objects/UtilityAnimationExport.h"
#include "objects/NetManager.h"
#include <QPushButton>
#include "models/CamsViewer.h"
#include <algorithm>
#include <map>
#include <memory>
#include "gfs/GfsRender.h"
#include "models/ObjectModelGet.h"
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
    , comboCompare{this, {"Plain chart", "Change since run 6 h earlier", "Change since run 12 h earlier", "Change since run 24 h earlier"}}
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
        add("SPC SREF", screen("SPCSREF"));
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
            soundingPick.pick(lon, lat);
        }
    });
    box.addWidgetReal(&image, 1, Qt::Alignment{});
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
        const int next = (strip->current() + 1) % n;
        if (frames.count(frameKey(std::atoi(objectModel.times[static_cast<size_t>(next)].c_str())))) {   // wait for the frame if it is not drawn yet
            strip->setCurrent(next);
            selectHour(next);
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

string ModelViewer::frameKey(int hour) const {
    string key = objectModel.model + "|" + objectModel.param + "|" + objectModel.sector + "|" + objectModel.run + "|" + std::to_string(hour);
    for (const auto& id : overlays) {
        key += "|" + id;
    }
    return key + variantKey;
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
    if (!frames.count(key)) {
        frameOrder.push_back(key);
    }
    frames[key] = frame;
    while (frameOrder.size() > 60) {   // the oldest go first
        frames.erase(frameOrder.front());
        frameOrder.pop_front();
    }
}

// Draws hours ahead of the one shown, one at a time, with the network's "ahead" priority (what is on screen is always served first). While playing, every hour of the loop, as far as it
// gets; otherwise the hour after, the one before and the one after that, so stepping with the arrows or the slider finds them drawn. It stops when they are drawn or the chart changes.
void ModelViewer::prefetch(int generation) {
    if (generation != prefetchGeneration || prefetching || !strip->isVisible() || variant.kind == GfsRender::Variant::Kind::Max || !GfsRender::handles(objectModel.model, objectModel.param)) {
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
    }
    int want = -1;
    string wantKey;
    for (const int index : candidates) {
        const auto key = frameKey(std::atoi(objectModel.times[static_cast<size_t>(index)].c_str()));
        if (!frames.count(key) && !failedAhead.count(key)) {
            want = index;
            wantKey = key;
            break;
        }
    }
    if (want < 0) {
        return;
    }
    prefetching = true;
    if (!gfsSession) {
        gfsSession = std::make_shared<GfsRender::Session>();
    }
    auto session = gfsSession;
    const auto model = objectModel.model, param = objectModel.param, sector = objectModel.sector, run = objectModel.run;
    const auto overlayIds = overlays;
    const auto shownVariant = variant.kind == GfsRender::Variant::Kind::Change ? variant : GfsRender::Variant{};
    const int hour = std::atoi(objectModel.times[static_cast<size_t>(want)].c_str());
    auto result = std::make_shared<std::pair<QByteArray, string>>();
    auto probe = std::make_shared<GfsChart::Probe>();
    new FutureVoid{this, [=] {
                       const NetManager::Scope ahead{NetManager::Priority::Ahead};   // behind whatever the user is waiting for
                       result->first = GfsRender::png(*session, model, param, sector, run, hour, overlayIds, result->second, probe.get(), shownVariant);
                   },
                   [this, result, probe, wantKey, generation] {
                       prefetching = false;
                       if (result->first.isEmpty()) {   // no chart at this hour (or the request was dropped as the view changed): not asked for again until the view changes
                           failedAhead.insert(wantKey);
                           if (playing && generation == prefetchGeneration) {
                               startPlaying(false);
                               strip->stop();
                           }
                           prefetch(prefetchGeneration);
                           return;
                       }
                       storeFrame(wantKey, {result->first, probe});
                       prefetch(prefetchGeneration);
                   }};
}

void ModelViewer::refreshTimeStrip() {
    frames.clear();   // a refreshed run: draw the frames again
    frameOrder.clear();
    failedAhead.clear();
    std::vector<string> labels;
    for (const auto& t : objectModel.times) {
        labels.push_back(WString::split(t, " ")[0]);
    }
    strip->setTimes(labels, static_cast<int>(comboboxTime.getIndex()));
    strip->setVisible(GfsRender::drawsModel(objectModel.model));
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

void ModelViewer::updateRunStatus() {
    comboboxTime.block();
    comboboxRun.block();
    comboboxSector.block();
    comboboxProduct.block();
    comboboxModel.block();
    comboboxTime.setList(objectModel.times);
    if (objectModel.model == "GLCFS") {
        // pass
    } else if (objectModel.model != "SREF" && objectModel.model != "HRRR" && objectModel.model != "HREF" && objectModel.model != "ESRL") {
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
    if (!pendingView[0].empty() && pendingView[1] == objectModel.model) {   // a saved view of this model, waiting for its runs
        const auto view = pendingView;
        applyView(view);
        return;
    }
    reload();
}

// The charts of a model drawn from GRIB are many: they are chosen in the grouped picker, and the plain list is for the models still fetched as pictures.
void ModelViewer::refreshProductButton() {
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
            {"GEFS", "Ensembles"}, {"REFS", "Ensembles"}, {"HREF", "Ensembles"}, {"SREF", "Ensembles"}, {"NAEFS", "Ensembles"},
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
