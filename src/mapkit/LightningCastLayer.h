// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef LIGHTNINGCASTLAYER_H
#define LIGHTNINGCASTLAYER_H

#include <QColor>
#include <QPolygonF>
#include "mapkit/MapLayer.h"

// NOAA / CIMSS LightningCast: the chance of lightning in the next hour from GOES-East, as contours at 10, 30, 50, 70 and 90 percent. The feed is a GRLevelX placefile
// of the last hour in 5 minute scans, read as the frames of the map's time bar.
class LightningCastLayer : public MapLayer {
public:
    string id() const override { return "satellite/lightningcast"; }
    string path() const override { return "Satellite/LightningCast, chance of lightning in the next hour (GOES-East, NOAA and CIMSS)"; }
    string source() const override { return "NOAA / CIMSS LightningCast (GOES-East ABI), experimental"; }
    string tip() const override { return "LightningCast from NOAA and the University of Wisconsin CIMSS: the chance of lightning in the next 60 minutes from GOES-East, lines at 10, 30, 50, 70 and 90 percent, every 5 minutes for the last hour. Experimental: informational use only"; }
    int order() const override { return 46; }
    int refreshSeconds() const override { return 120; }
    void refresh(MapHost&) override;
    void paint(QPainter&, MapHost&) override;
    bool timeAware() const override { return true; }
    vector<long> times() const override;
    void showTime(long seconds, MapHost&) override;
    string timeText() const override;
    vector<MapLegendRow> legend() const override;
    string summary() const override;

protected:
    void onEnable(MapHost& host) override { refresh(host); }

public:
    struct Contour {
        int percent;
        QColor color;
        QPolygonF points;   // x = longitude, y = latitude
    };
    struct Frame {
        long seconds;
        vector<Contour> contours;
    };

private:
    const Frame * frameFor(long seconds) const;
    vector<Frame> frames;
    long shown{0};
    bool loading{false};
    string error;
};

#endif  // LIGHTNINGCASTLAYER_H
