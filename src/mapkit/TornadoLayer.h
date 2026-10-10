// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef TORNADOLAYER_H
#define TORNADOLAYER_H

#include <memory>
#include "mapkit/MapLayer.h"
#include "tornado/TornadoData.h"

// The SPC tornado database as a layer: the tracks of the years chosen in the options (the last year, five years, ten years or all), by rating, as lines from the start
// to the end and dots where the end is not known. Hover gives the row of the one nearest the pointer when it is near enough to tell.
class TornadoLayer : public MapLayer {
public:
    string id() const override { return "severe/tornadoes"; }
    string path() const override { return "Severe weather/Tornado tracks (SPC history since 1950)"; }
    string tip() const override { return "The SPC tornado database: every tornado of the years chosen, coloured by its (E)F rating; the data run to the end of 2025"; }
    string source() const override { return "NOAA Storm Prediction Center tornado database"; }
    int order() const override { return 48; }
    void paint(QPainter&, MapHost&) override;
    MapHit pick(const QPointF&, MapHost&) const override;
    vector<MapLegendRow> legend() const override;
    QWidget * options(QWidget * parent, const std::function<void()>& changed) override;
    string summary() const override;

protected:
    void onEnable(MapHost& host) override;

private:
    bool wanted(const UtilityTornado::Tornado&) const;
    bool loading{false};
    int years{1};          // 0 the last year of the data, 1 the last 5, 2 the last 10, 3 all
    int rating{1};         // 0 all, 1 EF1 and up, 2 EF2, 3 EF3
    std::shared_ptr<const TornadoData::Database> db;
};

#endif  // TORNADOLAYER_H
