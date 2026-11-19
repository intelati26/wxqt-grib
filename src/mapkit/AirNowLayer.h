// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef AIRNOWLAYER_H
#define AIRNOWLAYER_H

#include <map>
#include <memory>
#include <vector>
#include <QDate>
#include "gfs/AirNowLoader.h"
#include "mapkit/MapLayer.h"

// EPA's AirNow air quality of the reporting areas (about 1,300 cities and their surroundings) as dots in the EPA's own colors, Good to Hazardous: today's observations and forecast, and the
// forecasts of the days ahead (to about five). The time bar steps through the days; hover for the area's index where the file has it.
class AirNowLayer : public MapLayer {
public:
    ~AirNowLayer() override { *alive = false; }
    string id() const override { return "air/airnow"; }
    string path() const override { return "Air quality/AirNow reporting areas (today and the forecast days)"; }
    string source() const override { return "EPA AirNow (reporting areas, forecasts and observations)"; }
    string tip() const override { return "The air quality of the cities EPA's AirNow reports, as dots in its colors (Good green to Hazardous maroon): today's worst of the observations and the forecast, and the forecast of each day ahead on the time bar"; }
    int order() const override { return 48; }
    int refreshSeconds() const override { return 1800; }
    void refresh(MapHost&) override;
    void paint(QPainter&, MapHost&) override;
    MapHit pick(const QPointF&, MapHost&) const override;
    vector<MapLegendRow> legend() const override;
    string summary() const override;
    bool working() const override { return loading; }
    bool forecast() const override { return true; }
    bool timeAware() const override { return true; }
    vector<long> times() const override;
    void showTime(long seconds, MapHost&) override;
    string timeText() const override;
    void setAsOf(const QDateTime& utc, MapHost& host) override;

protected:
    void onEnable(MapHost& host) override { refresh(host); }

private:
    QDate shownDate() const;                       // the day on view: the one the bar asked for, else the first with data
    std::map<QDate, std::vector<AirNow::Point>> days;
    QDate wanted;                                  // the day the bar or the history page asked for (invalid: today)
    bool loading{false};
    std::shared_ptr<bool> alive{std::make_shared<bool>(true)};
};

#endif  // AIRNOWLAYER_H
