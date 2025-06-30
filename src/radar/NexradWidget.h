// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef NEXRADWIDGET_H
#define NEXRADWIDGET_H

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <QAction>
#include <QGestureEvent>
#include <QLabel>
#include <QLineF>
#include <QPinchGesture>
#include "objects/FileStorage.h"
#include "objects/LatLon.h"
#include "radar/NexradColorLegend.h"
#include "radar/NexradDraw.h"
#include "radar/NexradLevelData.h"
#include "radar/NexradRenderTextObject.h"
#include "radar/NexradState.h"
#include "radar/NexradStateAnimation.h"
#include "radar/PolygonType.h"
#include "radar/ProjectionNumbers.h"
#include "ui/RadarStatusBox.h"
#include "ui/TextViewMetal.h"
#include "ui/Window.h"

using std::function;
using std::string;
using std::unordered_map;
using std::vector;

class NexradWidget : public QWidget {
public:
    NexradWidget(
        Window *, int, int, bool, const string&, int, int,
        const function<void(int, string)>&,
        const function<void(int, string)>&,
        const function<void(double, int)>&,
        const function<void(double, double, int)>&,
        const function<void()>&
    );
    ~NexradWidget() override;
//    void updateGps(double, double);
    void downloadDataForAnimation(int);
    void downloadData();
    void changeProduct();
    void processWarnings(PolygonType);
    void process(PolygonType);
    void constructSwo();
    void constructFire();
    void constructWBLines();
    void constructSti();
    void constructHi();
    void constructTvs();
    void constructWpcFronts();
    void resizePolygons();
    void draw();
    FileStorage fileStorage;
    NexradState nexradState;
    NexradRenderTextObject nexradRenderTextObject;
    NexradLevelData levelData;
    NexradDraw nexradDraw;
    std::unique_ptr<RadarStatusBox> radarStatusBox;
    NexradStateAnimation nexradStateAnimation;
    NexradColorLegend colorLegend;

protected:
    void paintEvent(QPaintEvent *) override;
    bool event(QEvent *) override;
    bool gestureEvent(QGestureEvent *);
    void wheelEvent(QWheelEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void contextMenuEvent(QContextMenuEvent *) override;
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
    void updateTitle();
    void toggleRadar();
    double mouseStartX{};
    double mouseStartY{};
    function<void(int, string)> fnProduct;
    function<void(int, string)> fnSector;
    function<void(double, int)> fnZoom;
    function<void(double, double, int)> fnPosition;
    function<void()> setTitleMain;
    unordered_map<int, QVector<QLineF>> swoLinesMap;
    unordered_map<int, QVector<QLineF>> fireLinesMap;
    int totalBins{};
    unordered_map<PolygonType, QVector<QLineF>> polygons;
    // vector<LatLon> locationDots;
    // double gpsX{};
    // double gpsY{};
    vector<vector<double>> windBarbCirclesTransformed;
    vector<QColor> windBarbCircleColors;
    QVector<QLineF> wbLines;
    QVector<QLineF> wbGustLines;
    QVector<QLineF> stormTrackLines;
    vector<QPolygonF> hiPolygons;
    vector<QPolygonF> tvsPolygons;
    string lastMouseType;
    bool hideRadar{false};
    bool hideRoads{false};
    int toggleIndex{};
    // used by pinch zoom
    int rotationAngle{};
    int currentStepScaleFactor{};
    int scaleFactor{};
    Window * parent;
};

#endif  // NEXRADWIDGET_H
