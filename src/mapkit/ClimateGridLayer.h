// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef CLIMATEGRIDLAYER_H
#define CLIMATEGRIDLAYER_H

#include <memory>
#include <vector>
#include "climate/GridAnalysis.h"
#include "climate/UtilityAcisClimate.h"
#include "mapkit/MapLayer.h"

// The climate of the stations (ACIS: the departure of temperature and precipitation from the 1991-2020 normal over the last days, the month so far, last month) made into a filled map
// like the NWS's: each station's value carried to the cells round it (climate/GridAnalysis), coloured in bands. Which measure is a table (AcisClimate::products), which places a table
// (AcisClimate::regions): this layer only draws what is in them.
class ClimateGridLayer : public MapLayer {
public:
    ClimateGridLayer();
    ~ClimateGridLayer() override { *alive = false; }
    string id() const override { return "climate/acis-grid"; }
    string path() const override { return "Climate/Departure from normal, temperature and precipitation (ACIS stations, filled map)"; }
    string source() const override { return "NOAA Regional Climate Centers, ACIS (stations against the 1991-2020 normals)"; }
    string tip() const override { return "How much warmer or colder, wetter or drier than normal (1991-2020) the last days, the month so far or last month have been, from the stations of the Regional Climate Centers' ACIS, carried over the map as filled bands"; }
    bool underCoast() const override { return true; }
    int order() const override { return 11; }
    int refreshSeconds() const override { return 3 * 3600; }
    void refresh(MapHost&) override;
    void paint(QPainter&, MapHost&) override;
    MapHit pick(const QPointF& pixels, MapHost&) const override;
    vector<MapLegendRow> legend() const override;
    QWidget * options(QWidget * parent, const std::function<void()>& changed) override;
    void optionChanged(MapHost& host) override { if (changeRequested) { changeRequested = false; refresh(host); } }
    string summary() const override;
    void setAsOf(const QDateTime& utc, MapHost&) override;

protected:
    void onEnable(MapHost& host) override { refresh(host); }

private:
    struct Part {                       // one region's analysis
        const AcisClimate::Region * region{nullptr};
        GridAnalysis::Field field;
        std::vector<AcisClimate::Station> stations;
        QImage image;
    };
    void period(QDate& from, QDate& to) const;
    QString periodText() const;
    int periodChoice{1};                // the index into the period list
    string productId{"avgt-dep"};
    int opacity{85};
    bool changeRequested{false};
    QDateTime asOf;
    std::vector<Part> parts;
    const AcisClimate::Product * shown{nullptr};   // the product the parts on hand are of
    QString shownPeriod;
    bool loading{false};
    bool again{false};
    string error;
    std::shared_ptr<bool> alive{std::make_shared<bool>(true)};
};

#endif  // CLIMATEGRIDLAYER_H
