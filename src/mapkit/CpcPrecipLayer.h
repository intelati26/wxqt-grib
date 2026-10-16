// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef CPCPRECIPLAYER_H
#define CPCPRECIPLAYER_H

#include <QImage>
#include <vector>
#include "mapkit/MapLayer.h"

// The rain that fell over the last 1 to 60 days on the lower 48: the NOAA Climate Prediction Center's daily gauge analysis (quarter degree, 12Z to 12Z, about 280 KB a day) summed
// over the period. The days are kept on disk, so a second look costs nothing. Totals only: the departure from normal waits for a daily climatology.
class CpcPrecipLayer : public MapLayer {
public:
    CpcPrecipLayer();
    string id() const override { return "precip/cpc-total"; }
    string path() const override { return "Radar and precipitation/CPC rain total, 1 to 60 days (gauge analysis)"; }
    string source() const override { return "NOAA Climate Prediction Center daily gauge analysis (CONUS, 0.25 degree)"; }
    string tip() const override { return "The rain that fell over the last 1 to 60 days over the lower 48, from the Climate Prediction Center's daily gauge analysis (a quarter degree grid, days end at 12Z). The days are downloaded once (about 0.3 MB each) and kept"; }
    bool underCoast() const override { return true; }
    int order() const override { return 9; }
    int refreshSeconds() const override { return 3 * 3600; }
    void refresh(MapHost&) override;
    void optionChanged(MapHost& host) override { if (changeRequested) { changeRequested = false; refresh(host); } }
    void paint(QPainter&, MapHost&) override;
    MapHit pick(const QPointF& pixels, MapHost&) const override;
    vector<MapLegendRow> legend() const override;
    QWidget * options(QWidget * parent, const std::function<void()>& changed) override;
    string summary() const override;

protected:
    void onEnable(MapHost& host) override { refresh(host); }

private:
    struct Band {
        double from;   // inches
        QColor color;
    };
    vector<Band> bands() const;
    void colorize();
    bool inches() const { return !millimeters; }
    int days{14};
    int opacity{80};
    bool millimeters{false};
    bool loading{false};
    bool changeRequested{false};
    int loadedDays{0};         // the period the totals on hand are for
    int usedDays{0};           // how many of those days had a file
    QDate lastDay;
    std::vector<float> total;  // millimeters, 300 x 120 from the south-west corner; below zero: no data
    QImage picture;            // the colours, the top row the northern one
    string error;
};

#endif  // CPCPRECIPLAYER_H
