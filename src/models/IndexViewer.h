// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef INDEXVIEWER_H
#define INDEXVIEWER_H

#include <map>
#include <string>
#include <utility>
#include <vector>
#include <QByteArray>
#include <QLabel>
#include <QString>
#include "objects/SampleGrid.h"
#include "ui/AnimationBar.h"
#include "ui/BackForward.h"
#include "ui/ComboBox.h"
#include "ui/HBox.h"
#include "ui/Photo.h"
#include "ui/VBox.h"
#include "ui/Window.h"
#include "ui/ZoomImage.h"

using std::string;
using std::vector;

// Viewer for derived severe-weather "Parametric" indices (SHIP today, SIGTOR/
// STP appended later - see docs/derived-severe-indices-plan.md). Deliberately
// mirrors GribViewer's construction almost line-for-line:
// UtilitySevereIndices::render() has the same signature shape as
// UtilityGrib::render() (index/region/forecastHour/runId, all locally
// resolvable, no server-directory-scraping the way SpcPostViewer needs), so
// the same synchronous-list + AnimationBar-driven-sweep pattern applies
// unchanged. The one real difference: SHIP's inputs only exist at RRFS's
// synoptic cycles (00/06/12/18z), so comboRun is populated from
// UtilitySevereIndices::runOptions() (already filtered to those cycles)
// rather than UtilityGrib::runOptions()'s full hourly list.
class IndexViewer : public Window {
public:
    explicit IndexViewer(Window *);

private:
    void reload();
    void moveBack();
    void moveForward();
    void updateForecastHours();
    void openFullImage();
    void onRangeRequested(int rangeStart, int rangeEnd);
    void renderNextAnimFrame(size_t sweepIndex, int generation);
    void onFrameShown(int localIndex);
    void onScrub(int globalIndex);
    void onSave();
    void onHover(double fx, double fy);
    void onHoverEnded();
    void refreshHover();
    void invalidateAnimation();
    QByteArray buildLegend(int indexIndex, double clipLo, double clipHi) const;
    void resizeEventCustom() override;

    VBox box;
    HBox boxTop;
    HBox boxImage;
    ZoomImage image;
    Photo legend;
    ComboBox comboRun;
    ComboBox comboIndex;
    ComboBox comboRegion;
    ComboBox comboForecastHour;
    BackForward backForward;
    AnimationBar animBar;
    std::vector<std::pair<string, string>> runOptions;
    string status;
    string pngPath;
    QByteArray renderedBytes;
    double dataMin{0.0};
    double dataMax{0.0};

    // metadata parallel to whatever frames are currently loaded in `animBar`
    vector<string> frameStatuses;
    vector<std::pair<double, double>> frameRanges;
    int animGeneration{0};   // bumped to abandon an in-flight render sweep

    // sweep-in-progress scratch state (indices are global, into comboForecastHour)
    vector<int> sweepIndices;
    vector<QByteArray> sweepFrames;
    vector<string> sweepStatuses;
    vector<std::pair<double, double>> sweepRanges;
    vector<string> sweepGridPaths;
    QByteArray pendingFrame;   // worker -> callback handoff
    string pendingStatus;
    std::pair<double, double> pendingRange{0.0, 0.0};
    string pendingGridPath;

    // hover read-out
    vector<string> frameGridPaths;       // parallel to the frames loaded in animBar
    QLabel * hoverLabel{};
    string sampleGridPath;               // .grid sidecar backing the visible frame
    std::map<string, SampleGrid> gridCache;   // parsed grids, keyed by sidecar path
    double lastHoverFx{-1.0};
    double lastHoverFy{-1.0};
};

#endif  // INDEXVIEWER_H
