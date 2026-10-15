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
#include "ui/FlowBox.h"
#include "ui/HBox.h"
#include "ui/Photo.h"
#include "ui/ZoomImage.h"
#include <deque>
#include <map>
#include <set>
#include <QComboBox>
#include <QTimer>
#include "models/ChartHover.h"
#include "models/CompareTile.h"
#include <QDateTime>
#include <QGridLayout>
#include "models/SoundingPick.h"
#include "models/TimeStrip.h"
#include <array>
#include <functional>
#include "ui/VBox.h"
#include "ui/Window.h"

using std::string;

namespace UtilityBufkit {
    struct Run;
}
class HodographGlyph;

class ModelViewer : public Window {
public:
    ModelViewer(Window *, const string&);
    // set when this viewer is a page of the model screen: another source is shown there (its id, ModelScreen::sources) instead of in a window of its own
    std::function<void(const string&)> openSource;
    void selectModel(const string& id);   // show this model ("SPCREFS" ...), as the model box would
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
    bool hourReady(int hour) const;   // the chart of every tile for that hour is drawn (a tile that cannot draw it does not hold the others back)
    // the comparison tiles: 1 x 1 (off), 1 x 2, 1 x 3 or 2 x 2 charts of the same hour. What differs is chosen: the charts (of this model, area and run), the models (the same chart), or the runs (the same
    // valid time from older runs). The first tile is the screen's own chart; the others take their view (zoom, place, hover) from it. Each tile's frames are kept and read ahead like the first one's.
    struct Spec {
        string model, param, sector;
        int shift{0};   // runs: hours older than the run chosen
        string member;  // members: the member this tile shows (when picked with its button)
        bool memberPicked{false};
    };
    struct Job {
        string model, param, sector, cycle, key, chart;   // chart: the key without the hour (a new hour of it keeps the zoom)
        int hour{0};
        std::vector<string> overlays;
        GfsRender::Variant variant;
        bool ok{true};
        string why;
    };
    enum class TilesShow { Charts, Models, Runs, Members, ModelsByTime };
    int tileCount() const;                       // the tiles besides the first: 0, 1, 2 or 3
    TilesShow tilesShow() const;
    void applyTileLayout();
    void resetTileSpecs();
    Job makeJob(int index, int mainHour, bool ahead) const;
    string tileMember(int index) const;          // the member a tile of the members comparison shows: the one it was given, else the ones after the first tile's, in the list's order
    string memberLabel(const string& value) const;
    string caption(const Job&) const;
    void renderTiles();
    void renderTile(int index);
    void showTile(int index, const Job&, const Frame&);
    void changeTile(int index);
    void pickFromTiles(int source, double fx, double fy);
    QByteArray compositePicture() const;
    QDateTime runTime(const string& run) const;
    ComboBox comboTiles, comboTilesShow;
    std::vector<string> memberLabels;     // what comboMember lists, by memberValues
    ComboBox comboMember;                 // one member of the ensemble instead of the mean (GEFS: 31, REFS: 5), for the charts that can show one
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
    FlowBox boxH2;   // wraps onto more rows when the window is narrowed, so the buttons never set a minimum width
    ComboBox comboCompare, comboPreload;
    std::vector<std::string> memberValues;   // the members' names in the model's files, as the combo lists them
    std::string member;                   // the one chosen: "" the chart as it is
    void refreshMembers();
    void addLocalSectors();
    void applyMainRunRule();   // a chart only a model's main runs have: shows the main run in its place, and says so above the timeline
    std::string runSwappedFrom;   // the run that was chosen before the chart made the screen move to a main run ("" when none)
    // a hodograph of each station's model sounding on the map, for the hour on view (laid over the view like the station buttons: not in a saved picture)
    QPushButton * buttonHodos{nullptr};
    QTimer hodoTimer;
    std::map<string, std::shared_ptr<UtilityBufkit::Run>> bufkitRuns;   // by station: the newest RRFS run of its BUFKIT file (every forecast hour)
    std::set<string> bufkitAsked;
    std::deque<string> bufkitQueue;
    int bufkitRunning{0};
    std::map<string, HodographGlyph *> hodoGlyphs;
    void refreshHodographs();
    void pumpBufkit();
    QPushButton * buttonStations{nullptr};   // a button on the map at each BUFKIT station: opens that station's model sounding (laid over the view, never in a saved picture)
    string stationPinKey;                    // the map the pins were placed on
    void refreshStationPins();
    QPushButton * buttonPlace{nullptr};
    QComboBox * comboLocalKm{nullptr};   // the size of an area round a saved location (one entry per place in the area list, this sets how big)
    void refreshSectorList();   // the ring and the name of the place on an area round a saved location (a pref, MODEL_PLACE_MARK)
    QPushButton * buttonSigma{nullptr};   // an anomaly chart in standard deviations instead of its own unit (dam, mb), for the charts that have both
    bool standardDeviations{false};
    QPushButton * buttonMax{};
    QPushButton * buttonBuild{};
    QMenu * menuBuild{};
    void refreshBuildMenu();
    void buildChart();
    void showBuilt(const string& id);
    QPushButton * buttonMembers{};   // REFS charts: each member's value at the picked point over the hours (the old REFS screen's plume graph)
    void openMemberGraph();
    void chooseOverlays();                 // the other models whose lines are drawn over this chart's (a dialog of the models that have the chart)
    void refreshOverlays();                // keeps the ones that still have the chart on screen, and the button's text
    std::vector<std::string> overlayModels;
    QPushButton * buttonOverlay{};
    void editColors();   // the color scale of the chart on screen (PaletteEditor)
    QPushButton * buttonColors{};
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
    std::vector<string> allTimes;         // every hour the model offers; objectModel.times is the part the run on show reaches (the RRFS's hourly runs stop at 18 h)
    void fitHoursToRun();
    std::map<string, std::vector<int>> fieldHours;   // "run|chart" -> the hours an SPC REFS field has (its own time coordinate)
    std::set<string> fieldHoursAsked;
    void askFieldHours();
    void recheckRuns();                   // every 15 minutes: has a newer run been published?
    QTimer recheckTimer;
    bool rechecking{false};
    void getRunStatus();
    void updateRunStatus();
    vector<string> runLabels() const;
    FlowBox boxH;
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
