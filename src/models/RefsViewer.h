// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef REFSVIEWER_H
#define REFSVIEWER_H

#include <array>
#include <string>
#include <utility>
#include <vector>
#include <QByteArray>
#include "models/RefsPanel.h"
#include "ui/AnimationBar.h"
#include "ui/ComboBox.h"
#include "ui/HBox.h"
#include "ui/VBox.h"
#include "ui/Window.h"

using std::string;
using std::vector;

// REFS ensemble viewer - Stage 0 (see docs/refs-viewer-plan.md): a 2x2 grid
// of `RefsPanel`s, each independently choosing which field to show (Stage 0
// only has the 2 ensemble-mean/pmmn fields `UtilityRefs::fields` carries so
// far - Stage 1 rounds that out to the full Mean/Spread/PMM/Probability
// set, Stage 2/3 add "Member N"/Paintball once member fetch plumbing
// exists), all driven by one shared run/region combo set and a single
// master `AnimationBar` (forecast hour). Proves the 4-panel layout
// mechanics + the `ensprod` render pipeline together, independently of the
// member-comparison work later stages add.
//
// Known Stage 0 simplifications, not yet done: the 4 panels render
// sequentially per hour (not concurrently - the plan's eventual target),
// and there is no linked crosshair/hover read-out yet (both deferred to a
// near-term follow-up, not silently dropped from the plan).
class RefsViewer : public Window {
public:
    explicit RefsViewer(Window *);

private:
    void reload();
    void updateForecastHours();
    void invalidateAnimation();
    void onRangeRequested(int rangeStart, int rangeEnd);
    void renderNextAnimFrame(size_t sweepIndex, int generation);
    void onFrameShown(int localIndex);
    void onScrub(int globalIndex);
    void onSave();
    void resizeEventCustom() override;

    VBox box;
    HBox boxTop;
    ComboBox comboRun;
    ComboBox comboRegion;
    ComboBox comboForecastHour;
    VBox boxGrid;
    HBox rowTop;
    HBox rowBottom;
    RefsPanel panel1;
    RefsPanel panel2;
    RefsPanel panel3;
    RefsPanel panel4;
    AnimationBar animBar;

    vector<std::pair<string, string>> runOptions;
    std::array<QByteArray, 4> renderedBytes;
    string status;
    int animGeneration{0};

    // sweep-in-progress / loaded-frame scratch state, one entry per loaded
    // forecast hour - each entry is 4 panels' worth of bytes (the
    // AnimationBar-as-adapter plumbing described in the plan: the bar only
    // tracks frame *count*/labels via a single representative QByteArray,
    // RefsViewer keeps the real 4-wide payload alongside it)
    vector<int> sweepIndices;
    vector<std::array<QByteArray, 4>> sweepFrames;
    vector<string> frameStatuses;
    std::array<QByteArray, 4> pendingFrame;
    string pendingStatus;
};

#endif  // REFSVIEWER_H
