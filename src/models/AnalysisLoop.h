// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef ANALYSISLOOP_H
#define ANALYSISLOOP_H

#include <atomic>
#include <memory>
#include <string>
#include <vector>
#include <QComboBox>
#include <QDateTimeEdit>
#include "models/LoopWindow.h"

// The NWS analysis: a loop of the RTMA / URMA surface analyses from one date-time to another (UTC), one to four charts side by side (the panes, pickers and hover values of LoopWindow, the
// Model Viewer's). Each hour is drawn from the URMA when its file is there (the analysis made again with later observations) and from the RTMA for the newest hours the URMA has not come to yet;
// the charts' titles name the one used. The hours are drawn in the background and play as they arrive.
class AnalysisLoop : public LoopWindow {
public:
    // region: "" the contiguous United States, or "-AK", "-HI", "-PR", "-GU" (the models RTMA-AK ... of that area)
    // charts: the chart ids (of the RTMA's list) of the panes to begin with, one to four; sector: the area of the Model Viewer; the range is in UTC
    AnalysisLoop(Window * parent, const std::string& sector, const std::vector<std::string>& charts, const QDateTime& from, const QDateTime& to, const std::string& region = "");
    void draw() { build(); }   // as the button does (for a development aid)

protected:
    std::string pickerModel() const override { return "RTMA" + suffix; }
    std::vector<std::string> areaIds() const override;
    void build() override;
    void renderFrame(size_t index, int mine, bool again) override;
    QString captionOf(size_t frame, size_t pane) const override;
    std::string titleOf(size_t frame) const override;
    std::string saveName() const override;

private:
    std::string suffix;   // "", "-AK" ... the end of the models' names
    std::vector<std::string> times;     // "yyyyMMddHH" of each hour of the loop
    std::vector<std::string> sources;   // the model each hour came from
    QDateTimeEdit * from{}, * to{};
    QComboBox * step{}, * regionBox{};
    int done{0}, urma{0}, rtma{0}, failed{0};
    std::string firstError;
    std::shared_ptr<std::atomic<long long>> urmaEnds;   // the first valid time (seconds since the epoch) the URMA turned out not to have: later ones go straight to the RTMA
};

#endif  // ANALYSISLOOP_H
