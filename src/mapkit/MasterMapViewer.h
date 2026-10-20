// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef MASTERMAPVIEWER_H
#define MASTERMAPVIEWER_H

#include <ctime>
#include <memory>
#include <map>
#include <set>
#include <string>
#include <vector>
#include <QCheckBox>
#include <QDialog>
#include <QToolButton>
#include <QComboBox>
#include <QPushButton>
#include <QSlider>
#include <QDateTimeEdit>
#include <QLabel>
#include <QScrollArea>
#include <QTimer>
#include <QTreeWidget>
#include <QVBoxLayout>
#include "mapkit/GlobeMapView.h"
#include "mapkit/MapLayer.h"
#include "ui/Button.h"
#include "ui/ComboBox.h"
#include "ui/HBox.h"
#include "ui/ControlRow.h"
#include "ui/Text.h"
#include "ui/VBox.h"
#include "ui/Window.h"

using std::string;
using std::vector;

// The master map: one map with every kind of thing that is drawn on a map as a layer in a tree - surface stations, river gauges, dams, buoys, tropical
// storms and the rest - switched on and off, each with its own options, over the same coastlines. A "view" in the box is a saved set of layers and a
// region (the old Rivers, Tropical and Surface observation screens as presets). What is on is remembered.
class ModelChartLayer;
class MasterMapViewer : public Window, private MapHost {
public:
    void setGlobe(bool on);   // the layers on a globe instead of the flat map
    void applyRange(const QDateTime& from, const QDateTime& to);   // loop from one time to another
    bool saveModelChart(const QString& path, std::string& problem);   // the model chart layer's Model Viewer picture of what is in view (false: none is on, or why it could not be made)
    // startView: the start of a saved view's name to open with (empty: what was on last time)
    explicit MasterMapViewer(Window * parent, const string& startView = {});
    // the map at a time of the past and a place (the history page): only the layers given (those that can show a past time), each set to that time; the region is the point
    // with its radius
    // The map as the model browser (what the Model Viewer page was): the model chart layer alone, on a model / chart / area (sector id; "" the ones used last, or the United States) and, when `valid` is
    // set, at that valid time
    static MasterMapViewer * openModels(Window * parent, const string& model = {}, const string& param = {}, const string& sector = {}, const QDateTime& valid = {});
    void openAt(const QDateTime& utc, double lat, double lon, double radiusKm, const vector<string>& layerIds);

private:
    // MapHost
    MapView& view() override { return *mapView; }
    Window * hostWindow() override { return this; }
    void redraw() override { mapView->map()->update(); listTimer.start(500); if (globeView != nullptr && globeOn) { globeView->scheduleSkin(); } }   // (a layer that has read something has a new summary for the list)
    void status(const string& text) override { extra = text; updateStatus(); }
    bool claimCell(const QPointF& pixels, double spacing) override;
    double markScale() const override { return thinning; }
    void timesChanged() override { rebuildTicks(); }
    void onlyLayer(const string& id) override;
    void background(std::function<void()> work, std::function<void()> done) override;

    void buildTree();
    void setLayerOn(const string& id, bool on);
    void applyPreset(size_t index);
    void saveState();
    void updateStatus();
    void paintMap(QPainter&);
    void paintMapCopy(QPainter&);
    void paintLegend(QPainter&);
    void showHover(const QPointF& pixels);
    MapHit bestHit(const QPointF& pixels) const;
    void showOptions(MapLayer *);
    void refreshActiveList();
    void showLayerOptions(MapLayer *);
    void showAddLayers();
    void tick();
    void exportView();
    bool modelSpec(string& chart, vector<string>& models, QDateTime& when) const;   // the model chart or combo on the map
    void exportWorld();
    void showAreaPicker();   // MAG's area picker on the globe: point at an area, see it; click, go there                          // the whole world, equal area, as one picture (asks)

public:
    void previewArea(const string& id, bool goThere);   // the area as a rectangle on the globe, which turns to it (goThere: and zooms to fit, and the flat map follows)
    void exportWorldTo(const QString& path, GlobeMapView::Projection projection, double centerLongitude, int width, bool highResolution = false, bool graticule = false);

private:
    void exportTo(const QString& path);          // PNG, or PDF by the ending of the name
    void paintExport(QPainter&, int mapSide, double scale, bool vectorOutput);
    // the time bar: stepping and looping through the frames of the layers that have them
    bool forecastsOnly() const;
    void openHours();
    void updateTimeline();   // the hour labels over the slider and the bar of the frames that are ready
    void rebuildTicks();
    void goLive();
    void jumpTo(const QDateTime& utc);   // a time of the person's choosing: the layers look at the hours around it
    void goTo(int index);
    void togglePlay();
    void loopBounds(int& first, int& last) const;
    void askRange();                     // "Loop from Time 1 to Time 2": a dialog for the two times
    void clearRange();
    void advance();
    ~MasterMapViewer() override;
    void closeEventCustom() override { closed = true; }
    void resizeEventCustom() override;
    MapLayer * layerOf(const string& id) const;

    VBox box;
    ControlRow rowTop;
    HBox rowMain;
    ComboBox comboView;
    Button buttonRefresh;
    Button buttonSave;
    Button buttonPlume;
    Button buttonOthers;
    Button buttonViews;
    Button buttonCompare;
    Button buttonArea;
    Button buttonGlobe;
    Button buttonAreas;
    GlobeMapView * globeView{};
    bool globeOn{false};
    GlobeMapView::Skin makeSkin(double minLat, double maxLat, double minLon, double maxLon, int width, double referencePixelsPerDegree = 0.0, bool waitForLayers = false);
    double thinning{1.0};                                     // how much wider apart the self-thinning marks are put while a skin is painted
    Button buttonTemps;
    Button buttonSounding;
    Button buttonMembers;
    void saveModelLoop();                            // the hours of the model chart as a GIF / video of titled pictures
    // views kept by name: the layers on, the region and the model chart's settings
    struct SavedView {
        string name;
        vector<string> layers;
        double minLat{0}, maxLat{0}, minLon{0}, maxLon{0};
        string model;
    };
    vector<SavedView> savedViews() const;
    void storeViews(const vector<SavedView>&);
    void refreshViewList();
    void viewsMenu();
    void applySaved(const SavedView&);
    void openCompare();                              // a second map beside this one: same place and time, to set to another chart, model or run
    void chooseArea();                               // the map to a named area (a region of the world, or round a saved location)
    bool pointFor(double& lat, double& lon) const;   // the pointer's last place on the map, or the middle
    ModelChartLayer * modelLayer() const;            // the model chart layer when it is on
    double pointerLat{0.0}, pointerLon{0.0};   // where the pointer last was on the map: the plume button opens at it
    bool havePointer{false};
    Text textStatus;
    std::unique_ptr<MapView> mapView;
    QLabel * hoverLabel{};
    QWidget * sidePanel{};
    QTreeWidget * tree{};
    QWidget * activeBox{};
    QScrollArea * activeScroll{};
    QVBoxLayout * activeLayout{};
    QDialog * addDialog{};
    QWidget * optionsBox{};
    QCheckBox * legendCheck{};
    QLabel * noticeLabel{};
    QWidget * timeRow{};
    QSlider * timeSlider{};
    QPushButton * playButton{};
    QPushButton * rangeButton{};
    QPushButton * hoursButton{};
    QWidget * timeTicks{};
    long loopFrom{0}, loopTo{0};    // the loop's own start and end (seconds), 0: all the frames the bar has
    QPushButton * liveButton{};
    QPushButton * stepBack{};
    QPushButton * stepForward{};
    QComboBox * speedCombo{};
    QLabel * timeLabel{};
    QDateTimeEdit * timePick{};
    QPushButton * timeGo{};
    QTimer playTimer;
    bool following{false};          // opened at a time by the History page: follows the history cursor, and moves it when its own time bar is moved
    bool applyingCursor{false};
    void followCursor(QObject * origin);
    void announceTime();
    vector<long> ticks;             // the times the bar steps through: every frame of every layer that has them, ascending
    long wantedTime{0};             // the time last chosen on the bar: kept when a layer's frames are made again (another chart)
    std::set<string> expanded;      // the layers whose options are open in the manager
    std::map<string, QWidget *> inlineOptions;   // their option panels (kept, not made again)
    std::map<string, QLabel *> summaryLabels;
    string activeSignature;
    bool live{true};                // the newest of everything (the frames follow new scans)
    bool playing{false};
    bool playWhenReady{false};
    bool settingSlider{false};
    QVBoxLayout * optionsLayout{};
    QLabel * optionsTitle{};
    vector<std::unique_ptr<MapLayer>> layers;       // in painting order
    std::map<string, QTreeWidgetItem *> items;      // layer id -> its tree item
    std::set<long long> cells;                      // the patches claimed in this painting
    std::map<string, std::time_t> refreshed;
    QTimer timer;
    QTimer listTimer;   // the list of layers on is made again a moment after the map is painted again
    string extra;
    bool building{false};
    bool closed{false};
};

#endif  // MASTERMAPVIEWER_H
