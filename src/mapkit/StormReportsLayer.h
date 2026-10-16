// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef STORMREPORTSLAYER_H
#define STORMREPORTSLAYER_H

#include <QDateTime>
#include "mapkit/MapLayer.h"

// The SPC storm reports of a convective day (12 UTC to 12 UTC) as dots: tornadoes red, wind blue, hail green. Of a time of the past the day is that time's; the time bar steps by the hour
// and shows the reports that had come in by then.
class StormReportsLayer : public MapLayer {
public:
    string id() const override { return "severe/storm-reports"; }
    string path() const override { return "Severe weather/Storm reports of the day (SPC)"; }
    string source() const override { return "NOAA Storm Prediction Center storm reports"; }
    string tip() const override { return "The preliminary storm reports of the day (tornado, wind and hail), as the SPC lists them; of a time of the past the day is that one, and the time bar adds the reports hour by hour"; }
    int order() const override { return 49; }
    int refreshSeconds() const override { return 600; }
    void paint(QPainter&, MapHost&) override;
    MapHit pick(const QPointF&, MapHost&) const override;
    vector<MapLegendRow> legend() const override;
    string summary() const override;
    bool timeAware() const override { return true; }
    vector<long> times() const override;
    void showTime(long seconds, MapHost&) override;
    string timeText() const override;
    void setAsOf(const QDateTime& utc, MapHost&) override;
    void refresh(MapHost&) override;

protected:
    void onEnable(MapHost& host) override { refresh(host); }

private:
    struct Report {
        int kind;            // 0 tornado, 1 wind, 2 hail
        long seconds;        // UTC
        double lat, lon;
        QString text;
    };
    QDate day() const;       // the convective day on view
    long start() const;      // its 12 UTC
    QDateTime asOf;
    long shown{0};
    QDate loadedDay;
    bool loading{false};
    string error;
    std::vector<Report> reports;
};

#endif  // STORMREPORTSLAYER_H
