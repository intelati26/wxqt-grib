// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef SPCPOSTVIEWER_H
#define SPCPOSTVIEWER_H

#include <map>
#include <string>
#include <utility>
#include <vector>
#include <QByteArray>
#include <QCheckBox>
#include <QLabel>
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

// Viewer for the NOMADS SPC Post products: HREFCT thunderstorm probability,
// calibrated lightning density, HREF/GEFS calibrated severe (tor/wind/hail)
// probability, and GEFS-MLP severe probability. Mirrors GribViewer's layout
// and animation-sweep pattern (see UtilitySpcPost.cpp for the render
// pipeline); unlike GribViewer, the run/forecast-hour lists are discovered by
// querying the server's directory listing rather than derived from the clock,
// so populating them is itself an asynchronous step.
class SpcPostViewer : public Window {
public:
    explicit SpcPostViewer(Window *);

private:
    void refreshRunOptions();
    void refreshForecastHours();
    void reload();
    void moveBack();
    void moveForward();
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
    static QByteArray buildLegend();
    void resizeEventCustom() override;
    int boundaryFlags() const;   // OR of whichever boundaryChecks are currently checked

    VBox box;
    HBox boxTop;
    HBox boxImage;
    ZoomImage image;
    Photo legend;
    ComboBox comboProduct;
    ComboBox comboDomain;
    // independent boundary-layer toggles (not mutually exclusive - see
    // UtilitySpcPost::boundaryOptions()/BoundaryState etc.), each paired
    // with its bit flag
    vector<std::pair<QCheckBox *, int>> boundaryChecks;
    ComboBox comboBackground;
    ComboBox comboRun;
    ComboBox comboForecastHour;
    BackForward backForward;
    AnimationBar animBar;
    std::vector<std::pair<string, string>> runOptionsList;
    string status;
    string pngPath;
    QByteArray renderedBytes;
    int animGeneration{0};   // bumped to abandon an in-flight render sweep / async fetch

    // metadata parallel to whatever frames are currently loaded in `animBar`
    vector<string> frameStatuses;

    // sweep-in-progress scratch state (indices are global, into comboForecastHour)
    vector<int> sweepIndices;
    vector<QByteArray> sweepFrames;
    vector<string> sweepStatuses;
    vector<string> sweepGridPaths;
    QByteArray pendingFrame;   // worker -> callback handoff
    string pendingStatus;
    string pendingGridPath;
    vector<string> pendingHours;   // worker -> callback handoff (forecast-hour listing)

    // hover read-out
    vector<string> frameGridPaths;       // parallel to the frames loaded in animBar
    QLabel * hoverLabel{};
    string sampleGridPath;               // .grid sidecar backing the visible frame
    std::map<string, SampleGrid> gridCache;
    double lastHoverFx{-1.0};
    double lastHoverFy{-1.0};
};

#endif  // SPCPOSTVIEWER_H
