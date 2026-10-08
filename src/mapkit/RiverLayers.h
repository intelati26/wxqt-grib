// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef RIVERLAYERS_H
#define RIVERLAYERS_H

#include <memory>
#include <vector>
#include "buoys/BuoyData.h"
#include "dams/DamData.h"
#include "mapkit/MapLayer.h"
#include "rivers/UtilityRivers.h"

// The Rivers screen's three kinds of marks as layers: NWS river gauges (a dot in the flood category's colour), Corps of Engineers dams (a diamond: orange
// generating, blue releasing) and NDBC buoys (a square coloured by wind or water temperature).
class GaugeLayer : public MapLayer {
public:
    string id() const override { return "rivers/gauges"; }
    string path() const override { return "Rivers and water/River gauges (NWS)"; }
    string source() const override { return "NWS National Water Prediction Service river gauges"; }
    string tip() const override { return "About 13,000 NWS river gauges, each in its flood category's colour; click one for its hydrograph, forecast and the National Water Model"; }
    int order() const override { return 40; }
    int refreshSeconds() const override { return 600; }
    void refresh(MapHost&) override;
    void paint(QPainter&, MapHost&) override;
    MapHit pick(const QPointF&, MapHost&) const override;
    vector<MapLegendRow> legend() const override;
    QWidget * options(QWidget * parent, const std::function<void()>& changed) override;
    string summary() const override;

protected:
    void onEnable(MapHost& host) override { refresh(host); }

private:
    bool shown(const UtilityRivers::Gauge&) const;
    bool loading{false};
    string error;
    int filter{0};
    std::shared_ptr<vector<UtilityRivers::Gauge>> gauges;
};

class DamLayer : public MapLayer {
public:
    string id() const override { return "rivers/dams"; }
    string path() const override { return "Rivers and water/Dams (Corps of Engineers)"; }
    string source() const override { return "US Army Corps of Engineers CWMS"; }
    string tip() const override { return "Corps of Engineers hydropower dams in the Little Rock and Tulsa districts: the latest release, power generated and pool; click one for its history"; }
    int order() const override { return 45; }
    int refreshSeconds() const override { return 600; }
    void refresh(MapHost&) override;
    void paint(QPainter&, MapHost&) override;
    MapHit pick(const QPointF&, MapHost&) const override;
    vector<MapLegendRow> legend() const override;
    string summary() const override;

protected:
    void onEnable(MapHost& host) override { refresh(host); }

private:
    bool loading{false};
    std::shared_ptr<vector<DamData::Latest>> dams;
};

class BuoyLayer : public MapLayer {
public:
    string id() const override { return "rivers/buoys"; }
    string path() const override { return "Rivers and water/Buoys and coastal stations (NDBC)"; }
    string source() const override { return "NOAA National Data Buoy Center"; }
    string tip() const override { return "NOAA's National Data Buoy Center buoys and coastal stations: the latest wind, waves, pressure and temperatures; click one for its history"; }
    int order() const override { return 42; }
    int refreshSeconds() const override { return 600; }
    void refresh(MapHost&) override;
    void paint(QPainter&, MapHost&) override;
    MapHit pick(const QPointF&, MapHost&) const override;
    vector<MapLegendRow> legend() const override;
    QWidget * options(QWidget * parent, const std::function<void()>& changed) override;
    string summary() const override;

protected:
    void onEnable(MapHost& host) override { refresh(host); }

private:
    QColor colorOf(const BuoyData::Marker&) const;
    bool loading{false};
    int colorBy{0};   // 0 wind, 1 water temperature
    std::shared_ptr<vector<BuoyData::Marker>> buoys;
};

#endif  // RIVERLAYERS_H
