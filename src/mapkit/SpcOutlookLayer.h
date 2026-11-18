// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef SPCOUTLOOKLAYER_H
#define SPCOUTLOOKLAYER_H

#include <memory>
#include <vector>
#include <QColor>
#include <QPointF>
#include <QString>
#include "mapkit/MapLayer.h"

// The Storm Prediction Center's convective outlook, days 1 to 8, as areas on the map, each day shown at its valid time: the time bar steps through the eight days (day 1 is the 12Z to 12Z
// period the outlook of today covers, day 2 the next ...), so the outlook that goes with a model chart, a radar loop or a satellite picture of a time is the one on the bar. Days 1 and 2:
// the categorical risk or the tornado, wind or hail probabilities; day 3: the categorical risk or the total severe probability; days 4 to 8: the 15% and 30% areas. The SPC's own
// GeoJSON of the areas (https://www.spc.noaa.gov/products/outlook/ and .../exper/day4-8/), drawn in the SPC's own colours.
class SpcOutlookLayer : public MapLayer {
public:
    SpcOutlookLayer();
    ~SpcOutlookLayer() override { *alive = false; }
    string id() const override { return "severe/spc-outlook"; }
    string path() const override { return "Severe weather/SPC convective outlook, days 1-8 (each day at its valid time)"; }
    string source() const override { return "NOAA Storm Prediction Center (convective outlooks)"; }
    string tip() const override { return "The SPC's convective outlook as areas: the categorical risk (marginal to high) or the tornado, wind and hail chances. The time bar steps through days 1 to 8, each at the 12Z to 12Z period it is valid for; hover for the area's risk and when the outlook was issued"; }
    int order() const override { return 39; }
    int refreshSeconds() const override { return 1800; }
    void refresh(MapHost&) override;
    void optionChanged(MapHost& host) override { refresh(host); }
    void paint(QPainter&, MapHost&) override;
    MapHit pick(const QPointF&, MapHost&) const override;
    vector<MapLegendRow> legend() const override;
    QWidget * options(QWidget * parent, const std::function<void()>& changed) override;
    string summary() const override;
    bool working() const override { return loading; }
    bool forecast() const override { return true; }   // (day 1 is the first frame: "latest"; the later days are ahead)
    bool timeAware() const override { return true; }
    vector<long> times() const override;
    void showTime(long seconds, MapHost& host) override;
    string timeText() const override;
    void setAsOf(const QDateTime& utc, MapHost& host) override { showTime(static_cast<long>(utc.toSecsSinceEpoch()), host); }   // (the history page and the map's "at this time": the day that holds it)

protected:
    void onEnable(MapHost& host) override { refresh(host); }

private:
    struct Zone {
        QString name;                              // "Enhanced Risk", "5% Tornado Risk"
        QString label;                             // "ENH", "5%"
        QColor fill, stroke;
        std::vector<std::vector<QPointF>> rings;   // (lon, lat): the outer rings and the holes of every polygon of the area
    };
    struct Day {
        int number{0};                             // 1 to 8
        bool read{false};                          // the file came (an outlook with no area is a file too)
        QString product;                           // what the day shows ("Categorical", "Total severe probability", "15% and 30% severe probability")
        QString issued, forecaster;
        std::vector<Zone> zones;
    };
    int shownDay() const;                          // 0 to 7 for the time on the bar, -1 when no day's period holds it
    long dayStart(int index) const { return base + 86400L * index; }
    vector<Day> days;
    long base{0};                                  // the start (12Z) of day 1, seconds since 1970
    long shown{0};                                 // the time the bar asked for; 0: live (day 1)
    int product{0};                                // 0 categorical, 1 tornado, 2 wind, 3 hail
    bool loading{false};
    bool again{false};                             // a choice was made while the files were being read: read them again when done
    string error;
    std::shared_ptr<bool> alive{std::make_shared<bool>(true)};
};

#endif  // SPCOUTLOOKLAYER_H
