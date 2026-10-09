// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef MODELVIEWER_H
#define MODELVIEWER_H

#include <memory>
#include <string>
#include "gfs/GfsRender.h"
#include "models/ObjectModel.h"
#include <QPointer>
#include "models/ProductPicker.h"
#include "ui/BackForward.h"
#include "ui/Button.h"
#include "ui/ComboBox.h"
#include "ui/HBox.h"
#include "ui/Photo.h"
#include "ui/VBox.h"
#include "ui/Window.h"

using std::string;

class ModelViewer : public Window {
public:
    ModelViewer(Window *, const string&);
    void showPicker();
    void showSectorPicker();   // the same, for the areas   // the grouped chart picker (the models drawn from GRIB)

private:
    void changeModelCb();
    void changeRunCb();
    void changeSectorCb();
    void changeProductCb();
    void changeTimeCb();
    void changeModel(int);
    void changeParam(int);
    void changeSector(int);
    void changeRun(int);
    void changeTime(size_t);
    void moveBack();
    void moveForward();
    void reload();
    void refreshProductButton();
    int drawing{0};
    std::shared_ptr<GfsRender::Session> gfsSession;
    std::vector<std::string> overlays;   // the lines and barbs ticked onto the chart
    void getRun();
    void getRunStatus();
    void updateRunStatus();
    HBox boxH;
    VBox box;
    Photo photo;
    ObjectModel objectModel;
    ComboBox comboboxRun;
    ComboBox comboboxModel;
    ComboBox comboboxSector;
    ComboBox comboboxProduct;
    ComboBox comboboxTime;
    BackForward backForward;
    Button buttonProducts;
    Button buttonSector;
    QPointer<ProductPicker> sectorPicker;
    QPointer<ProductPicker> picker;
};

#endif  // MODELVIEWER_H
