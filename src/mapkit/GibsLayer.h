// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef GIBSLAYER_H
#define GIBSLAYER_H

#include <map>
#include <set>
#include <QImage>
#include "mapkit/MapLayer.h"

// NASA GIBS (Global Imagery Browse Services) daily imagery of the polar orbiters as map tiles: true color and false color from VIIRS (NOAA-20, NOAA-21, Suomi NPP) and MODIS
// (Terra, Aqua), the infrared window and the day / night band. Any date back to the start of each layer; the tiles for the view are read as it is moved and kept for the session.
class GibsLayer : public MapLayer {
public:
    GibsLayer();
    string id() const override { return "satellite/polar"; }
    string path() const override { return "Satellite/Polar orbiters (NASA GIBS: VIIRS, MODIS)"; }
    string source() const override { return "NASA GIBS / EOSDIS (VIIRS NOAA-20, NOAA-21, Suomi NPP; MODIS Terra, Aqua)"; }
    string tip() const override { return "Daily imagery of the polar-orbiting satellites as tiles for any date: true color, false color (snow, ice and fire scars), the infrared window and the night lights. A day's picture is built from that day's passes, so the gaps are where no pass reached"; }
    bool underCoast() const override { return true; }
    int order() const override { return 6; }
    void paint(QPainter&, MapHost&) override;
    QWidget * options(QWidget * parent, const std::function<void()>& changed) override;
    string summary() const override;

protected:
    void onEnable(MapHost& host) override { host.redraw(); }

private:
    struct Product {
        const char * id;
        const char * label;
        const char * extension;   // jpg or png
        const char * matrixSet;   // GoogleMapsCompatible_Level9 ...
        int maxZoom;
    };
    static const std::vector<Product>& products();
    string dateText() const;   // yyyy-MM-dd of the day shown
    std::map<string, QImage> tiles;
    std::set<string> asked;
    int productIndex{0};
    int daysBack{1};           // 0 today (a partial day), 1 yesterday ...
    int opacity{100};
    int shownZoom{0};
};

#endif  // GIBSLAYER_H
