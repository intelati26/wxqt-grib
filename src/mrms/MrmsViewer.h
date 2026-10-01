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
#include <QPointF>
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
    void rebuildProducts(const string& selectId);   // the grouped product list: hand-set ones, then the rest of the server's
    void refreshNewest();                           // auto-update: a newer scan than the one shown?
    bool us() const { return comboUnits.getIndex() == 0; }
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
    void resizeEventCustom() override;
    void changeZoom(double factor);              // the radar widget's wheel / click zoom
    void changePosition(double dx, double dy);   // its drag pan, in pixels
    void fitRadar();                             // the map is a square that fills the window
    bool eventFilter(QObject *, QEvent *) override;
    struct Projection2 {   // the radar projection as x = ax * lon + bx, y = ay * mercator(lat) + by
        double ax;
        double bx;
        double ay;
        double by;
    };
    Projection2 projection() const;
    void showHover(const QPointF& widgetPos);
    const UtilityMrms::Product& product() const;

    VBox box;
    HBox rowTop;
    ComboBox comboProduct;
    ComboBox comboScan;
    BackForward backForward;
    ComboBox comboLoop;
    ComboBox comboUnits;   // US (inches, kft) or metric
    ComboBox comboAuto;    // how often to look for a newer scan
    Button buttonLoop;
    Text textStatus;
    NexradWidget * radar{};
    QPointF pointer;           // last position of the mouse over the map, for zooming about it
    bool pointerInside{false};

    vector<UtilityMrms::Product> extraProducts;   // discovered on the server
    vector<UtilityMrms::Product> productList;     // what the product combo shows, in order
    QTimer autoTimer;
    bool refreshing{false};
    bool userPickedProduct{false};   // so a late-arriving product list does not override a choice made meanwhile
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
