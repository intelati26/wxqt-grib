// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef SEVEREAREASLAYERS_H
#define SEVEREAREASLAYERS_H

#include <QDateTime>
#include <QPointF>
#include <QString>
#include <memory>
#include <vector>
#include "mapkit/MapLayer.h"

// The areas the NWS and the SPC draw for severe weather, as polygons on the master map.
//
// WarningPolygonsLayer: the storm-based warnings (tornado, severe thunderstorm, flash flood, flood and the advisories, special marine and the rest) from the Iowa Environmental Mesonet's
// copy of the NWS products (live, and of any time of the past: the map's "as of" time) and the SPC's tornado and severe thunderstorm watches (live: NOAA's WWA map service). Hover says what it is,
// who issued it, when it runs out and its tags (tornado observed, hail size, wind gust, damage threat, PDS, emergency); a click opens the warning's text.
//
// MesoscaleDiscussionLayer: the SPC's mesoscale discussions in force (NOAA's map service). Hover gives the areas, what it concerns, when it is valid and its summary; a click opens the whole text.
namespace SevereAreas {
    struct Area {
        std::vector<std::vector<QPointF>> rings;   // (longitude, latitude)
        int kind{0};                               // see the layers
        bool strong{false};                        // an emergency
        QString title;
        QString text;                              // the hover text
        QString textUrl;                           // what a click opens ("" none)
        QString fullText;                          // or the text itself, when it was read with the polygon
        QString windowTitle;
        long begin{0}, end{0};
    };
}

class WarningPolygonsLayer : public MapLayer {
public:
    WarningPolygonsLayer();
    ~WarningPolygonsLayer() override { *alive = false; }
    string id() const override { return "severe/warnings"; }
    string path() const override { return "Severe weather/NWS warnings and watches (polygons)"; }
    string source() const override { return "NWS warnings (Iowa Environmental Mesonet), SPC watches (NOAA)"; }
    string tip() const override { return "The NWS storm-based warning polygons (tornado, severe thunderstorm, flash flood, flood, marine) and the SPC's tornado and severe thunderstorm watches. Hover for what it is, when it ends and its tags; click for the text of the warning. Of a time of the past (the history page) the warnings in force then; the watches are only live"; }
    int order() const override { return 44; }
    int refreshSeconds() const override { return 120; }
    void refresh(MapHost&) override;
    void paint(QPainter&, MapHost&) override;
    MapHit pick(const QPointF&, MapHost&) const override;
    vector<MapLegendRow> legend() const override;
    QWidget * options(QWidget * parent, const std::function<void()>& changed) override;
    string summary() const override;
    bool working() const override { return loading; }
    void setAsOf(const QDateTime& utc, MapHost&) override;

protected:
    void onEnable(MapHost& host) override { refresh(host); }

private:
    bool wanted(int kind) const;
    bool loading{false};
    int generation{0};
    QDateTime asOf;
    string error;
    std::vector<SevereAreas::Area> areas;
    int enabledMask{0x1ff};   // the kinds shown
    std::shared_ptr<bool> alive{std::make_shared<bool>(true)};
};

// The WPC's mesoscale precipitation discussions in force (flash flooding and heavy rain): the outline is the LAT...LON of the text, which WPC publishes with each one.
class MesoscalePrecipitationLayer : public MapLayer {
public:
    ~MesoscalePrecipitationLayer() override { *alive = false; }
    string id() const override { return "severe/mpd"; }
    string path() const override { return "Severe weather/WPC mesoscale precipitation discussions"; }
    string source() const override { return "NOAA Weather Prediction Center (mesoscale precipitation discussions)"; }
    string tip() const override { return "The WPC's mesoscale precipitation discussions in force (heavy rain and flash flooding), outlined on the map. Hover for the areas affected, what it concerns, when it is valid and its summary; click for the whole discussion. Live only"; }
    int order() const override { return 42; }
    int refreshSeconds() const override { return 300; }
    void refresh(MapHost&) override;
    void paint(QPainter&, MapHost&) override;
    MapHit pick(const QPointF&, MapHost&) const override;
    vector<MapLegendRow> legend() const override;
    string summary() const override;
    bool working() const override { return loading; }

protected:
    void onEnable(MapHost& host) override { refresh(host); }

private:
    bool loading{false};
    string error;
    std::vector<SevereAreas::Area> areas;
    std::shared_ptr<bool> alive{std::make_shared<bool>(true)};
};

class MesoscaleDiscussionLayer : public MapLayer {
public:
    ~MesoscaleDiscussionLayer() override { *alive = false; }
    string id() const override { return "severe/mcd"; }
    string path() const override { return "Severe weather/SPC mesoscale discussions"; }
    string source() const override { return "NOAA Storm Prediction Center (mesoscale discussions)"; }
    string tip() const override { return "The SPC's mesoscale discussions in force, outlined on the map. Hover for the areas affected, what it concerns, when it is valid and its summary; click for the whole discussion. Live only"; }
    int order() const override { return 43; }
    int refreshSeconds() const override { return 180; }
    void refresh(MapHost&) override;
    void paint(QPainter&, MapHost&) override;
    MapHit pick(const QPointF&, MapHost&) const override;
    vector<MapLegendRow> legend() const override;
    string summary() const override;
    bool working() const override { return loading; }

protected:
    void onEnable(MapHost& host) override { refresh(host); }

private:
    bool loading{false};
    string error;
    std::vector<SevereAreas::Area> areas;
    std::shared_ptr<bool> alive{std::make_shared<bool>(true)};
};

#endif  // SEVEREAREASLAYERS_H
