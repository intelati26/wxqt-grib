// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef SARWINDSLAYER_H
#define SARWINDSLAYER_H

#include <map>
#include <memory>
#include <set>
#include <vector>
#include <QDateTime>
#include <QImage>
#include <QPolygonF>
#include "hurricane/UtilitySarWinds.h"
#include "mapkit/MapLayer.h"

// The newest SAR wind scenes of the world's storms (NOAA STAR: RADARSAT Constellation, Sentinel-1) on the map: a mark where each scene is, and, when the map is zoomed to the storm, the scene's
// wind field itself (10 m wind, painted from STAR's NetCDF file, 0.005 degrees, about 500 m) in the storm's colors. A click on a scene opens the storm's SAR winds window.
class SarWindsLayer : public MapLayer {
public:
    string id() const override { return "satellite/sar-winds"; }
    string path() const override { return "Satellite/SAR winds of the storms (RCM, Sentinel-1: NOAA STAR)"; }
    string source() const override { return "NOAA STAR SAR winds (RADARSAT Constellation, Government of Canada; Copernicus Sentinel-1)"; }
    string tip() const override { return "The latest synthetic aperture radar passes over the world's storms (RADARSAT Constellation and Sentinel-1): a mark at each scene, and, zoomed in, the 10 m wind it measured at about 500 m detail. Click a scene for the storm's SAR winds window"; }
    int order() const override { return 8; }
    int refreshSeconds() const override { return 1800; }
    void refresh(MapHost&) override;
    void paint(QPainter&, MapHost&) override;
    MapHit pick(const QPointF&, MapHost&) const override;
    vector<MapLegendRow> legend() const override;
    QWidget * options(QWidget * parent, const std::function<void()>& changed) override;
    string summary() const override;
    bool working() const override;

protected:
    void onEnable(MapHost& host) override { refresh(host); }

private:
    // a scene as it is kept: the winds and the directions on a regular (Mercator) grid, where it is, and its picture
    struct SceneData {
        int width{0}, height{0};
        double west{0}, south{0}, east{0}, north{0}, maxKnots{0};
        double stepRad{0}, yTop{0};            // one cell: radians of longitude (and of Mercator y), the Mercator y of the top edge
        std::vector<uint16_t> wind;            // tenths of a knot, 65535: none (no value, land, a retrieval that gave up)
        std::vector<uint16_t> direction;       // tenths of a degree the wind blows from (the model's, the one the retrieval used), 65535: none
        QPolygonF outline;                     // the corners of the scene (lon, lat)
        QImage image;
    };
    struct Entry {
        string stormId, stormName;
        int year{0};
        UtilitySarWinds::Scene scene;
        string netcdf;                       // the scene's wind file
        QDateTime when;
        std::shared_ptr<SceneData> data;     // once painted
        bool rendering{false}, failed{false};
    };
    void render(MapHost&, size_t index);
    void paintBarbs(QPainter&, MapHost&, const SceneData&) const;
    bool showBarbs{true};
    bool loading{false};
    string error;
    std::shared_ptr<std::vector<Entry>> entries;
};

#endif  // SARWINDSLAYER_H
