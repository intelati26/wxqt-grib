// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef MRMSLAYER_H
#define MRMSLAYER_H

#include <map>
#include <memory>
#include <vector>
#include "mapkit/MapLayer.h"
#include "mrms/UtilityMrms.h"

// NOAA MRMS radar-derived products (reflectivity, hail, rotation, rain ...) as a picture under the other layers: the newest scan of the product chosen
// in the options, looked up cell by cell for every screen pixel so the detail stays at full resolution at any zoom. The first of the master map's
// raster layers; the opacity is set in the options so that the marks below and above can be read.
class MrmsLayer : public MapLayer {
public:
    string id() const override { return "radar/mrms"; }
    string path() const override { return "Radar and precipitation/MRMS (radar-derived products)"; }
    string tip() const override { return "NOAA's Multi-Radar Multi-Sensor products: reflectivity, hail size, rotation, rain rate and totals; the newest scan of the product chosen in the options"; }
    int order() const override { return 10; }
    int refreshSeconds() const override { return 120; }
    bool timeAware() const override { return true; }
    vector<long> times() const override;
    void prepareTimes(MapHost&) override;
    void showTime(long seconds, MapHost&) override;
    string timeText() const override;
    void refresh(MapHost&) override;
    void optionChanged(MapHost& host) override { if (reloadNeeded) { reloadNeeded = false; refresh(host); } }
    void paint(QPainter&, MapHost&) override;
    MapHit pick(const QPointF&, MapHost&) const override;
    vector<MapLegendRow> legend() const override;
    QWidget * options(QWidget * parent, const std::function<void()>& changed) override;
    string summary() const override;

protected:
    void onEnable(MapHost& host) override { refresh(host); }

private:
    const UtilityMrms::Product& product() const;
    void rebuildColors();
    void display(const UtilityMrms::Frame&);
    bool loading{false};
    string error;
    int productIndex{0};
    int opacity{80};         // percent
    bool us{true};
    UtilityMrms::Frame frame;
    QByteArray indices;      // one 8-bit index per cell of the frame's grid, 0 transparent
    QVector<QRgb> colors;
    bool have{false};
    bool reloadNeeded{false};
    bool preparing{false};
    bool loopWanted{false};
    int loopLength{24};      // frames in a loop
    long shownSeconds{0};    // 0: the newest scan (live); else the scan at or before this time
    std::vector<UtilityMrms::Scan> scans;               // the server's list for the product, oldest first
    std::map<long, UtilityMrms::Frame> frames;          // the scans loaded, by time (kept compressed)
    int generation{0};
};

#endif  // MRMSLAYER_H
