// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef MODELVIEWER_H
#define MODELVIEWER_H

#include <string>
#include "models/ObjectModel.h"
#include "ui/BackForward.h"
#include "ui/ComboBox.h"
#include "ui/HBox.h"
#include "ui/Photo.h"
#include "ui/VBox.h"
#include "ui/Window.h"

using std::string;

class ModelViewer : public Window {
public:
    ModelViewer(Window *, const string&);

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
};

#endif  // MODELVIEWER_H
