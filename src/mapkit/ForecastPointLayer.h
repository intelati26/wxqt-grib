// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef FORECASTPOINTLAYER_H
#define FORECASTPOINTLAYER_H

#include <memory>
#include "mapkit/MapLayer.h"
#include "misc/UtilityForecastPoint.h"

// The saved locations on the master map, each with the day's high and low and the chance of rain (the NWS forecast points page's idea of a map of points); a click opens the
// whole page of that point.
class ForecastPointLayer : public MapLayer {
public:
    string id() const override { return "forecast/points"; }
    string path() const override { return "Forecast/Forecast points (your saved locations)"; }
    string source() const override { return "NWS gridded forecasts (api.weather.gov)"; }
    string tip() const override { return "Your saved locations with today's high and low and the chance of rain; click one for its week, outlooks and hourly graphs"; }
    int order() const override { return 70; }
    int refreshSeconds() const override { return 1800; }
    void refresh(MapHost&) override;
    void paint(QPainter&, MapHost&) override;
    MapHit pick(const QPointF&, MapHost&) const override;
    vector<MapLegendRow> legend() const override;
    string summary() const override;

protected:
    void onEnable(MapHost& host) override { refresh(host); }

private:
    struct Entry {
        std::string name;
        double lat, lon;
        std::shared_ptr<UtilityForecastPoint::Data> data;
    };
    bool loading{false};
    std::shared_ptr<std::vector<Entry>> points;
};

#endif  // FORECASTPOINTLAYER_H
