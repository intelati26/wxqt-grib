// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef GOESLAYER_H
#define GOESLAYER_H

#include <memory>
#include <QImage>
#include "mapkit/MapLayer.h"

// A GOES full-disk picture (NOAA / NESDIS STAR, 5424 x 5424 pixels, about 2 km at the sub-satellite point) as the base of the map, under the coastlines and every
// other layer. The picture is in the satellite's own view (the ABI fixed grid), so every screen pixel is looked up in it through the geostationary projection
// (GOES-R Product User's Guide); the result is kept until the map is moved or zoomed. It is read when the layer is switched on and when the layers are
// refreshed by hand, not on a timer: the picture is about 17 MB.
class GoesLayer : public MapLayer {
public:
    string id() const override { return "satellite/goes"; }
    string path() const override { return "Satellite/GOES full disk (base layer)"; }
    string tip() const override { return "A GOES full-disk picture under everything else, about 2 km a pixel. Read when switched on and when the layers are refreshed (the file is about 17 MB)."; }
    string source() const override { return "NOAA / NESDIS STAR GOES ABI"; }
    bool underCoast() const override { return true; }
    int order() const override { return 5; }
    void refresh(MapHost&) override;
    void paint(QPainter&, MapHost&) override;
    QWidget * options(QWidget * parent, const std::function<void()>& changed) override;
    void optionChanged(MapHost& host) override { if (reloadNeeded) { reloadNeeded = false; refresh(host); } }
    string summary() const override;

protected:
    void onEnable(MapHost& host) override { refresh(host); }

private:
    struct Satellite {
        const char * name;
        const char * folder;
        double longitude;      // the sub-satellite longitude, degrees east
    };
    static const Satellite& satellite(int index);
    string url() const;
    bool loading{false};
    bool reloadNeeded{false};
    string error;
    int satelliteIndex{0};     // 0 GOES-East, 1 GOES-West
    int product{0};            // 0 Sandwich, 1 GeoColor
    int brightness{100};       // percent
    string loadedAt;
    std::shared_ptr<QImage> picture;
    int generation{0};
    // the picture as it was drawn: reused until the view changes
    QImage cache;
    double cacheZoom{0}, cacheX{0}, cacheY{0};
    int cacheWidth{0}, cacheHeight{0};
    int cacheSatellite{-1};
    int cacheBrightness{-1};
    const void * cachePicture{nullptr};
};

#endif  // GOESLAYER_H
