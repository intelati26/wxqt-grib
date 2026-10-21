// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef SATELLITEPASSLAYER_H
#define SATELLITEPASSLAYER_H

#include <memory>
#include <vector>
#include <QPointF>
#include "mapkit/MapLayer.h"
#include "space/SatellitePasses.h"

// Where the polar orbiters will be in the next hours, on the map: the swath of each instrument as a band along the ground track with the time (UTC) of the satellite at every half hour, so that
// the passes over a storm or a place can be read off the picture; the places of interest as marks. The same element sets and model as the schedule window (space/SatellitePassViewer).
class SatellitePassLayer : public MapLayer {
public:
    string id() const override { return "satellite/passes"; }
    string path() const override { return "Satellite/Polar orbiter passes: swaths of the next hours (VIIRS, MODIS, microwave, ASCAT, SAR)"; }
    string source() const override { return "CelesTrak element sets, SGP4"; }
    string tip() const override { return "Where the polar-orbiting weather and radar satellites go in the next hours: the swath of each instrument as a band with the time of the satellite along it. The places of interest are marked"; }
    int order() const override { return 66; }
    int refreshSeconds() const override { return 600; }
    void refresh(MapHost&) override;
    void paint(QPainter&, MapHost&) override;
    MapHit pick(const QPointF&, MapHost&) const override;
    vector<MapLegendRow> legend() const override;
    QWidget * options(QWidget * parent, const std::function<void()>& changed) override;
    void optionChanged(MapHost& host) override { refresh(host); }
    string summary() const override;
    bool working() const override { return loading; }

protected:
    void onEnable(MapHost& host) override { refresh(host); }

private:
    struct Band {
        string satellite, instrument;
        SatellitePasses::Kind kind;
        std::vector<std::array<QPointF, 4>> quads;     // (lon, lat) of the two edges at two moments
        std::vector<std::pair<QPointF, QString>> labels;   // where the satellite is at a time worth saying
        std::vector<QPointF> track;                    // (lon, lat) every minute
        long long when{0};                             // the moment of the closest approach (a pass over a storm or place)
        QString text;                                  // "NOAA-20 VIIRS 15:02Z: CHAMPI"
    };
    struct Data {
        std::vector<Band> bands;
        std::vector<SatellitePasses::Place> places;
        string problem;
    };
    int hours{6};
    bool onlyOver{true};     // only the passes that cover a storm or a place of interest, as the footprint round the moment of the closest approach
    bool kinds[4]{true, true, true, true};
    bool loading{false};
    std::shared_ptr<Data> data;
};

#endif  // SATELLITEPASSLAYER_H
