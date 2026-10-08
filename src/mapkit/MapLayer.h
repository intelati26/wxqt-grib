// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef MAPLAYER_H
#define MAPLAYER_H

#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <QColor>
#include <QPainter>
#include <QPointF>
#include <QString>
#include <QWidget>
#include "radar/MapLegend.h"
#include "radar/MapView.h"
#include "ui/Window.h"

using std::string;
using std::vector;

// Everything on the master map is a MapLayer: something that can be switched on in the layer tree, loads its own data when it is, paints itself over the
// map, answers "what is under the pointer", and tells the legend what its colours mean. The MapHost is what a layer may ask of the screen it is on.

// one thing under the pointer, as a layer sees it
struct MapHit {
    double distance{1e9};                          // pixels from the pointer; the nearest wins within a layer
    double reach{12.0};                            // how near the pointer has to be, pixels
    int priority{0};                               // between layers: the higher is offered first (a dam over the gauge under it)
    QString text;                                  // the hover popup
    std::function<void(Window *)> open;            // a click opens this (empty: nothing to open)
    bool valid() const { return distance <= reach && !text.isEmpty(); }
};

class MapHost {
public:
    virtual ~MapHost() = default;
    virtual MapView& view() = 0;
    virtual Window * hostWindow() = 0;
    virtual void redraw() = 0;                                   // something changed: paint again
    virtual void status(const string&) = 0;                      // a line under the toolbar
    virtual void timesChanged() = 0;                             // a time-aware layer has new frames (the time bar looks again)
    // a patch of the map (`spacing` pixels square) can be claimed once per painting; true when it was free. Layers that thin their marks use it so that
    // two layers do not both put a mark on the same spot.
    virtual bool claimCell(const QPointF& pixels, double spacing) = 0;
    // work on another thread, then `done` on this one (dropped when the screen has been closed)
    virtual void background(std::function<void()> work, std::function<void()> done) = 0;
};

class MapLayer {
public:
    virtual ~MapLayer() = default;
    virtual string id() const = 0;                               // stable, for the saved settings: "obs/airports"
    virtual string path() const = 0;                             // where it sits in the tree: "Observations/Airports (METAR)"
    virtual string tip() const { return {}; }
    virtual bool underCoast() const { return false; }           // a base layer: painted before the coastlines and borders, so that they stay on top
    virtual int order() const { return 50; }                     // painting order, lowest first: pictures under 20, areas 20-39, lines and marks 40 and up
    virtual int refreshSeconds() const { return 0; }             // 0: only when switched on or refreshed by hand
    void enable(MapHost& host) { on = true; onEnable(host); }
    void disable() { on = false; onDisable(); }
    bool enabled() const { return on; }
    virtual void refresh(MapHost&) {}
    virtual void optionChanged(MapHost&) {}                      // one of its options was changed (called before the map is painted again)
    virtual void paint(QPainter&, MapHost&) = 0;                 // in window units, as the radar screens' topLayer
    virtual MapHit pick(const QPointF& /*pixels*/, MapHost&) const { return {}; }
    virtual vector<MapLegendRow> legend() const { return {}; }
    // its own choices (colour by, filters), shown under the tree when selected; `changed` is called when one of them changes
    virtual QWidget * options(QWidget * /*parent*/, const std::function<void()>& /*changed*/) { return nullptr; }
    virtual string summary() const { return {}; }
    virtual string source() const { return {}; }                 // who made the data, for the line under an exported picture
    // time: a layer with frames in time (radar scans, a forecast's hours) lists them, and shows the one the time bar asks for
    virtual bool timeAware() const { return false; }
    virtual vector<long> times() const { return {}; }            // seconds since 1970, ascending: the frames that can be shown now
    virtual void prepareTimes(MapHost&) {}                       // load the frames for a loop (the bar calls it when a loop is wanted); timesChanged() when done
    virtual void showTime(long /*seconds*/, MapHost&) {}         // show the frame at or just before this time; 0 is "live": the newest
    virtual string timeText() const { return {}; }               // when the frame on show is valid                // a short line for the status: "5,300 stations"

protected:
    virtual void onEnable(MapHost&) {}
    virtual void onDisable() {}

private:
    bool on{false};
};

// the layers the master map offers, and the saved sets of them
namespace MapCatalog {
    vector<std::unique_ptr<MapLayer>> makeLayers();
    struct Preset {
        string name;
        vector<string> layers;                                   // ids
        double minLat, maxLat, minLon, maxLon;                   // the region
    };
    const vector<Preset>& presets();
}

#endif  // MAPLAYER_H
