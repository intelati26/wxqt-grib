// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include <mutex>
#include <QPointer>
#include "MapWidget.h"
#include "radar/RadarSites.h"
#include <QApplication>
#include <QMenu>
#include <QPainter>
#include <QTimer>
#include <memory>
#include "objects/Color.h"
#include "objects/FutureVoid.h"
#include "objects/PolygonWarning.h"
#include "objects/PolygonWatch.h"
#include "objects/WString.h"
#include "radar/FireDayOne.h"
#include "radar/MapWindBarbs.h"
#include "radar/Projection.h"
#include "radar/SwoDayOne.h"
#include "radar/Warnings.h"
#include "radar/Watch.h"
#include "radar/WpcFronts.h"
#include "settings/RadarPreferences.h"
#include "settings/UIPreferences.h"
#include "util/UtilityIO.h"
#include "util/UtilityList.h"
#include "util/UtilityUI.h"

MapWidget::MapWidget(
    Window * parent,
    int paneNumber,
    int numberOfPanes,
    bool useASpecificRadar,
    const string& radarToUse,
    int originalWidth,
    int originalHeight,
    const function<void(double, int)>& fnZoom,
    const function<void(double, double, int)>& fnPosition
)
    : QWidget{parent}
    , mapState{paneNumber, numberOfPanes, useASpecificRadar, radarToUse, originalWidth, originalHeight}
    , mapTextObject{numberOfPanes, &mapState, &fileStorage}
    , mapDraw{&mapState, &fileStorage, &mapTextObject}
    , fnZoom{fnZoom}
    , fnPosition{fnPosition}
    , parent{parent}
{
    setAttribute(Qt::WA_DeleteOnClose);
    mapState.originalWidth = originalWidth;
    mapState.originalHeight = originalHeight;
    grabGesture(Qt::PinchGesture);
    show();
    mapDraw.initGeom();
}

bool MapWidget::event(QEvent * event) {
    if (event->type() == QEvent::Gesture)
        return gestureEvent(dynamic_cast<QGestureEvent*>(event));
    return QWidget::event(event);
}

// https://doc.qt.io/qt-5/gestures-overview.html
// https://doc.qt.io/qt-5/qgestureevent.html#details
// pinch gesture for mobile
bool MapWidget::gestureEvent(QGestureEvent * event) {
    // https://doc.qt.io/qt-5/qtwidgets-gestures-imagegestures-example.html
    if (QGesture * pinch = event->gesture(Qt::PinchGesture)) {
        pinchTriggered(dynamic_cast<QPinchGesture *>(pinch));
    }
    // if (QGesture *swipe = event->gesture(Qt::SwipeGesture))
    //     swipeTriggered(static_cast<QSwipeGesture *>(swipe));
    // else if (QGesture *pan = event->gesture(Qt::PanGesture))
    //     panTriggered(static_cast<QPanGesture *>(pan));
    // if (QGesture *pinch = event->gesture(Qt::PinchGesture))
    //     pinchTriggered(static_cast<QPinchGesture *>(pinch));
    return true;
}

void MapWidget::pinchTriggered(QPinchGesture *gesture) {
    QPinchGesture::ChangeFlags changeFlags = gesture->changeFlags();
    if (changeFlags & QPinchGesture::RotationAngleChanged) {
        auto rotationDelta = static_cast<int>(gesture->rotationAngle() - gesture->lastRotationAngle());
        rotationAngle += rotationDelta;
        // qDebug() << "pinchTriggered(): rotate by" << rotationDelta << "->" << rotationAngle;
    }
    if (changeFlags & QPinchGesture::ScaleFactorChanged) {
        currentStepScaleFactor = static_cast<int>(gesture->totalScaleFactor());
        fnZoom(gesture->scaleFactor(), mapState.paneNumber);
        // qDebug() << "pinchTriggered(): zoom by" << gesture->scaleFactor() << "->" << currentStepScaleFactor;
    }
    if (gesture->state() == Qt::GestureFinished) {
        scaleFactor *= currentStepScaleFactor;
        currentStepScaleFactor = 1;
    }
    update();
}

void MapWidget::wheelEvent(QWheelEvent * event) {
    if (UIPreferences::mapScrollWheelMotion) {
        if (event->angleDelta().y() > 0) {
            fnZoom(0.77, mapState.paneNumber);
        } else {
            fnZoom(1.33, mapState.paneNumber);
        }
    } else {
        if (event->angleDelta().y() > 0) {
            fnZoom(1.33, mapState.paneNumber);
        } else {
            fnZoom(0.77, mapState.paneNumber);
        }
    }
    update();
}

void MapWidget::mouseMoveEvent(QMouseEvent * event) {
    if (event->buttons() == Qt::NoButton) {
        return;   // a screen that tracks the pointer (MRMS value readout) gets moves with no button down: not a drag
    }
    // mapState.xPos -= mouseStartX - event->pos().x();
    // mapState.yPos -= mouseStartY - event->pos().y();

    lastMouseType = "Drag";
    // update();
    fnPosition(-1.0 * (mouseStartX - event->pos().x()), -1.0 * (mouseStartY - event->pos().y()), mapState.paneNumber);
    mouseStartX = event->pos().x();
    mouseStartY = event->pos().y();
}

void MapWidget::mousePressEvent(QMouseEvent * event) {
    mouseStartX = event->pos().x();
    mouseStartY = event->pos().y();
    lastMouseType = "Click";
}

void MapWidget::mouseDoubleClickEvent([[maybe_unused]] QMouseEvent * event) {
    lastMouseType = "Double Click";
}

void MapWidget::performSingleClickAction() {
    if (lastMouseType == "Click") {
        fnZoom(0.77, mapState.paneNumber);
    }
}

void MapWidget::mouseReleaseEvent([[maybe_unused]] QMouseEvent * event) {
    if (lastMouseType == "Drag") {
        update();
    } else if (lastMouseType == "Click") {
        clickAt = event->position();
        if (clickHandler && clickHandler(clickAt)) {
            lastMouseType = "";   // the owner used the click (a gauge, a marker): it does not also zoom
            return;
        }
        QTimer::singleShot(QApplication::doubleClickInterval(), [this]() {performSingleClickAction();});
    } else {
        fnZoom(1.33, mapState.paneNumber);
    }
}

void MapWidget::paintEvent(QPaintEvent * event) {
    QPainter painter{this};
    mapDraw.initSurface(&painter, event);
    if (dataLayer) {
        painter.save();
        dataLayer(painter);
        painter.restore();
    }
    // if (mapState.zoom > 0.9 && !hideRoads) {
    //     mapDraw.drawGeomLine(HwExtLines);
    // }
    // if (mapState.zoom > 0.5) {
    //     mapDraw.drawGeomLine(CountyLines);
    //     if (!hideRoads) {
    //         mapDraw.drawGeomLine(HwLines);
    //     }
    //     mapDraw.drawGeomLine(LakeLines);
    // }
    // mapDraw.drawGeomLine(StateLines);
    // mapDraw.drawGeomLine(CaLines);
    // mapDraw.drawGeomLine(MxLines);

    if (mapState.zoom > 0.7) {
        for (auto t : {CountyLines, HwLines, HwExtLines, LakeLines}) {
            mapDraw.drawGeomLine(t);
        }
    }
    for (auto t : {StateLines, CaLines, MxLines}) {
        mapDraw.drawGeomLine(t);
    }

    if (RadarPreferences::locationDot) {
        mapDraw.drawGenericCircles(RadarPreferences::locdotSize, fileStorage.locationDotsColor, fileStorage.locationDotsTransformed);
    }
    if (RadarPreferences::obsWindbarbs && !windBarbCircleColors.empty() && mapState.zoom > 0.3) {
        mapDraw.drawGenericLine(RadarPreferences::wbLinesize, Qt::red, wbGustLines);
        mapDraw.drawGenericLine(RadarPreferences::wbLinesize, RadarPreferences::colorObsWindbarbs, wbLines);
        mapDraw.drawGenericCircles(RadarPreferences::aviationSize * 2.0, windBarbCircleColors, windBarbCirclesTransformed);
    }
    drawWatch();
    drawWarnings();
    if (RadarPreferences::swo) {
        drawSwo();
    }
    if (RadarPreferences::fire) {
        drawFire();
    }
    if (RadarPreferences::wpcFronts && mapState.zoom < 0.5) {
        drawWpcFronts();
    }
    // KEEP
    // if (RadarPreferences::locdotFollowsGps) {
    //     for (int i = 0; i < locationDotsTransformedGps.size(); i += 2) {
    //         painter.setPen(QPen(RadarPreferences::colorLocdot, 1.5 / mapState.zoom, Qt::SolidLine));
    //         painter.setBrush(QBrush(RadarPreferences::colorLocdot, Qt::SolidPattern));
    //         QPointF center = QPointF(locationDotsTransformedGps[i], locationDotsTransformedGps[i + 1]);
    //         painter.drawEllipse(center, scaledCircleSize, scaledCircleSize);
    //     }
    //     for (int i = 0; i < locationDotsTransformedGps.size(); i += 2) {
    //         painter.setPen(QPen(RadarPreferences::colorLocdot, 1.5 / mapState.zoom, Qt::SolidLine));
    //         painter.setBrush(QBrush(RadarPreferences::colorLocdot, Qt::NoBrush));
    //         QPointF center = QPointF(locationDotsTransformedGps[i], locationDotsTransformedGps[i + 1]);
    //         painter.drawEllipse(center, scaledCircleSize * 6.0, scaledCircleSize * 6.0);
    //     }
    // }
    if (RadarPreferences::cities && mapState.zoom > 0.5) {
        mapDraw.drawText(RadarPreferences::colorCity, mapState.cities);
    }
    if (RadarPreferences::countyLabels && mapState.zoom > 0.9) {
        mapDraw.drawText(RadarPreferences::colorCountyLabels, mapState.countyLabels);
    }
    if (RadarPreferences::obs && mapState.zoom > 0.5) {
        mapDraw.drawText(RadarPreferences::colorObs, mapState.observations);
    }
    if (topLayer) {
        painter.setWorldTransform(QTransform{});   // keep the window mapping: window units, no pan / zoom
        topLayer(painter);
    }
}

void MapWidget::drawSwo() {
    for (auto riskLevelIndex : range(SwoDayOne::threatList.size())) {
        if (SwoDayOne::polygonBy.contains(riskLevelIndex) && swoLinesMap.contains(riskLevelIndex)) {
            mapDraw.drawGenericLine(
                RadarPreferences::swoLinesize,
                SwoDayOne::colors[riskLevelIndex],
                swoLinesMap[riskLevelIndex]);
        }
    }
}

void MapWidget::drawFire() {
    for (auto riskLevelIndex : range(FireDayOne::threatList.size())) {
        if (FireDayOne::polygonBy.contains(riskLevelIndex) && fireLinesMap.contains(riskLevelIndex)) {
            mapDraw.drawGenericLine(
                RadarPreferences::swoLinesize,
                FireDayOne::colors[riskLevelIndex],
                fireLinesMap[riskLevelIndex]);
        }
    }
}

void MapWidget::drawWpcFronts() {
    if (mapState.zoom < 0.5) {
        for (const auto& front : WpcFronts::fronts) {
            if (front.coordinatesModified[mapState.paneNumber].size() > 1 && front.coordinatesModified[mapState.paneNumber].size() < 500) {
                mapDraw.drawGenericLine(
                    RadarPreferences::watmcdLinesize,
                    front.penColor,
                    front.coordinatesModified[mapState.paneNumber]);
            }
        }
        mapDraw.drawText(Qt::red, mapState.pressureCenterLabelsRed);
        mapDraw.drawText(Qt::blue, mapState.pressureCenterLabelsBlue);
    }
}

void MapWidget::drawWarnings() {
    for (const auto type1 : PolygonWarning::polygonList) {
        if (PolygonWarning::byType[type1]->isEnabled && polygons.contains(type1)) {
            mapDraw.drawGenericLine(
                RadarPreferences::warnLinesize,
                PolygonWarning::byType[type1]->colorInt,
                polygons[type1]);
        }
    }
}

void MapWidget::drawWatch() {
    for (const auto type1 : PolygonWatch::polygonList) {
        if (PolygonWatch::byType[type1]->isEnabled && polygons.contains(type1)) {
            mapDraw.drawGenericLine(
                RadarPreferences::watmcdLinesize,
                PolygonWatch::byType[type1]->colorInt,
                polygons[type1]);
        }
    }
}

// KEEP
MapWidget::~MapWidget() {
    jobGuard->closeAndWait();   // a download or decode still running uses this widget's members
}

void MapWidget::runJob(const function<void()>& work, const function<void()>& done) {
    const auto guard = jobGuard;
    const QPointer<MapWidget> self{this};
    new FutureVoid{parent,
        [guard, work] {
            if (!guard->enter()) {
                return;
            }
            try {
                work();
            } catch (...) {
                // a failed download leaves the old picture; nothing to report from here
            }
            guard->leave();
        },
        [self, done] {
            if (!self.isNull()) {
                done();
            }
        }};
}

// KEEP
// void MapWidget::updateGps(double lat, double lon) {
//    gpsX = lat;
//    gpsY = lon;
//    if (RadarPreferences::locdotFollowsGps) {
//        locationDotsTransformedGps.clear();
//        // lat lon are correct pos / neg but must match below
//        auto coords = UtilityCanvasProjection::computeMercatorNumbers(gpsX, -1.0 * gpsY, mapState.getPn());
//        locationDotsTransformedGps.append(coords);
//    }
// }

void MapWidget::constructWBLines() {
    if (RadarPreferences::obs) {
        mapTextObject.addTextLabelsObservations();
    }
    if (RadarPreferences::obsWindbarbs) {
        windBarbCirclesTransformed.clear();
        windBarbCircleColors.clear();
        wbLines.clear();
        wbGustLines.clear();
        const auto wBFloats = MapWindBarbs::decodeAndPlot(mapState.getPn(), false, fileStorage);
        for (auto x : range3(0, wBFloats.size(), 4)) {
            wbLines.push_back(QLineF{wBFloats[x], wBFloats[x + 1], wBFloats[x + 2], wBFloats[x + 3]});
        }
        const auto wBGustFloats = MapWindBarbs::decodeAndPlot(mapState.getPn(), true, fileStorage);
        for (auto x : range3(0, wBGustFloats.size(), 4)) {
            wbGustLines.push_back(QLineF{wBGustFloats[x], wBGustFloats[x + 1], wBGustFloats[x + 2], wBGustFloats[x + 3]});
        }
        vector<double> obsX;
        vector<double> obsY;
        vector<int> obsColor;
        {
            const std::lock_guard<std::mutex> guard{*fileStorage.lock};   // a worker may be replacing the lists
            obsX = fileStorage.obsArrX;
            obsY = fileStorage.obsArrY;
            obsColor = fileStorage.obsArrAviationColor;
        }
        for (auto index : range(std::min(obsX.size(), std::min(obsY.size(), obsColor.size())))) {
            const auto rawColor = obsColor[index];
            windBarbCirclesTransformed.push_back(Projection::computeMercatorNumbers(obsX[index], obsY[index], mapState.getPn()));
            windBarbCircleColors.emplace_back(Color::red(rawColor), Color::green(rawColor), Color::blue(rawColor));
        }
    }
}

void MapWidget::process(PolygonType polygonType) {
    const auto numbers = Watch::add(mapState.getPn(), polygonType);
    polygons[polygonType] = QVector<QLineF>();
    for (auto position : range3(0, numbers.size(), 4)) {
        polygons[polygonType].push_back(QLineF(numbers[position], numbers[position + 1], numbers[position + 2], numbers[position + 3]));
    }
}

void MapWidget::constructSwo() {
    for (auto riskLevelIndex : range(SwoDayOne::threatList.size())) {
        if (SwoDayOne::polygonBy.contains(riskLevelIndex)) {
            swoLinesMap[riskLevelIndex] = QVector<QLineF>();
            for (auto x : range3(0, SwoDayOne::polygonBy[riskLevelIndex].size(), 4)) {
                const auto floatList = SwoDayOne::polygonBy[riskLevelIndex];
                const auto coords1 = Projection::computeMercatorNumbers(floatList[x], floatList[x + 1], mapState.getPn());
                const auto coords2 = Projection::computeMercatorNumbers(floatList[x + 2], floatList[x + 3], mapState.getPn());
                swoLinesMap[riskLevelIndex].push_back(QLineF{coords1[0], coords1[1], coords2[0], coords2[1]});
            }
        } else {
            if (swoLinesMap.contains(riskLevelIndex)) {
                swoLinesMap[riskLevelIndex].clear();
            }
        }
    }
}

void MapWidget::constructFire() {
    for (auto riskLevelIndex : range(FireDayOne::threatList.size())) {
        if (FireDayOne::polygonBy.contains(riskLevelIndex)) {
            fireLinesMap[riskLevelIndex] = QVector<QLineF>();
            for (auto x : range3(0, FireDayOne::polygonBy[riskLevelIndex].size(), 4)) {
                const auto floatList = FireDayOne::polygonBy[riskLevelIndex];
                const auto coords1 = Projection::computeMercatorNumbers(floatList[x], floatList[x + 1], mapState.getPn());
                const auto coords2 = Projection::computeMercatorNumbers(floatList[x + 2], floatList[x + 3], mapState.getPn());
                fireLinesMap[riskLevelIndex].push_back(QLineF{coords1[0], coords1[1], coords2[0], coords2[1]});
            }
        } else {
            if (fireLinesMap.contains(riskLevelIndex)) {
                fireLinesMap[riskLevelIndex].clear();
            }
        }
    }
}

void MapWidget::constructWpcFronts() {
    for (auto& front : WpcFronts::fronts) {
        front.translate(mapState.paneNumber, mapState.getPn());
    }
    mapTextObject.addWpcPressureCenters();
}

void MapWidget::processWarnings(PolygonType polygonGenericType) {
    const auto numbers = Warnings::add(mapState.getPn(), polygonGenericType);
    polygons[polygonGenericType] = QVector<QLineF>();
    for (auto position : range3(0, numbers.size(), 4)) {
        polygons[polygonGenericType].push_back(QLineF{numbers[position], numbers[position + 1], numbers[position + 2], numbers[position + 3]});
    }
}

void MapWidget::draw() {
    update();
}
