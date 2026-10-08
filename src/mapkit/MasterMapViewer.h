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
#include <QComboBox>
#include <QPushButton>
#include <QSlider>
#include <QLabel>
#include <QTimer>
#include <QTreeWidget>
#include <QVBoxLayout>
#include "mapkit/MapLayer.h"
#include "ui/Button.h"
#include "ui/ComboBox.h"
#include "ui/HBox.h"
#include "ui/Text.h"
#include "ui/VBox.h"
#include "ui/Window.h"

using std::string;
using std::vector;

// The master map: one map with every kind of thing that is drawn on a map as a layer in a tree - surface stations, river gauges, dams, buoys, tropical
// storms and the rest - switched on and off, each with its own options, over the same coastlines. A "view" in the box is a saved set of layers and a
// region (the old Rivers, Tropical and Surface observation screens as presets). What is on is remembered.
class MasterMapViewer : public Window, private MapHost {
public:
    explicit MasterMapViewer(Window * parent);

private:
    // MapHost
    MapView& view() override { return *mapView; }
    Window * hostWindow() override { return this; }
    void redraw() override { mapView->map()->update(); }
    void status(const string& text) override { extra = text; updateStatus(); }
    bool claimCell(const QPointF& pixels, double spacing) override;
    void timesChanged() override { rebuildTicks(); }
    void background(std::function<void()> work, std::function<void()> done) override;

    void buildTree();
    void setLayerOn(const string& id, bool on);
    void applyPreset(size_t index);
    void saveState();
    void updateStatus();
    void paintMap(QPainter&);
    void paintLegend(QPainter&);
    void showHover(const QPointF& pixels);
    MapHit bestHit(const QPointF& pixels) const;
    void showOptions(MapLayer *);
    void tick();
    void exportView();
    void exportTo(const QString& path);          // PNG, or PDF by the ending of the name
    void paintExport(QPainter&, int mapSide, double scale, bool vectorOutput);
    // the time bar: stepping and looping through the frames of the layers that have them
    void rebuildTicks();
    void goLive();
    void goTo(int index);
    void togglePlay();
    void advance();
    void closeEventCustom() override { closed = true; }
    void resizeEventCustom() override;
    MapLayer * layerOf(const string& id) const;

    VBox box;
    HBox rowTop;
    HBox rowMain;
    ComboBox comboView;
    Button buttonRefresh;
    Button buttonSave;
    Text textStatus;
    std::unique_ptr<MapView> mapView;
    QLabel * hoverLabel{};
    QWidget * sidePanel{};
    QTreeWidget * tree{};
    QWidget * optionsBox{};
    QCheckBox * legendCheck{};
    QWidget * timeRow{};
    QSlider * timeSlider{};
    QPushButton * playButton{};
    QPushButton * liveButton{};
    QPushButton * stepBack{};
    QPushButton * stepForward{};
    QComboBox * speedCombo{};
    QLabel * timeLabel{};
    QTimer playTimer;
    vector<long> ticks;             // the times the bar steps through: every frame of every layer that has them, ascending
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
    string extra;
    bool building{false};
    bool closed{false};
};

#endif  // MASTERMAPVIEWER_H
