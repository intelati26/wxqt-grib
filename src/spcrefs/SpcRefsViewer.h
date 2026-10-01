// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef SPCREFSVIEWER_H
#define SPCREFSVIEWER_H

#include <memory>
#include <string>
#include <vector>
#include <QByteArray>
#include <QDateTime>
#include "spcrefs/UtilitySpcRefs.h"
#include "ui/AnimationBar.h"
#include "ui/BackForward.h"
#include "ui/ComboBox.h"
#include "ui/HBox.h"
#include "ui/Text.h"
#include "ui/VBox.h"
#include "ui/Window.h"
#include "ui/ZoomImage.h"

using std::string;
using std::vector;

// SPC's REFS ensemble products (the ones on https://www.spc.noaa.gov/exper/refs/viewer) read straight from SPC's open
// Zarr data: ready-made means, maxima, probabilities, paintball member masks and updraft-helicity swaths on the 3 km CONUS
// grid. Pick a product, a cycle and a forecast time; Play loops the times and Save exports the picture (or the loop) with
// the product, run and valid time in a border.
class SpcRefsViewer : public Window {
public:
    explicit SpcRefsViewer(Window * parent);

private:
    struct FrameInfo {
        QDateTime valid;
        int timeIndex{0};
        int hour{0};
    };

    void openCycle();
    void rebuildProducts(const string& selectId);
    void productChanged();
    void loadTimes();
    void showTime();
    void regionChanged();
    void moveBack();
    void moveForward();
    void onHover(double fx, double fy);
    void onRangeRequested(int rangeStart, int rangeEnd);
    void renderNextFrame(size_t sweepIndex, int generation);
    void onFrameShown(int localIndex);
    void onScrub(int globalIndex);
    void onSave();
    void closeEventCustom() override;
    void updateStatus();
    void say(const string& text);
    QByteArray withBorder(const QByteArray& png, const FrameInfo& info) const;
    const UtilitySpcRefs::Product& product() const;
    int memberIndex() const;

    VBox box;
    HBox rowTop;
    ComboBox comboProduct;
    ComboBox comboCycle;
    ComboBox comboTime;
    ComboBox comboMember;
    ComboBox comboRegion;   // the whole CONUS grid or one of the standard mesoscale sectors
    BackForward backForward;
    Text textStatus;
    AnimationBar animBar;
    ZoomImage image;

    std::shared_ptr<ZarrStore> store;
    vector<QDateTime> cycles;
    vector<UtilitySpcRefs::Product> products;
    vector<string> memberNames;        // raw names from the store, in bit order
    vector<QDateTime> validTimes;      // the chosen product's time coordinate
    QDateTime initTime;
    UtilitySpcRefs::View view;         // the part of the grid on screen
    vector<float> values;              // the field on screen
    QByteArray shownPng;
    QString hoverText;
    vector<FrameInfo> frameInfos;      // parallel to the loop frames in the animation bar
    int generation{0};
    int animGeneration{0};
    bool closed{false};
    bool userPickedProduct{false};
    string pendingError;
};

#endif  // SPCREFSVIEWER_H
