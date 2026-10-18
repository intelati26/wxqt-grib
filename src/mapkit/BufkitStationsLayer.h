// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef BUFKITSTATIONSLAYER_H
#define BUFKITSTATIONSLAYER_H

#include "mapkit/MapLayer.h"

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

private:
    struct Shown {
        const void * station;
        QPointF units;
        QPointF pixels;
        string id;
    };
    std::vector<Shown> choose(MapHost&) const;
};

#endif  // BUFKITSTATIONSLAYER_H
