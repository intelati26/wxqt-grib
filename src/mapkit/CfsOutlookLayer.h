// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef CFSOUTLOOKLAYER_H
#define CFSOUTLOOKLAYER_H

#include <map>
#include <memory>
#include <QDateTime>
#include <QImage>
#include "cfs/UtilityCfs.h"
#include "mapkit/MapLayer.h"

// The seasonal outlook of NOAA's CFSv2: the newest run's monthly means (the month in progress and the nine after it) of one field, the mean of the four newest runs (member 1 of each cycle: whole nine month forecasts six hours apart) or the newest alone. The time bar
// steps through the months. Of a time of the past the run is that day's 00 UTC one (the open bucket begins 31 October 2018). The values are the model's own, or their departure from the CFS's own normal for the start date (NOAA's calibration climatology of the reforecasts, 1999-2010 or
// 1982-2010; the month in progress has none).
class CfsOutlookLayer : public MapLayer {
public:
    CfsOutlookLayer();
    ~CfsOutlookLayer() override { *alive = false; }
    string id() const override { return "climate/cfs-outlook"; }
    string path() const override { return "Climate/CFSv2 seasonal outlook (monthly means of the newest run)"; }
    string source() const override { return "NOAA/NCEP Climate Forecast System version 2 (CFSv2), open data on AWS"; }
    string tip() const override { return "NOAA's CFSv2 nine-month forecast: the monthly mean of a field for each month ahead, , 1 degree. The time bar steps through the months"; }
    bool underCoast() const override { return true; }
    int order() const override { return 11; }
    int refreshSeconds() const override { return 6 * 3600; }
    void refresh(MapHost&) override;
    void paint(QPainter&, MapHost&) override;
    MapHit pick(const QPointF& pixels, MapHost&) const override;
    vector<MapLegendRow> legend() const override;
    QWidget * options(QWidget * parent, const std::function<void()>& changed) override;
    void optionChanged(MapHost& host) override;
    string summary() const override;
    bool working() const override { return loading; }
    bool timeAware() const override { return true; }
    vector<long> times() const override;
    void prepareTimes(MapHost& host) override { load(host); }
    void showTime(long seconds, MapHost&) override;
    string timeText() const override;
    void setAsOf(const QDateTime& utc, MapHost&) override;

protected:
    void onEnable(MapHost& host) override { refresh(host); }

private:
    struct Frame {
        GfsGrid::Grid grid;
        QImage image;
    };
    void load(MapHost&);
    QImage colored(const GfsGrid::Grid&) const;
    double inUnits(double value) const;       // a value of the file in the program's units (degrees F and inches, or C and mm; a departure in F is x1.8 with no 32)
    QString unitsShown() const;
    std::vector<std::pair<double, QColor>> stops() const;   // the colours of what is shown: value stops
    long middleOf(const string& yyyymm) const;     // the 15th of the month, UTC
    string monthShown() const;                     // the month on view ("202611")
    string variableKey{"t2m"};
    int memberChoice{0};          // 0 the mean of the four newest runs, 1 the newest alone
    int mode{0};                  // 0 the values, 1 the departure from the CFS normal, 2 the departure in standard deviations
    int baseline{1999};           // the years of the normal: 1999-2010 or 1982-2010
    int opacity{85};
    long shown{0};
    bool loading{false};
    bool changeRequested{false};
    QDateTime asOf;
    UtilityCfs::Run run;
    bool haveRun{false};
    std::map<string, Frame> frames;   // yyyymm
    string error;
    std::shared_ptr<bool> alive{std::make_shared<bool>(true)};
};

#endif  // CFSOUTLOOKLAYER_H
