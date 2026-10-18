// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef COMBOCHARTLAYER_H
#define COMBOCHARTLAYER_H

#include <memory>
#include <vector>
#include "mapkit/MapLayer.h"
#include "mapkit/ModelChartLayer.h"

// The finest picture there is, anywhere: one chart (500 mb heights and wind, the 2 m temperature ...) drawn from every model that has it at once, the coarse global models first and the fine
// regional ones over them, so that the 3 km models show where they reach (the United States, Canada, Germany ...) and the world's own model shows everywhere else. All the models are
// shown for the same valid time; hovering reads the finest model that covers the place, and says which.
class ComboChartLayer : public MapLayer {
public:
    ComboChartLayer();
    string id() const override { return "model/combo"; }
    string path() const override { return "Models/Best resolution combo (every model that has the chart, the finest on top)"; }
    string source() const override { return "NOAA/NCEP, ECMWF, DWD, ECCC model data"; }
    string tip() const override { return "One chart from every model that has it at once, the coarse global models underneath and the fine regional ones (3 km and under) over them: the finest picture anywhere. Hover reads the finest model at the pointer"; }
    bool underCoast() const override { return true; }
    int order() const override { return 13; }
    int refreshSeconds() const override { return 1800; }
    void refresh(MapHost&) override;
    void paint(QPainter&, MapHost&) override;
    MapHit pick(const QPointF& pixels, MapHost&) const override;
    vector<MapLegendRow> legend() const override;
    QWidget * options(QWidget * parent, const std::function<void()>& changed) override;
    void optionChanged(MapHost& host) override;
    string summary() const override;
    string notice() const override;
    bool timeAware() const override { return true; }
    vector<long> times() const override;
    void showTime(long seconds, MapHost&) override;
    string timeText() const override;
    void setAsOf(const QDateTime& utc, MapHost&) override;
    void setRange(const QDateTime& from, const QDateTime&, MapHost& host) override { setAsOf(from, host); }
    void prepareTimes(MapHost&) override;

    // the models of the table with their resolution in km (coarse to fine), for the chart asked for; empty when none has it
    // the grid spacing of a model in km (0 when it is not in the table)
    static double kmOf(const string& model);
    // for the globe: the chart, the models in the stack, the valid time on view
    const string& chart() const { return param; }
    vector<string> modelNames() const { return names; }
    QDateTime validTime() const { return parts.empty() ? QDateTime{} : parts.back()->validTime(); }
    static vector<std::pair<string, double>> modelsWith(const string& param, const QDate& day);

protected:
    void onEnable(MapHost& host) override { build(host); }

private:
    void build(MapHost&);
    QDate dayNow() const;
    bool isOn(const string& model) const;       // in the stack: in by default and not left out, or switched on by the person
    string param{"500_wnd_ht"};
    std::vector<string> excluded;                              // models the person has left out
    std::vector<string> included_;                             // models that are off unless asked for (the AI ones, the ensembles) that the person switched on
    std::vector<std::unique_ptr<ModelChartLayer>> parts;       // coarse first
    std::vector<string> names;                                 // the model of each part
    std::vector<double> kilometers;
    long shown{0};
    QDateTime asOf;
};

#endif  // COMBOCHARTLAYER_H
