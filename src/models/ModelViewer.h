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
#include "ui/ZoomImage.h"
#include <map>
#include <QTimer>
#include "models/ChartHover.h"
#include "models/TimeStrip.h"
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
    // the timeline: frames drawn ahead (play loops through them) and the strip that runs it
    struct Frame {
        QByteArray bytes;
        std::shared_ptr<GfsChart::Probe> probe;
    };
    string frameKey(int hour) const;
    void showFrame(const Frame&);
    void selectHour(int index);
    void startPlaying(bool on);
    void prefetch(int generation);
    void refreshTimeStrip();
    TimeStrip * strip{};
    QTimer playTimer;
    std::map<string, Frame> frames;
    int prefetchGeneration{0};
    bool prefetching{false};
    bool playing{false};
    string lastChart;                    // the model, chart, area and extras of the picture shown: a new hour of the same keeps the zoom
    int drawing{0};
    std::shared_ptr<GfsRender::Session> gfsSession;
    std::vector<std::string> overlays;   // the lines and barbs ticked onto the chart
    void getRun();
    void getRunStatus();
    void updateRunStatus();
    HBox boxH;
    VBox box;
    ZoomImage image;                     // the chart: wheel / Ctrl +- zoom, drag to pan
    ObjectModel objectModel;
    ComboBox comboboxRun;
    ComboBox comboboxModel;
    ComboBox comboboxSector;
    ComboBox comboboxProduct;
    ComboBox comboboxTime;
    BackForward backForward;
    std::unique_ptr<ChartHover> hover;   // the value under the pointer, for the charts drawn from GRIB
    Button buttonProducts;
    Button buttonSector;
    QPointer<ProductPicker> sectorPicker;
    QPointer<ProductPicker> picker;
};

#endif  // MODELVIEWER_H
