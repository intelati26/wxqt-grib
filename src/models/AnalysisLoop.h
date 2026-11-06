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
#include <QByteArray>
#include <QComboBox>
#include <QDateTimeEdit>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QTimer>
#include "ui/VBox.h"
#include "ui/Window.h"

// The NWS analysis: a loop of the RTMA / URMA surface analyses from one date-time to another (UTC), one to four charts side by side. Each hour is drawn from the URMA when its file is there (the analysis made again
// with later observations) and from the RTMA for the newest hours the URMA has not come to yet; the charts' titles name the one used. The hours are drawn in the background and play as they arrive.
class AnalysisLoop : public Window {
public:
    // region: "" the contiguous United States, or "-AK", "-HI", "-PR" (the models RTMA-AK ... of that area)
    // charts: the chart ids (of the RTMA's list) of the panes to begin with, one to four; sector: the area of the Model Viewer; the range is in UTC
    AnalysisLoop(Window * parent, const std::string& sector, const std::vector<std::string>& charts, const QDateTime& from, const QDateTime& to, const std::string& region = "");
    void draw() { build(); }   // as the button does (for a development aid)

private:
    void closeEventCustom() override;
    void resizeEventCustom() override;
    void build();
    void stop();
    void showFrame(int index);
    void advance();
    void save();
    void panesChanged();
    void redraw();
    std::string sector;
    VBox box;
    QDateTimeEdit * from{}, * to{};
    QComboBox * step{}, * panes{}, * regionBox{};
    std::vector<QComboBox *> chartBoxes;
    QPushButton * buildButton{}, * playButton{}, * saveButton{};
    QLabel * status{}, * view{};
    QSlider * slider{};
    QTimer timer;
    std::vector<std::string> chartIds;   // the ids of the RTMA's charts, in the order of the boxes' items
    std::vector<QByteArray> frames;      // one for each hour of the range; empty until drawn
    std::vector<QString> frameNames;
    std::vector<std::string> sources;    // "URMA", "RTMA" or "" for each
    int shown{-1};
    int generation{0};
    int done{0}, urma{0}, rtma{0}, failed{0};
    std::string firstError;
    std::shared_ptr<std::atomic<bool>> cancel;
    std::shared_ptr<std::atomic<long long>> urmaEnds;   // the first valid time (seconds since the epoch) the URMA turned out not to have: later ones go straight to the RTMA
    bool closed{false};
};

#endif  // ANALYSISLOOP_H
