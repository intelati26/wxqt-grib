// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef BUFKITSTATIONSLAYER_H
#define BUFKITSTATIONSLAYER_H

#include <deque>
#include <map>
#include <memory>
#include <set>
#include "mapkit/MapLayer.h"
#include "sounding/UtilityBufkit.h"

// The stations of the BUFKIT model soundings (about 1500 airports and sounding sites) as buttons on the map: the NWS offices' stations first, then the other airports, none nearer than a button
// to another, more as the map is zoomed in. A click opens that station's RRFS sounding (every forecast hour). The Model Viewer page's Stations button.
class BufkitStationsLayer : public MapLayer {
public:
    string id() const override { return "model/stations"; }
    string path() const override { return "Models/Sounding stations (click one for its model sounding)"; }
    string source() const override { return "Penn State BUFKIT feed (RRFS soundings)"; }
    string tip() const override { return "A button at each station of the BUFKIT model soundings (airports and sounding sites): click one for that station's RRFS sounding, every forecast hour. Zoom in for more stations"; }
    int order() const override { return 75; }
    void paint(QPainter&, MapHost&) override;
    MapHit pick(const QPointF&, MapHost&) const override;
    string summary() const override;
    QWidget * options(QWidget * parent, const std::function<void()>& changed) override;
    void optionChanged(MapHost& host) override { host.timesChanged(); host.redraw(); }
    // with the hodographs on, the stations draw each its hodograph of the RRFS winds at the time on the bar, and the bar steps through the hours of the run
    bool timeAware() const override { return hodographs; }
    vector<long> times() const override;
    void showTime(long seconds, MapHost& host) override { shown = seconds; host.redraw(); }
    string timeText() const override;
    ~BufkitStationsLayer() override { *alive = false; }
    BufkitStationsLayer();

private:
    // a station's hodograph at the time on view: false when its file is not in yet, or has no profile for the hour
    bool glyph(const string& station, std::vector<QPointF>& tips, QPointF& storm, double& srh) const;
    void pump(MapHost& host) const;
    long validNow() const;
    bool hodographs{false};
    long shown{0};
    mutable std::map<string, std::shared_ptr<UtilityBufkit::Run>> runs;
    mutable std::set<string> asked;
    mutable std::deque<string> queue;
    mutable int running{0};
    std::shared_ptr<bool> alive{std::make_shared<bool>(true)};
    struct Shown {
        const void * station;
        QPointF units;
        QPointF pixels;
        string id;
    };
    std::vector<Shown> choose(MapHost&) const;
};

#endif  // BUFKITSTATIONSLAYER_H
