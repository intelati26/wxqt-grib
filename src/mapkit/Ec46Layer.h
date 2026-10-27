// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef EC46LAYER_H
#define EC46LAYER_H

#include <map>
#include <memory>
#include <QDate>
#include <QImage>
#include "gfs/GfsGrid.h"
#include "mapkit/BandLines.h"
#include "mapkit/MapLayer.h"

// The ECMWF's extended range forecast (EC46, 46 days: its weeks) or seasonal forecast (SEAS5, 7 months: its months): the mean of its ensemble for each period ahead, as a departure from the ECMWF's own climate or as the values, of the 2 m temperature, the
// precipitation or the sea level pressure. From Open-Meteo's seasonal API (no key): the points of a grid over the view (at most 450, so a map is some 450 of its calls, kept six hours); "Load the
// view" makes the grid again for what is on the screen. The time bar steps through the weeks.
class Ec46Layer : public MapLayer {
public:
    Ec46Layer();
    ~Ec46Layer() override { *alive = false; }
    string id() const override { return "climate/ec46-weeks"; }
    string path() const override { return "Climate/ECMWF extended range and seasonal (EC46 weeks, SEAS5 months)"; }
    string source() const override { return "ECMWF EC46 ensemble mean, data by Open-Meteo.com"; }
    string tip() const override { return "The ECMWF's 46 day forecast: the ensemble mean of each week ahead as a departure from the ECMWF's climate (or the values) for the area on the screen. The time bar steps through the weeks (or, for the SEAS5, the months)"; }
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
    void prepareTimes(MapHost&) override {}
    void showTime(long seconds, MapHost&) override;
    string timeText() const override;

protected:
    void onEnable(MapHost& host) override { refresh(host); }

private:
    struct Frame {
        GfsGrid::Grid grid;
        QImage image;
        std::vector<BandLines::Contour> lines;
    };
    void load(MapHost&);
    QImage colored(const GfsGrid::Grid&) const;
    double inUnits(double value) const;
    QString unitsShown() const;
    std::vector<std::pair<double, QColor>> stops() const;
    long middleOf(const QDate& periodStart) const;   // the middle of the week (or month), UTC
    QDate weekShown() const;                         // the period on view's first day
    string variableKey{"temperature_2m_mean"};
    int mode{0};                  // 0 the departure from the ECMWF's climate, 1 the values
    int span{0};                  // 0 the weeks of the EC46 (46 days), 1 the months of the SEAS5 (7 months)
    bool monthlyFrames{false};    // the frames on hand are months
    int opacity{85};
    bool numbers{true};
    long shown{0};
    bool loading{false};
    bool changeRequested{false};
    double west{0}, south{0}, east{0}, north{0}, step{0};   // the grid of the frames
    int points{0};
    std::map<QDate, Frame> frames;   // by the period's first day
    string error;
    std::shared_ptr<bool> alive{std::make_shared<bool>(true)};
};

#endif  // EC46LAYER_H
