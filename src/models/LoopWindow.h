// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef LOOPWINDOW_H
#define LOOPWINDOW_H

#include <atomic>
#include <memory>
#include <set>
#include <string>
#include <vector>
#include <QByteArray>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPointer>
#include <QPushButton>
#include <QSlider>
#include <QTimer>
#include "gfs/GfsChart.h"
#include "models/PaneGrid.h"
#include "models/ProductPicker.h"
#include "ui/VBox.h"
#include "ui/Window.h"

// What the screens that loop one to four model charts through time have in common (the NWS analysis window, the 15 minute window): the panes (PaneGrid: hover values, markers, the chart picker of
// the Model Viewer behind each Change..., the same area picker), the slider, play and save, the hours drawn in the background and held as pictures with their values (let go of far from the one
// on view and drawn again on the way back), the draw that is stopped when something it was drawn from changes. A window made on this only says what its time axis is and how one frame is drawn.
class LoopWindow : public Window {
public:
    struct Cell {
        QByteArray bytes;
        std::shared_ptr<GfsChart::Probe> probe;   // the values under the pointer
    };
    void hoverAt(double fx, double fy) { grid->hoverAt(fx, fy); }   // (a development aid)
    int shownIndex() const { return shown; }   // (a development aid)
    QSize firstPictureSize() const {   // (a development aid: the size of the first chart drawn)
        for (const auto& cells : frames) {
            if (!cells.empty()) {
                return QImage::fromData(cells.front().bytes).size();
            }
        }
        return {};
    }

protected:
    LoopWindow(Window * parent, const std::string& sector, const std::vector<std::string>& charts);
    ~LoopWindow() override = default;

    // ---- what a window made on this says
    virtual std::string pickerModel() const = 0;                          // the model whose charts, favorites and areas the pickers offer
    virtual std::vector<std::string> areaIds() const = 0;                 // the areas that can be picked
    virtual void build() = 0;                                             // the Draw button: begin the frames, then draw each
    virtual void renderFrame(size_t index, int mine, bool again) = 0;     // draw the panes of one frame (again: only its values wanted back; end with storeAgain)
    virtual QString captionOf(size_t frame, size_t pane) const = 0;       // what is written over a pane
    virtual std::string titleOf(size_t frame) const = 0;                  // the window's title with that frame on view
    virtual std::string saveName() const = 0;
    virtual int delayMs() const { return 450; }

    // ---- what it can use
    void beginFrames(size_t count);                                       // a new draw: nothing drawn yet, the slider and Save ready
    void storeFrame(size_t index, std::vector<Cell>&& cells, const QString& name);   // a frame drawn (the first one is shown, or the last one when showNewestFirst)
    void frameFailed(size_t index);                                       // a frame that could not be drawn (the newest one failing lets the next frame drawn be shown)
    void storeAgain(size_t index, std::vector<Cell>&& cells);             // the values of a frame drawn again
    void invalidate(const QString& why);                                  // the charts, the area or the range changed: what was drawn is of something else
    void stop();                                                          // the frames not yet started are left
    int paneCount() const { return grid->count(); }
    std::vector<std::string> activeCharts() const;                        // the charts of the panes in use
    std::string chartLabel(const std::string& id) const;
    void refreshAreaButton();
    void setCharts(const std::vector<std::string>& ids, const std::vector<std::string>& names);   // the charts the pickers offer
    void closeEventCustom() override;

    // the top row (the window adds its controls to it, with these) and the rest
    VBox box;
    QHBoxLayout * topLayout{};
    QPushButton * areaButton{};
    QPushButton * buildButton{};
    QComboBox * panes{};
    QComboBox * resolution{};   // the size the charts are drawn at (the Model Viewer's choice)
    QLabel * status{};
    PaneGrid * grid{};
    std::string sector;
    std::vector<std::string> paneCharts;      // the chart of each of the four panes
    std::vector<std::string> chartIds;        // the charts the pickers offer
    std::vector<std::string> chartNames;
    std::vector<std::vector<Cell>> frames;
    std::vector<QString> frameNames;
    int shown{-1};
    int generation{0};
    bool closed{false};
    bool showNewestFirst{false};   // the loop is of times up to now: the last frame is shown as soon as it is drawn and the others come after it (newest first), not whichever frame is done first
    std::shared_ptr<std::atomic<bool>> cancel;

private:
    void showFrame(int index);
    void advance();
    void save();
    void pickChart(int pane);
    void pickArea();
    void trimProbes();
    QSlider * slider{};
    QPushButton * playButton{}, * saveButton{};
    QTimer timer;
    std::set<size_t> redrawing;
    QPointer<ProductPicker> chartPicker;
    QPointer<ProductPicker> areaPicker;
};

#endif  // LOOPWINDOW_H
