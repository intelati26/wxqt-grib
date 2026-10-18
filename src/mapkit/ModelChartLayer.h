// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef MODELCHARTLAYER_H
#define MODELCHARTLAYER_H

#include <functional>
#include <memory>
#include <QImage>
#include "gfs/GfsChart.h"
#include "gfs/GfsRender.h"
#include "mapkit/MapLayer.h"

// Any chart of the Model Viewer (any model, any of its charts: the fill, the lines, the barbs) as a layer of the scrollable master map: the chart is drawn for the part of the world in view (a
// little more, and finer as the map is zoomed) without its header, legend or frame, and moves with the map until the view leaves it, when it is drawn again. The time bar steps through the
// model's forecast hours. "Export view" can save the same chart in the Model Viewer's own picture, with its title, run and valid times and legend (titled()).
class ModelChartLayer : public MapLayer {
public:
    ModelChartLayer();
    ~ModelChartLayer() override { *alive = false; }
    string id() const override { return "model/chart"; }
    string path() const override { return "Models/Model chart (any model and chart of the Model Viewer)"; }
    string source() const override { return source_; }
    string tip() const override { return "A chart of the Model Viewer (any model, any chart) over the map: scroll and zoom it like any layer, step through the forecast hours on the time bar, hover for the values. Export view saves it as the Model Viewer's picture, with its title and legend"; }
    bool underCoast() const override { return true; }
    int order() const override { return 12; }
    int refreshSeconds() const override { return 1800; }
    void refresh(MapHost&) override;
    void paint(QPainter&, MapHost&) override;
    MapHit pick(const QPointF& pixels, MapHost&) const override;
    vector<MapLegendRow> legend() const override;
    QWidget * options(QWidget * parent, const std::function<void()>& changed) override;
    void optionChanged(MapHost& host) override;
    string summary() const override;
    bool timeAware() const override { return true; }
    vector<long> times() const override;
    void showTime(long seconds, MapHost&) override;
    string timeText() const override;
    void setAsOf(const QDateTime& utc, MapHost&) override;

    const string& modelName() const { return model; }

    // The chart in the Model Viewer's own picture for the part of the world in view (header with the model, chart, run and valid times, the legend, the credit): false with the reason.
    bool titled(MapHost&, QImage& out, string& error) const;

protected:
    void onEnable(MapHost& host) override { refresh(host); }

private:
    struct Rendered {
        QImage image;
        double west{0}, south{0}, east{0}, north{0};
        GfsChart::Probe probe;
        string key;             // model|chart|run|hour
        int widthPixels{0};
    };
    string runOf() const;                      // "yyyyMMddHH": the run on view
    int hourShown() const;
    bool visibleBox(MapHost&, double& west, double& south, double& east, double& north) const;
    void render(MapHost&);
    string keyFor(int hour) const;
    string variantKey() const;
    GfsRender::Variant variantNow(int hour) const;
    string model{"GFS"};
    string param{"500_wnd_ht"};
    std::vector<string> overlays;              // lines and barbs ticked onto the chart (ProductPicker)
    string member;                             // one ensemble member instead of the mean
    bool sigma{false};                         // in standard deviations from the normal
    bool maxDay{false};                        // the largest of the fill over the 24 hours ending at the hour shown
    string pinnedRun;                          // "yyyyMMddHH" of a run picked from the list; empty: the newest, or the time bar's
    std::function<void()> optionsRefresh;      // redraws the option panel's boxes (while it is open)
    string newestRun;                          // the newest run found
    int changeBack{0};                         // hours: the change since the run that much older (0: the plain chart)
    string run;                                // the run found, "yyyyMMddHH"
    QDateTime asOf;
    std::vector<int> hours;                    // the forecast hours the run has
    long shown{0};                             // the valid time on view (seconds), 0: the first hour
    int opacity{90};
    bool busy{false};
    bool finding{false};
    string error;
    string source_{"NOAA/NCEP model data"};
    std::shared_ptr<Rendered> current;
    std::shared_ptr<Rendered> previous;        // the chart before this hour or view, drawn until the new one is in
    GfsRender::Session session;
    std::shared_ptr<bool> alive{std::make_shared<bool>(true)};
};

#endif  // MODELCHARTLAYER_H
