// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "ModelViewer.h"
#include "models/ObjectModelGet.h"
#include "models/UtilityModels.h"
#include "objects/FutureBytes.h"
#include "objects/FutureVoid.h"
#include "objects/WString.h"
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
    boxH.addWidget(comboboxProduct);
    boxH.addWidget(comboboxTime);
    boxH.addLayout(backForward);
    box.addLayout(boxH);
    box.addWidgetAndCenter(photo);
    box.getAndShow(this);

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

    reload();
}
