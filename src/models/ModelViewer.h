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
#include <deque>
#include <map>
#include <set>
#include <QTimer>
#include "models/ChartHover.h"
#include "models/CompareTile.h"
#include <QDateTime>
#include <QGridLayout>
#include "models/SoundingPick.h"
#include "models/TimeStrip.h"
#include <array>
#include "ui/VBox.h"
#include "ui/Window.h"

using std::string;

class ModelViewer : public Window {
public:
    ModelViewer(Window *, const string&);
    void showPicker();
    void showModelPicker();    // the models in groups (global, regional, convection allowing, ensembles, waves ...)
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
    // the comparison tiles: 1 x 1 (off), 1 x 2, 1 x 3 or 2 x 2 charts of the same hour. What differs is chosen: the charts (of this model, area and run), the models (the same chart), or the runs (the same
    // valid time from older runs). The first tile is the screen's own chart; the others take their view (zoom, place, hover) from it. Each tile's frames are kept and read ahead like the first one's.
    struct Spec {
        string model, param, sector;
        int shift{0};   // runs: hours older than the run chosen
    };
    struct Job {
        string model, param, sector, cycle, key, chart;   // chart: the key without the hour (a new hour of it keeps the zoom)
        int hour{0};
        std::vector<string> overlays;
        GfsRender::Variant variant;
        bool ok{true};
        string why;
    };
    enum class TilesShow { Charts, Models, Runs };
    int tileCount() const;                       // the tiles besides the first: 0, 1, 2 or 3
    TilesShow tilesShow() const;
    void applyTileLayout();
    void resetTileSpecs();
    Job makeJob(int index, int mainHour, bool ahead) const;
    string caption(const Job&) const;
    void renderTiles();
    void renderTile(int index);
    void showTile(int index, const Job&, const Frame&);
    void changeTile(int index);
    void pickFromTiles(int source, double fx, double fy);
    QByteArray compositePicture() const;
    QDateTime runTime(const string& run) const;
    ComboBox comboTiles, comboTilesShow;
    QWidget * compareArea{};
    QGridLayout * grid{};
    QFrame * mainTile{};
    QLabel * mainCaption{};
    std::array<CompareTile *, 3> tiles{};
    std::array<Spec, 3> tileSpecs;
    std::array<int, 3> tileGeneration{};
    bool syncingViews{false};
    void showFrame(const Frame&);
    void selectHour(int index);
    void startPlaying(bool on);
    void prefetch(int generation);
    void refreshTimeStrip();
    // what the RRFS screen had, now for every model: change since an earlier run, maxima over hours, saved views, a sounding at a clicked point, save as a loop, the full picture
    void setVariant(GfsRender::Variant);
    void showMax(bool day1);
    void saveLoop();
    void loadViews();
    void saveView();
    void applyView(const std::array<string, 5>&);
    void rebuildViewsMenu();
    HBox boxH2;
    ComboBox comboCompare, comboPreload;
    QPushButton * buttonMax{};
    QPushButton * buttonBuild{};
    QMenu * menuBuild{};
    void refreshBuildMenu();
    void buildChart();
    void showBuilt(const string& id);
    QPushButton * buttonViews{};
    QMenu * menuViews{};
    SoundingPick soundingPick;
    GfsRender::Variant variant;
    string variantKey;                   // the variant as part of a frame's key
    QByteArray shownBytes;
    std::shared_ptr<GfsChart::Probe> shownProbe;
    std::vector<std::array<string, 5>> views;   // name, model, chart, area, extras (comma separated)
    std::array<string, 5> pendingView;          // a saved view of another model: applied when its runs are in
    TimeStrip * strip{};
    QTimer playTimer;
    void storeFrame(const string& key, const Frame&);
    std::map<string, Frame> frames;
    std::deque<string> frameOrder;       // the order frames were drawn: the oldest are dropped past the memory they may hold
    size_t frameBytes{0};                // what the frames hold (the picture and the hover grids)
    std::set<string> failedAhead;        // hours that could not be read ahead for this view
    int prefetchGeneration{0};
    std::set<string> inFlight;           // the hours being drawn ahead (a few at a time)
    int preloadAhead() const;            // how many hours ahead of the one shown are drawn on their own: 0 only the neighbours, a large number for the whole run
    void refreshLoaded();                // the timeline's bar and count of the hours that are ready
    bool playing{false};
    string lastChart;                    // the model, chart, area and extras of the picture shown: a new hour of the same keeps the zoom
    int drawing{0};
    std::shared_ptr<GfsRender::Session> gfsSession;
    std::vector<std::string> overlays;   // the lines and barbs ticked onto the chart
    void getRun();
    void getRunStatus();
    void updateRunStatus();
    vector<string> runLabels() const;
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
    Button buttonModel;
    QPointer<ProductPicker> modelPicker;
    QPointer<ProductPicker> sectorPicker;
    QPointer<ProductPicker> picker;
};

#endif  // MODELVIEWER_H
