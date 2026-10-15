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
#include <QDialog>
#include <QPointer>
#include <QLabel>
#include <QPointF>
#include <QVector>
#include <QTimer>
#include "mrms/MrmsOverlays.h"
#include "mrms/UtilityMrms.h"
#include "radar/MapLayerDownload.h"
#include "radar/MapWidget.h"
#include "ui/BackForward.h"
#include "ui/Button.h"
#include "ui/ComboBox.h"
#include "ui/HBox.h"
#include "ui/ControlRow.h"
#include "ui/Shortcut.h"
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
    void showProductTree();                         // the product list as groups that open and close, in place of the combo's flat list
    void loadLightning();                           // the lightning overlay's points (NLDN cloud-to-ground density of MRMS), drawn over the map as a point grid
    void updateLocations();                         // the shown product's value at each saved location, as a line of text
    bool us() const { return comboUnits.getIndex() == 0; }
    void loadScans();                 // the scan list for the chosen product, then the newest scan
    void showScan(int comboIndex);
    void startLoop();
    void stopLoop();
    void stepLoop();
    void setFrame(const UtilityMrms::Frame&);
    void paintData(QPainter&);
    void drawGrid(QPainter&, const UtilityMrms::Grid&, const QByteArray& indices, const QVector<QRgb>& colors, bool outline);
    void loadChance();     // the lightning probability of the next 30 / 60 minutes as contour lines over the map
    void loadHail();       // the hail swaths over the product (MRMS MESH maximum of a period), from nickel size up
    void loadRotation();   // the rotation tracks over the product (MRMS low- or mid-level rotation track of a period), strong rotation only
    void showOverlays();            // the panel of the overlays: stays open while several are chosen
    void refreshOverlayLabel();
    void paintLegend(QPainter&);
    void moveScan(int step);
    void onSave();                    // the map (or the loaded loop) with a title / valid time / save time border, as PNG / WebP / ...
    QByteArray renderFrame(const UtilityMrms::Frame&, const QString& units, const QString& extra);
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
    void showConus();
    void showHover(const QPointF& widgetPos);
    const UtilityMrms::Product& product() const;

    VBox box;
    ControlRow rowTop;
    ComboBox comboProduct;
    ComboBox comboScan;
    BackForward backForward;
    ComboBox comboLoop;
    ComboBox comboUnits;   // US (inches, kft) or metric
    ComboBox comboAuto;    // how often to look for a newer scan
    ComboBox comboLightning;
    ComboBox comboChance;      // the lightning probability contours: off, next 30 or 60 minutes
    ComboBox comboRotation;    // rotation tracks over any product: off, or a period of the 0-2 / 3-6 km track   // lightning points over any product: off, or the last 1 / 5 / 15 / 30 minutes
    Button buttonOverlays;     // opens the overlay panel (the four combos below hold the choices; they are not shown)
    QPointer<QDialog> overlayDialog;
    Button buttonLoop;
    Button buttonSave;
    Shortcut shortcutSave;
    Text textStatus;
    MapWidget * radar{};
    std::vector<MapWidget *> radarList;                  // the one map, in the form the overlay downloader takes
    std::unique_ptr<MapLayerDownload> layerDownload;      // warnings, watches, outlooks, fronts, observations (the Map settings)
    QLabel * hoverLabel{};     // the coordinate / value popup over the map
    QLabel * labelLocations{}; // "Home: 0.82 in   Springfield: none" under the status line
    using Strike = MrmsOverlays::Strike;
    vector<Strike> lightningPoints;   // each cell with a strike, by how recent
    QDateTime lightningTime;
    int lightningGeneration{0};
    bool lightningLoading{false};
    using ChanceLine = MrmsOverlays::ChanceLine;
    vector<ChanceLine> chanceLines;
    int chanceGeneration{0};
    bool chanceLoading{false};
    ComboBox comboHail;        // hail swaths over any product: off, or the largest MESH of the last 30 min .. 24 h
    vector<std::pair<UtilityMrms::Grid, QByteArray>> hailLevels;   // the swath at 1 / 4 / 16 cell blocks, like the rotation tracks
    QVector<QRgb> hailColors;
    int hailGeneration{0};
    bool hailLoading{false};
    // the tracks at coarser cells too (the largest value of each 4 x 4 and 16 x 16 block): a track a few hundred meters wide still shows when a pixel spans kilometers
    vector<std::pair<UtilityMrms::Grid, QByteArray>> rotationLevels;
    QVector<QRgb> rotationColors;
    int rotationGeneration{0};
    bool rotationLoading{false};
    bool hoverShown{false};    // draw the crosshair at `pointer`
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
