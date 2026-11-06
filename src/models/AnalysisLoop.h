// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef ANALYSISLOOP_H
#define ANALYSISLOOP_H

#include <array>
#include <atomic>
#include <memory>
#include <set>
#include <string>
#include <vector>
#include <QByteArray>
#include <QComboBox>
#include <QDateTimeEdit>
#include <QGridLayout>
#include <QLabel>
#include <QPointer>
#include <QPushButton>
#include <QSlider>
#include <QTimer>
#include "gfs/GfsChart.h"
#include "models/CompareTile.h"
#include "models/ProductPicker.h"
#include "ui/VBox.h"
#include "ui/Window.h"

// The NWS analysis: a loop of the RTMA / URMA surface analyses from one date-time to another (UTC), one to four charts side by side, in the panes of the Model Viewer (the hover read-out of the
// values, a click marks a point in every pane, a pane's Change... opens the same chart picker, the area the same area picker). Each hour is drawn from the URMA when its file is there (the
// analysis made again with later observations) and from the RTMA for the newest hours the URMA has not come to yet; the charts' titles name the one used. The hours are drawn in the
// background and play as they arrive.
class AnalysisLoop : public Window {
public:
    // region: "" the contiguous United States, or "-AK", "-HI", "-PR", "-GU" (the models RTMA-AK ... of that area)
    // charts: the chart ids (of the RTMA's list) of the panes to begin with, one to four; sector: the area of the Model Viewer; the range is in UTC
    AnalysisLoop(Window * parent, const std::string& sector, const std::vector<std::string>& charts, const QDateTime& from, const QDateTime& to, const std::string& region = "");
    void draw() { build(); }   // as the button does (for a development aid)
    void hoverAt(double fx, double fy) {   // (a development aid: the read-out of every pane at that point of the picture)
        for (auto * tile : tiles) {
            if (tile->isVisible() && tile->hover) {
                tile->hover->show(fx, fy, 12, 12);
            }
        }
    }

private:
    struct Cell {
        QByteArray bytes;
        std::shared_ptr<GfsChart::Probe> probe;   // the values under the pointer; let go of for the hours far from the one on view (many hours would hold gigabytes) and drawn again on the way back
    };
    void closeEventCustom() override;
    void build();
    void stop();
    void renderHour(size_t index, int mine, bool again);
    void showFrame(int index);
    void advance();
    void save();
    void panesChanged();
    void invalidate(const QString& why);   // the charts, the area or the range changed: the hours drawn are of something else
    void applyLayout();
    void pickChart(int pane);
    void pickArea();
    void refreshAreaButton();
    void trimProbes();
    QByteArray composite(size_t index) const;
    std::string chartLabel(const std::string& id) const;
    std::string sector;
    std::string suffix;   // "", "-AK" ... the end of the models' names
    std::vector<std::string> times;   // "yyyyMMddHH" of each hour of the loop
    VBox box;
    QDateTimeEdit * from{}, * to{};
    QComboBox * step{}, * panes{}, * regionBox{};
    QPushButton * areaButton{}, * buildButton{}, * playButton{}, * saveButton{};
    QLabel * status{};
    QWidget * area{};
    QGridLayout * grid{};
    std::array<CompareTile *, 4> tiles{};
    std::vector<std::string> paneCharts;   // the chart of each of the four panes
    QSlider * slider{};
    QTimer timer;
    std::vector<std::string> chartIds;     // the RTMA's charts and their names
    std::vector<std::string> chartNames;
    std::vector<std::vector<Cell>> frames;   // one for each hour of the range: the pane pictures (empty until drawn)
    std::vector<QString> frameNames;
    std::vector<std::string> sources;        // the model each hour came from
    std::vector<std::string> chartsDrawn;    // the charts the frames hold (when the panes are changed the hours are drawn again)
    std::set<size_t> redrawing;
    QPointer<ProductPicker> chartPicker;
    QPointer<ProductPicker> areaPicker;
    int shown{-1};
    int generation{0};
    int done{0}, urma{0}, rtma{0}, failed{0};
    std::string firstError;
    std::shared_ptr<std::atomic<bool>> cancel;
    std::shared_ptr<std::atomic<long long>> urmaEnds;   // the first valid time (seconds since the epoch) the URMA turned out not to have: later ones go straight to the RTMA
    bool closed{false};
};

#endif  // ANALYSISLOOP_H
