// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef SPACELAYERS_H
#define SPACELAYERS_H

#include <memory>
#include "mapkit/MapLayer.h"
#include "space/UtilitySpace.h"

// The aurora forecast of NOAA's OVATION model as a layer: the chance of seeing the aurora on a 1 degree grid, in colours from green to red.
class AuroraLayer : public MapLayer {
public:
    string id() const override { return "space/aurora"; }
    string path() const override { return "Space weather/Aurora forecast (OVATION)"; }
    string source() const override { return "NOAA Space Weather Prediction Center (OVATION)"; }
    string tip() const override { return "The chance of seeing the aurora (NOAA SWPC's OVATION model, for the next half hour or so), on a 1 degree grid"; }
    int order() const override { return 22; }
    int refreshSeconds() const override { return 300; }
    void refresh(MapHost&) override;
    void paint(QPainter&, MapHost&) override;
    MapHit pick(const QPointF&, MapHost&) const override;
    vector<MapLegendRow> legend() const override;
    string summary() const override;

protected:
    void onEnable(MapHost& host) override { refresh(host); }

private:
    static QColor colorOf(float chance);
    bool loading{false};
    std::shared_ptr<UtilitySpace::Ovation> data;
};

#endif  // SPACELAYERS_H
