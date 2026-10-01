// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef GRIBVIEWER_H
#define GRIBVIEWER_H

#include <array>
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
#include "models/SoundingPick.h"
#include "ui/Button.h"
#include "ui/ComboBox.h"
#include "ui/HBox.h"
#include "ui/Photo.h"
#include "ui/VBox.h"
#include "ui/Window.h"
#include "ui/ZoomImage.h"

using std::string;
using std::vector;

// Basic viewer for operational RRFS GRIB2 fields, rendered by GDAL. The
// forecast-hour axis is driven by a shared AnimationBar (scrub / play / export
// range / save); each frame is a full GDAL render, so a play/save request only
// ever triggers a bounded, progress-reporting render sweep (see maxAnimFrames
// in GribViewer.cpp), not a render of every forecast hour up front.
class GribViewer : public Window {
public:
    explicit GribViewer(Window *);

private:
    void reload();
    void showMaxOfRange();
    void showMaxOfHours(const std::vector<std::string>& hours);
    void showDay1Max();
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
    void onMapClicked(double fx, double fy);
    const SampleGrid * currentGrid();
    void refreshHover();
    void invalidateAnimation();
    void loadViews();                // from the saved preference into comboViews
    void storeViews() const;
    void saveView();                 // names the current field / region / compare and keeps it
    void applyView(int comboIndex);
    void deleteView();
    int compareHours() const;   // 0 = plain field, else the comparison run's age in hours
    QByteArray buildLegend(int fieldIndex, double clipLo, double clipHi) const;
    void resizeEventCustom() override;

    VBox box;
    HBox boxTop;
    HBox boxImage;
    ZoomImage image;
    Photo legend;
    ComboBox comboRun;
    ComboBox comboField;
    ComboBox comboRegion;
    ComboBox comboForecastHour;
    ComboBox comboCompare;   // Off / change since the run 6, 12 or 24 hours earlier
    ComboBox comboViews;     // saved field / region / compare combinations
    BackForward backForward;
    Button buttonMax;
    Button buttonDay1;
    Button buttonSaveView;
    Button buttonDeleteView;
    SoundingPick soundingPick;
    vector<std::array<string, 4>> views;   // name, field, region, compare label
    std::vector<std::string> day1Pending;   // hours computed off-thread for the Day 1 button
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
    SampleGridCache gridCache;   // parsed hover grids, keyed by sidecar path
    double lastHoverFx{-1.0};
    double lastHoverFy{-1.0};
};

#endif  // GRIBVIEWER_H
