// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef GIBSLAYER_H
#define GIBSLAYER_H

#include <map>
#include <set>
#include <vector>
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
    // the days as frames of the map's time bar: stepping or looping through them with the other layers on top
    bool timeAware() const override { return true; }
    vector<long> times() const override;
    void prepareTimes(MapHost&) override;
    void showTime(long seconds, MapHost&) override;
    string timeText() const override;
    void setAsOf(const QDateTime& utc, MapHost&) override;
    QWidget * options(QWidget * parent, const std::function<void()>& changed) override;
    string summary() const override;
    vector<MapLegendRow> legend() const override;

protected:
    void onEnable(MapHost& host) override { host.redraw(); }

private:
    struct Product {
        const char * id;
        const char * label;
        const char * extension;   // jpg or png
        const char * matrixSet;   // GoogleMapsCompatible_Level9 ...
        int maxZoom;
        const char * colormap{nullptr};   // GIBS's color map of the product (its legend), for the science products
    };
    static const std::vector<Product>& products();
    string dateText() const;   // yyyy-MM-dd of the day shown
    struct Tile {
        int z, x, y;
    };
    std::vector<Tile> tilesInView(MapHost&, int& zoom) const;   // the tiles that cover the view, at the zoom that matches its pixels
    void request(MapHost&, const Product&, const string& date, const Tile&);   // from the disk copy, or the network, once
    static string tileKey(const Product&, const string& date, const Tile&);
    string urlOf(const Product&, const string& date, const Tile&) const;
    std::map<string, vector<MapLegendRow>> legends;   // by color map, read once from GIBS
    std::set<string> legendAsked;
    std::map<string, QImage> tiles;
    std::set<string> asked;
    int productIndex{0};
    int daysBack{1};           // 0 today (a partial day), 1 yesterday ...
    int opacity{100};
    int shownZoom{0};
    int liveDays{1};           // the day the bar's "latest" shows (the choice in the options)
};

#endif  // GIBSLAYER_H
