// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef MAPWIDGET_H
#define MAPWIDGET_H

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include <QAction>
#include <QGestureEvent>
#include <QLabel>
#include <QLineF>
#include <QPainter>
#include <QPinchGesture>
#include "objects/FileStorage.h"
#include "radar/JobGuard.h"
#include "objects/LatLon.h"
#include "radar/MapDraw.h"
#include "radar/MapTextObject.h"
#include "radar/MapState.h"
#include "radar/PolygonType.h"
#include "radar/ProjectionNumbers.h"
#include "ui/TextViewMetal.h"
#include "ui/Window.h"

using std::function;
using std::string;
using std::unordered_map;
using std::vector;

// The map every map screen is built on: the state, county and highway lines, cities, the location dot and the overlays (warnings, watches
// and discussions, outlooks, fronts, observations), panned and zoomed by its owner. The owner paints what it shows on it with `dataLayer`
// (under the lines, in map coordinates) and `topLayer` (over everything, in window units).
class MapWidget : public QWidget {
public:
    // owner callbacks: zoom by a factor (wheel, pinch, click) and pan by pixels (drag); the numbers 0 / 1 / true are the pane and the
    // projection's radar site: the map is centred on `radarToUse`
    MapWidget(
        Window *, int, int, bool, const string&, int, int,
        const function<void(double, int)>&,
        const function<void(double, double, int)>&
    );
    ~MapWidget() override;
    // Run `work` on a worker thread, then `done` on the UI thread. The widget is kept alive until the work has finished (its
    // destructor waits), and `done` is skipped if the widget has gone by then.
    void runJob(const function<void()>& work, const function<void()>& done);
    void processWarnings(PolygonType);
    void process(PolygonType);
    void constructSwo();
    void constructFire();
    void constructWBLines();
    void constructWpcFronts();
    void draw();
    FileStorage fileStorage;
    MapState mapState;
    MapTextObject mapTextObject;
    MapDraw mapDraw;
    function<void(QPainter&)> dataLayer;
    function<void(QPainter&)> topLayer;
    // when set, a plain click goes to it first (the point in widget pixels); true means it was used, and the click does not zoom
    function<bool(const QPointF&)> clickHandler;

protected:
    void paintEvent(QPaintEvent *) override;
    bool event(QEvent *) override;
    bool gestureEvent(QGestureEvent *);
    void wheelEvent(QWheelEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseDoubleClickEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;

private slots:
    void performSingleClickAction();

private:
    void drawSwo();
    void drawFire();
    void drawWpcFronts();
    void drawWarnings();
    void drawWatch();
    void pinchTriggered(QPinchGesture *);
    double mouseStartX{};
    double mouseStartY{};
    QPointF clickAt;
    function<void(double, int)> fnZoom;
    function<void(double, double, int)> fnPosition;
    unordered_map<int, QVector<QLineF>> swoLinesMap;
    unordered_map<int, QVector<QLineF>> fireLinesMap;
    std::shared_ptr<JobGuard> jobGuard{std::make_shared<JobGuard>()};
    unordered_map<PolygonType, QVector<QLineF>> polygons;
    vector<vector<double>> windBarbCirclesTransformed;
    vector<QColor> windBarbCircleColors;
    QVector<QLineF> wbLines;
    QVector<QLineF> wbGustLines;
    string lastMouseType;
    // used by pinch zoom
    int rotationAngle{};
    int currentStepScaleFactor{};
    int scaleFactor{};
    Window * parent;
};

#endif  // MAPWIDGET_H
