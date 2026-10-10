// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef OBSLAYERS_H
#define OBSLAYERS_H

#include <memory>
#include <vector>
#include "mapkit/MapLayer.h"
#include "obs/SurfaceStation.h"

// Surface stations on the master map: the airports' METARs and the MADIS mesonets, each a layer; the barbs, values, colouring and units are shared by
// both (set under either).
class StationLayer : public MapLayer {
public:
    explicit StationLayer(bool airports) : airports{airports} {}
    string id() const override { return airports ? "obs/airports" : "obs/mesonet"; }
    string path() const override { return airports ? "Observations/Airports (METAR)" : "Observations/Mesonets and other networks (MADIS)"; }
    string tip() const override;
    string source() const override { return airports ? "NWS Aviation Weather Center METARs" : "NOAA MADIS mesonet data"; }
    int order() const override { return airports ? 60 : 61; }   // the airports first, so that they win a crowded patch
    int refreshSeconds() const override { return airports ? 120 : 1800; }
    void refresh(MapHost&) override;
    void paint(QPainter&, MapHost&) override;
    MapHit pick(const QPointF&, MapHost&) const override;
    vector<MapLegendRow> legend() const override;
    QWidget * options(QWidget * parent, const std::function<void()>& changed) override;
    string summary() const override;

protected:
    void onEnable(MapHost& host) override { refresh(host); }

private:
    bool networkShown(const SurfaceStation&) const;
    QColor dotColor(const SurfaceStation&) const;
    bool airports;
    bool loading{false};
    string error;
    std::shared_ptr<std::vector<SurfaceStation>> stations;
    struct Drawn {
        size_t index;
        QPointF at;
    };
    mutable std::vector<Drawn> drawn;
};

#endif  // OBSLAYERS_H
