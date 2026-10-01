// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef MRMSVIEWER_H
#define MRMSVIEWER_H

#include <memory>
#include <string>
#include <vector>
#include <QByteArray>
#include <QVector>
#include <QTimer>
#include "mrms/UtilityMrms.h"
#include "radar/NexradWidget.h"
#include "ui/BackForward.h"
#include "ui/Button.h"
#include "ui/ComboBox.h"
#include "ui/HBox.h"
#include "ui/Text.h"
#include "ui/VBox.h"
#include "ui/Window.h"

using std::string;
using std::vector;

// NOAA MRMS radar-derived products (reflectivity, hail, rotation, rain, ...) on the same scrollable, zoomable map as
// the radar screens - their state / county / highway lines, cities, location dot and warnings - with the scan picked
// by time and a loop of the latest scans. The picture of a scan comes from UtilityMrms (GDAL decodes the GRIB into
// the CONUS grid in Web Mercator); here it is placed on the radar projection with two corner points.
class MrmsViewer : public Window {
public:
    explicit MrmsViewer(Window * parent);

private:
    void loadScans();                 // the scan list for the chosen product, then the newest scan
    void showScan(int comboIndex);
    void startLoop();
    void stopLoop();
    void stepLoop();
    void setFrame(const UtilityMrms::Frame&);
    void paintData(QPainter&);
    void paintLegend(QPainter&);
    void moveScan(int step);
    void closeEventCustom() override;
    const UtilityMrms::Product& product() const;

    VBox box;
    HBox rowTop;
    ComboBox comboProduct;
    ComboBox comboScan;
    BackForward backForward;
    ComboBox comboLoop;
    Button buttonLoop;
    Text textStatus;
    NexradWidget * radar{};

    vector<UtilityMrms::Scan> scans;          // oldest first
    vector<int> comboToScan;                  // combo row (newest first) -> index into scans
    UtilityMrms::Frame current;
    QByteArray currentIndices;                // the shown scan, one 8-bit index per grid cell
    QVector<QRgb> currentColors;
    vector<UtilityMrms::Frame> loopFrames;
    size_t loopPosition{0};
    QTimer loopTimer;
    bool looping{false};
    int generation{0};                        // bumped whenever work in flight should be dropped
    bool closed{false};
    string pendingError;
    UtilityMrms::Frame pendingFrame;
    vector<UtilityMrms::Frame> pendingLoop;
};

#endif  // MRMSVIEWER_H
