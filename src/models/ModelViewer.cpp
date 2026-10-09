// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "ModelViewer.h"
#include "ui/ActivityLabel.h"
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
    , photo{this, FullWithHeight, [this] { return getPhotoHeight(); }}
    , objectModel{modelType}
    , comboboxRun{this}
    , comboboxModel{this, objectModel.models}
    , comboboxSector{this, objectModel.sectors}
    , comboboxProduct{this, objectModel.paramLabels}
    , comboboxTime{this, objectModel.times}
    , backForward{this, [this] { moveBack(); }, [this] { moveForward(); }}
    , buttonProducts{this, None, "Charts..."}
    , buttonSector{this, None, "Area..."}
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
    boxH.addWidget(comboboxRun);
    boxH.addWidget(comboboxSector);
    boxH.addWidget(buttonSector);
    buttonSector.connect([this] { showSectorPicker(); });
    boxH.addWidget(comboboxProduct);
    boxH.addWidget(buttonProducts);
    buttonProducts.connect([this] { showPicker(); });
    boxH.addWidget(comboboxTime);
    boxH.addLayout(backForward);
    box.addLayout(boxH);
    box.addWidgetAndCenter(photo);
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

void ModelViewer::reload() {
    objectModel.writePrefs();
    setTitle(objectModel.model + " " + objectModel.sector + " " + objectModel.times[comboboxTime.getIndex()]);
    if (GfsRender::handles(objectModel.model, objectModel.param)) {
        // the GFS charts are drawn here from the GRIB data
        const int mine = ++drawing;
        if (!gfsSession) {
            gfsSession = std::make_shared<GfsRender::Session>();   // its folder goes when this screen does
        }
        auto session = gfsSession;
        const auto model = objectModel.model, param = objectModel.param, sector = objectModel.sector, run = objectModel.run;
        const auto overlayIds = overlays;
        const int hour = std::atoi(objectModel.getTime().c_str());
        auto result = std::make_shared<std::pair<QByteArray, string>>();
        new FutureVoid{this, [=] { result->first = GfsRender::png(*session, model, param, sector, run, hour, overlayIds, result->second); },
                       [this, result, mine] {
                           if (mine == drawing && !result->first.isEmpty()) {
                               photo.setBytes(result->first);
                           } else if (mine == drawing) {
                               setTitle("GFS: " + result->second);
                           }
                       }};
        return;
    }
    new FutureBytes{this, ObjectModelGet::imageUrl(objectModel), [this] (const auto& ba) { photo.setBytes(ba); }};
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
    reload();
}

// The charts of a model drawn from GRIB are many: they are chosen in the grouped picker, and the plain list is for the models still fetched as pictures.
void ModelViewer::refreshProductButton() {
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
