// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef EUMETVIEWLAYER_H
#define EUMETVIEWLAYER_H

#include <map>
#include <set>
#include <vector>
#include <QDateTime>
#include <QImage>
#include "mapkit/MapLayer.h"

// EUMETSAT's open map service (EUMETView, no login): the Metop ASCAT coastal winds, the Sentinel-3 OLCI true color and the Meteosat Third Generation pictures of the Atlantic, Europe and Africa. A picture
// is asked for as the view is (one image of the visible part), and kept for the session. The satellites of the swath products (ASCAT, OLCI) make a strip a pass: a window of hours ending at the time shown
// is laid together, and the stamp on the map says the window and the newest the service has. The Meteosat pictures are one every ten minutes.
class EumetViewLayer : public MapLayer {
public:
    EumetViewLayer();
    string id() const override { return "satellite/eumetview"; }
    string path() const override { return "Satellite/EUMETSAT EUMETView (ASCAT winds, Sentinel-3, Meteosat)"; }
    string source() const override { return "EUMETSAT EUMETView (Metop ASCAT, Copernicus Sentinel-3, Meteosat Third Generation)"; }
    string tip() const override { return "EUMETSAT's open picture service: the Metop ASCAT coastal winds, Sentinel-3 OLCI true color (daily strips of the passes) and the Meteosat Third Generation geocolor, infrared and visible pictures (0 degrees: the Atlantic, Europe, Africa). The stamp on the map says what time it is from, and the newest time the service has."; }
    bool underCoast() const override { return true; }
    int order() const override { return 7; }
    int refreshSeconds() const override { return 900; }
    void refresh(MapHost&) override;
    void paint(QPainter&, MapHost&) override;
    QWidget * options(QWidget * parent, const std::function<void()>& changed) override;
    string summary() const override;
    string timeText() const override;
    bool working() const override { return fetching > 0; }
    void setAsOf(const QDateTime& utc, MapHost&) override;

protected:
    void onEnable(MapHost& host) override { refresh(host); }

private:
    struct Product {
        const char * layer;
        const char * label;
        bool swath;               // a strip a pass: a window of hours is laid together; false: one picture at a time (every ten minutes)
        int stepMinutes;          // of a picture (frames): the time is rounded down to it
    };
    static const std::vector<Product>& products();
    const Product& product() const { return products()[static_cast<size_t>(productIndex)]; }
    QDateTime endTime() const;     // the time shown: the one asked for, else the newest the service has (else now)
    string timeArgument() const;   // the WMS "time" of the picture
    string imageUrl(double west, double south, double east, double north, int width, int height) const;
    std::map<string, QImage> images;
    std::set<string> asked;
    std::map<string, QDateTime> latest;   // the newest time of each layer, from the service's capabilities
    bool capabilitiesAsked{false};
    QDateTime capabilitiesAt;
    QDateTime asOf;               // invalid: the newest
    int productIndex{0};
    int windowHours{12};
    int opacity{100};
    int fetching{0};
    QString shownBox;
};

#endif  // EUMETVIEWLAYER_H
