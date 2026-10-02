// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include <mutex>
#include <QPointer>
#include "NexradWidget.h"
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
#include "radar/NexradDownload.h"
#include "radar/NexradLevel3WindBarbs.h"
#include "radar/NexradLongPressMenu.h"
#include "radar/NexradRenderUI.h"
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

NexradWidget::NexradWidget(
    Window * parent,
    int paneNumber,
    int numberOfPanes,
    bool useASpecificRadar,
    const string& radarToUse,
    int originalWidth,
    int originalHeight,
    const function<void(int, string)>& fnProduct,
    const function<void(int, string)>& fnSector,
    const function<void(double, int)>& fnZoom,
    const function<void(double, double, int)>& fnPosition,
    const function<void()>& setTitleMain
)
    : QWidget{parent}
    , nexradState{paneNumber, numberOfPanes, useASpecificRadar, radarToUse, originalWidth, originalHeight}
    , nexradRenderTextObject{numberOfPanes, &nexradState, &fileStorage}
    , levelData{&nexradState, &fileStorage}
    , nexradDraw{&nexradState, &fileStorage, &nexradRenderTextObject}
    , radarStatusBox{std::make_unique<RadarStatusBox>(parent)}
    , colorLegend{nexradState.getRadarProduct()}
    , fnProduct{fnProduct}
    , fnSector{fnSector}
    , fnZoom{fnZoom}
    , fnPosition{fnPosition}
    , setTitleMain{setTitleMain}
    , parent{parent}
{
    setAttribute(Qt::WA_DeleteOnClose);

    // # FIXME TODO see wxpygtk
    // self.highwayZoom: float = 0.9
    // # if mainWindow:
    // #     self.highwayZoom = 0.58

    nexradState.originalWidth = originalWidth;
    nexradState.originalHeight = originalHeight;
    grabGesture(Qt::TapAndHoldGesture);
    grabGesture(Qt::PinchGesture);
    show();

    radarStatusBox->connect([this] { toggleRadar(); });
    nexradDraw.initGeom();
}

bool NexradWidget::event(QEvent * event) {
    if (event->type() == QEvent::Gesture)
        return gestureEvent(dynamic_cast<QGestureEvent*>(event));
    return QWidget::event(event);
}

// https://doc.qt.io/qt-5/gestures-overview.html
// https://doc.qt.io/qt-5/qgestureevent.html#details
// pinch gesture for mobile
bool NexradWidget::gestureEvent(QGestureEvent * event) {
    if (auto gesture = event->gesture(Qt::TapAndHoldGesture)) {
        auto t = dynamic_cast<QTapAndHoldGesture *>(gesture);
        if (t && t->state() == Qt::GestureStarted) {
            // emit TapAndHoldStarted();
            if (UtilityUI::isMobile()) {
                const auto posF = t->position();
                const auto posGlobal = QPoint{static_cast<int>(posF.x()), static_cast<int>(posF.y())};
                const auto positionRelative = mapFromGlobal(posGlobal);
                const auto latLon = NexradRenderUI::getLatLonFromScreenPosition(nexradState, positionRelative.x(), positionRelative.y());
                NexradLongPressMenu::setupContextMenu(parent, posGlobal, nexradState, latLon, fnSector, fnProduct);
            }
        } else if (t && t->state() == Qt::GestureFinished) {
            // qDebug() << "tap and hold end " << event;
            // emit TapAndHoldFinished();
        }
    }
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

void NexradWidget::pinchTriggered(QPinchGesture *gesture) {
    QPinchGesture::ChangeFlags changeFlags = gesture->changeFlags();
    if (changeFlags & QPinchGesture::RotationAngleChanged) {
        auto rotationDelta = static_cast<int>(gesture->rotationAngle() - gesture->lastRotationAngle());
        rotationAngle += rotationDelta;
        // qDebug() << "pinchTriggered(): rotate by" << rotationDelta << "->" << rotationAngle;
    }
    if (changeFlags & QPinchGesture::ScaleFactorChanged) {
        currentStepScaleFactor = static_cast<int>(gesture->totalScaleFactor());
        fnZoom(gesture->scaleFactor(), nexradState.paneNumber);
        // qDebug() << "pinchTriggered(): zoom by" << gesture->scaleFactor() << "->" << currentStepScaleFactor;
    }
    if (gesture->state() == Qt::GestureFinished) {
        scaleFactor *= currentStepScaleFactor;
        currentStepScaleFactor = 1;
    }
    update();
}

void NexradWidget::wheelEvent(QWheelEvent * event) {
    if (UIPreferences::nexradScrollWheelMotion) {
        if (event->angleDelta().y() > 0) {
            fnZoom(0.77, nexradState.paneNumber);
        } else {
            fnZoom(1.33, nexradState.paneNumber);
        }
    } else {
        if (event->angleDelta().y() > 0) {
            fnZoom(1.33, nexradState.paneNumber);
        } else {
            fnZoom(0.77, nexradState.paneNumber);
        }
    }
    update();
}

void NexradWidget::mouseMoveEvent(QMouseEvent * event) {
    if (event->buttons() == Qt::NoButton) {
        return;   // a screen that tracks the pointer (MRMS value readout) gets moves with no button down: not a drag
    }
    // nexradState.xPos -= mouseStartX - event->pos().x();
    // nexradState.yPos -= mouseStartY - event->pos().y();

    lastMouseType = "Drag";
    // update();
    fnPosition(-1.0 * (mouseStartX - event->pos().x()), -1.0 * (mouseStartY - event->pos().y()), nexradState.paneNumber);
    mouseStartX = event->pos().x();
    mouseStartY = event->pos().y();
}

void NexradWidget::mousePressEvent(QMouseEvent * event) {
    mouseStartX = event->pos().x();
    mouseStartY = event->pos().y();
    lastMouseType = "Click";
    ctrlHeld = (event->modifiers() & Qt::ControlModifier) != 0;
}

void NexradWidget::mouseDoubleClickEvent([[maybe_unused]] QMouseEvent * event) {
    lastMouseType = "Double Click";
}

void NexradWidget::performSingleClickAction() {
    if (lastMouseType == "Click") {
        if (onClick) {
            onClick();
            return;
        }
        fnZoom(0.77, nexradState.paneNumber);
    }
}

void NexradWidget::mouseReleaseEvent([[maybe_unused]] QMouseEvent * event) {
    if (ctrlHeld && lastMouseType == "Click" && !onClick) {
        // Ctrl+click: switch this pane to the radar site nearest the clicked point
        ctrlHeld = false;
        lastMouseType = "";
        const auto latLon = NexradRenderUI::getLatLonFromScreenPosition(nexradState, event->pos().x(), event->pos().y());
        const auto code = RadarSites::getNearestCode(latLon);
        if (!code.empty() && code != nexradState.getRadarSite()) {
            fnSector(nexradState.paneNumber, code);
        }
        return;
    }
    if (lastMouseType == "Drag") {
        update();
    } else if (lastMouseType == "Click") {
        QTimer::singleShot(QApplication::doubleClickInterval(), [this]() {performSingleClickAction();});
    } else if (!onClick) {
        fnZoom(1.33, nexradState.paneNumber);
    }
}

void NexradWidget::contextMenuEvent(QContextMenuEvent * event) {
    const auto latLon = NexradRenderUI::getLatLonFromScreenPosition(nexradState, event->pos().x(), event->pos().y());
    NexradLongPressMenu::setupContextMenu(parent, event->globalPos(), nexradState, latLon, fnSector, fnProduct);
}

// the radar bins; the painter has the window / pan / zoom transform of NexradDraw::initSurface
void NexradWidget::drawRadarBins(QPainter& painter) {
    // the bins tile exactly, so they are filled without an outline (much faster); a brush is set only when the level changes
    painter.setPen(Qt::NoPen);
    const auto& quads = levelData.radarBuffers.quads;
    const auto& levels = levelData.radarBuffers.levels;
    int lastLevel = -1;
    const auto count = std::min(static_cast<size_t>(std::max(totalBins, 0)), quads.size());
    for (size_t bin = 0; bin < count; bin += 1) {
        if (levels[bin] != lastLevel) {
            lastLevel = levels[bin];
            painter.setBrush(levelData.radarBuffers.brushOf(lastLevel));
        }
        const auto& quad = quads[bin];
        const QPointF corners[4]{{quad.x[0], quad.y[0]}, {quad.x[1], quad.y[1]}, {quad.x[2], quad.y[2]}, {quad.x[3], quad.y[3]}};
        painter.drawPolygon(corners, 4);
    }
}

void NexradWidget::paintRadarLayer(QPainter& painter) {
    const std::lock_guard<std::mutex> guard{dataLock};
    // the window is 1000 x 1000 map units (NexradDraw::initSurface); fx / fy turn units into pixels
    const auto fx = width() / 1000.0;
    const auto fy = height() / 1000.0;
    const auto margin = 250;   // map units kept beyond each side of the window
    const auto pixelsWide = qRound((1000 + 2 * margin) * fx);
    const auto pixelsHigh = qRound((1000 + 2 * margin) * fy);
    const auto ratio = devicePixelRatioF();
    const QColor background = RadarPreferences::nexradRadarBackgroundColor;
    if (static_cast<qint64>(pixelsWide) * pixelsHigh > 48'000'000 || pixelsWide < 1 || pixelsHigh < 1) {
        drawRadarBins(painter);   // too big to keep a picture of
        return;
    }
    const auto usable = !radarCache.isNull() && cacheVersion == dataVersion && cacheZoom == nexradState.zoom &&
                        cacheWidgetSize == size() && cacheRatio == ratio && cacheBackground == background &&
                        std::abs(nexradState.xPos - cacheX) <= margin * 0.95 && std::abs(nexradState.yPos - cacheY) <= margin * 0.95;
    if (!usable) {
        QPixmap picture{QSize{pixelsWide, pixelsHigh} * ratio};
        picture.setDevicePixelRatio(ratio);
        picture.fill(background);
        QPainter cache{&picture};
        cache.setViewport(0, 0, pixelsWide, pixelsHigh);
        cache.setWindow(-500 - margin, -250 - margin, 1000 + 2 * margin, 1000 + 2 * margin);
        cache.translate(nexradState.xPos, nexradState.yPos);
        cache.scale(nexradState.zoom, nexradState.zoom);
        drawRadarBins(cache);
        cache.end();
        radarCache = picture;
        cacheVersion = dataVersion;
        cacheZoom = nexradState.zoom;
        cacheX = nexradState.xPos;
        cacheY = nexradState.yPos;
        cacheWidgetSize = size();
        cacheRatio = ratio;
        cacheBackground = background;
    }
    // draw the picture in plain pixels, moved by the pan since it was made
    painter.save();
    painter.resetTransform();
    painter.drawPixmap(QPointF{-margin * fx + (nexradState.xPos - cacheX) * fx, -margin * fy + (nexradState.yPos - cacheY) * fy}, radarCache);
    painter.restore();
}

void NexradWidget::paintEvent(QPaintEvent * event) {
    QPainter painter{this};
    nexradDraw.initSurface(&painter, event);
    if (dataLayer) {
        painter.save();
        dataLayer(painter);
        painter.restore();
    } else if (!hideRadar) {
        paintRadarLayer(painter);
    }
    // if (nexradState.zoom > 0.9 && !hideRoads) {
    //     nexradDraw.drawGeomLine(HwExtLines);
    // }
    // if (nexradState.zoom > 0.5) {
    //     nexradDraw.drawGeomLine(CountyLines);
    //     if (!hideRoads) {
    //         nexradDraw.drawGeomLine(HwLines);
    //     }
    //     nexradDraw.drawGeomLine(LakeLines);
    // }
    // nexradDraw.drawGeomLine(StateLines);
    // nexradDraw.drawGeomLine(CaLines);
    // nexradDraw.drawGeomLine(MxLines);

    if (nexradState.zoom > 0.7 && !hideRoads) {
        for (auto t : {CountyLines, HwLines, HwExtLines, LakeLines}) {
            nexradDraw.drawGeomLine(t);
        }
    }
    for (auto t : {StateLines, CaLines, MxLines}) {
        nexradDraw.drawGeomLine(t);
    }

    if (RadarPreferences::locationDot) {
        nexradDraw.drawGenericCircles(RadarPreferences::locdotSize, fileStorage.locationDotsColor, fileStorage.locationDotsTransformed);
    }
    if (RadarPreferences::sti) {
        nexradDraw.drawGenericLine(RadarPreferences::stiLinesize, RadarPreferences::colorSti, stormTrackLines);
    }
    if (RadarPreferences::obsWindbarbs && !windBarbCircleColors.empty() && nexradState.zoom > 0.3) {
        nexradDraw.drawGenericLine(RadarPreferences::wbLinesize, Qt::red, wbGustLines);
        nexradDraw.drawGenericLine(RadarPreferences::wbLinesize, RadarPreferences::colorObsWindbarbs, wbLines);
        nexradDraw.drawGenericCircles(RadarPreferences::aviationSize * 2.0, windBarbCircleColors, windBarbCirclesTransformed);
    }
    drawWatch();
    drawWarnings();
    if (RadarPreferences::swo) {
        drawSwo();
    }
    if (RadarPreferences::fire) {
        drawFire();
    }
    if (RadarPreferences::wpcFronts && nexradState.zoom < 0.5) {
        drawWpcFronts();
    }
    // KEEP
    // if (RadarPreferences::locdotFollowsGps) {
    //     for (int i = 0; i < locationDotsTransformedGps.size(); i += 2) {
    //         painter.setPen(QPen(RadarPreferences::colorLocdot, 1.5 / nexradState.zoom, Qt::SolidLine));
    //         painter.setBrush(QBrush(RadarPreferences::colorLocdot, Qt::SolidPattern));
    //         QPointF center = QPointF(locationDotsTransformedGps[i], locationDotsTransformedGps[i + 1]);
    //         painter.drawEllipse(center, scaledCircleSize, scaledCircleSize);
    //     }
    //     for (int i = 0; i < locationDotsTransformedGps.size(); i += 2) {
    //         painter.setPen(QPen(RadarPreferences::colorLocdot, 1.5 / nexradState.zoom, Qt::SolidLine));
    //         painter.setBrush(QBrush(RadarPreferences::colorLocdot, Qt::NoBrush));
    //         QPointF center = QPointF(locationDotsTransformedGps[i], locationDotsTransformedGps[i + 1]);
    //         painter.drawEllipse(center, scaledCircleSize * 6.0, scaledCircleSize * 6.0);
    //     }
    // }
    if (RadarPreferences::hailIndex) {
        nexradDraw.drawTriangles(hiPolygons, RadarPreferences::colorHi);
    }
    if (RadarPreferences::tvs) {
        nexradDraw.drawTriangles(tvsPolygons, RadarPreferences::colorTvs);
    }
    if (RadarPreferences::cities && nexradState.zoom > 0.5) {
        nexradDraw.drawText(RadarPreferences::colorCity, nexradState.cities);
    }
    if (RadarPreferences::countyLabels && nexradState.zoom > 0.9) {
        nexradDraw.drawText(RadarPreferences::colorCountyLabels, nexradState.countyLabels);
    }
    if (RadarPreferences::obs && nexradState.zoom > 0.5) {
        nexradDraw.drawText(RadarPreferences::colorObs, nexradState.observations);
    }
    if (RadarPreferences::colorLegend && nexradState.zoom < 4.0 && !dataLayer) {
        colorLegend.paintEvent(painter, nexradState.zoom, nexradState.xPos, nexradState.yPos);
    }
    if (topLayer) {
        painter.setWorldTransform(QTransform{});   // keep the window mapping: window units, no pan / zoom
        topLayer(painter);
    }
}

void NexradWidget::drawSwo() {
    for (auto riskLevelIndex : range(SwoDayOne::threatList.size())) {
        if (SwoDayOne::polygonBy.contains(riskLevelIndex) && swoLinesMap.contains(riskLevelIndex)) {
            nexradDraw.drawGenericLine(
                RadarPreferences::swoLinesize,
                SwoDayOne::colors[riskLevelIndex],
                swoLinesMap[riskLevelIndex]);
        }
    }
}

void NexradWidget::drawFire() {
    for (auto riskLevelIndex : range(FireDayOne::threatList.size())) {
        if (FireDayOne::polygonBy.contains(riskLevelIndex) && fireLinesMap.contains(riskLevelIndex)) {
            nexradDraw.drawGenericLine(
                RadarPreferences::swoLinesize,
                FireDayOne::colors[riskLevelIndex],
                fireLinesMap[riskLevelIndex]);
        }
    }
}

void NexradWidget::drawWpcFronts() {
    if (nexradState.zoom < 0.5) {
        for (const auto& front : WpcFronts::fronts) {
            if (front.coordinatesModified[nexradState.paneNumber].size() > 1 && front.coordinatesModified[nexradState.paneNumber].size() < 500) {
                nexradDraw.drawGenericLine(
                    RadarPreferences::watmcdLinesize,
                    front.penColor,
                    front.coordinatesModified[nexradState.paneNumber]);
            }
        }
        nexradDraw.drawText(Qt::red, nexradState.pressureCenterLabelsRed);
        nexradDraw.drawText(Qt::blue, nexradState.pressureCenterLabelsBlue);
    }
}

void NexradWidget::drawWarnings() {
    for (const auto type1 : PolygonWarning::polygonList) {
        if (PolygonWarning::byType[type1]->isEnabled && polygons.contains(type1)) {
            nexradDraw.drawGenericLine(
                RadarPreferences::warnLinesize,
                PolygonWarning::byType[type1]->colorInt,
                polygons[type1]);
        }
    }
}

void NexradWidget::drawWatch() {
    for (const auto type1 : PolygonWatch::polygonList) {
        if (PolygonWatch::byType[type1]->isEnabled && polygons.contains(type1)) {
            nexradDraw.drawGenericLine(
                RadarPreferences::watmcdLinesize,
                PolygonWatch::byType[type1]->colorInt,
                polygons[type1]);
        }
    }
}

// KEEP
NexradWidget::~NexradWidget() {
    jobGuard->closeAndWait();   // a download or decode still running uses this widget's members
}

void NexradWidget::runJob(const function<void()>& work, const function<void()>& done) {
    const auto guard = jobGuard;
    const QPointer<NexradWidget> self{this};
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

void NexradWidget::setHistoryTime(const QDateTime& time) {
    historyMs = time.isValid() ? time.toMSecsSinceEpoch() : 0;
}

QDateTime NexradWidget::historyTime() const {
    const auto ms = historyMs.load();
    return ms == 0 ? QDateTime{} : QDateTime::fromMSecsSinceEpoch(ms, Qt::UTC);
}

string NexradWidget::radarInfo() {
    const std::lock_guard<std::mutex> guard{dataLock};
    return levelData.radarInfo;
}

// KEEP
// void NexradWidget::updateGps(double lat, double lon) {
//    gpsX = lat;
//    gpsY = lon;
//    if (RadarPreferences::locdotFollowsGps) {
//        locationDotsTransformedGps.clear();
//        // lat lon are correct pos / neg but must match below
//        auto coords = UtilityCanvasProjection::computeMercatorNumbers(gpsX, -1.0 * gpsY, nexradState.getPn());
//        locationDotsTransformedGps.append(coords);
//    }
// }

void NexradWidget::downloadData() {
    const auto url = NexradDownload::latestFileUrl(nexradState.getRadarSite(), nexradState.getRadarProduct(), historyTime());
    historyMissing = false;
    if (url.empty()) {
        historyMissing = true;   // history asked for a time the bucket has no scan for: the picture on screen stays
        return;
    }
    fileStorage.setMemoryBuffer(UtilityIO::downloadAsByteArray(url));
    // decode into a new set of buffers off to the side and swap them in: the UI thread keeps painting the old picture meanwhile
    NexradLevelData fresh{&nexradState, &fileStorage};
    const auto bins = fresh.decodeAndGenerateRadials();
    const std::lock_guard<std::mutex> guard{dataLock};
    levelData = std::move(fresh);
    levelData.rebind(&nexradState, &fileStorage);
    totalBins = bins;
    dataVersion += 1;
}

void NexradWidget::constructWBLines() {
    if (RadarPreferences::obs) {
        nexradRenderTextObject.addTextLabelsObservations();
    }
    if (RadarPreferences::obsWindbarbs) {
        windBarbCirclesTransformed.clear();
        windBarbCircleColors.clear();
        wbLines.clear();
        wbGustLines.clear();
        const auto wBFloats = NexradLevel3WindBarbs::decodeAndPlot(nexradState.getPn(), false, fileStorage);
        for (auto x : range3(0, wBFloats.size(), 4)) {
            wbLines.push_back(QLineF{wBFloats[x], wBFloats[x + 1], wBFloats[x + 2], wBFloats[x + 3]});
        }
        const auto wBGustFloats = NexradLevel3WindBarbs::decodeAndPlot(nexradState.getPn(), true, fileStorage);
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
            windBarbCirclesTransformed.push_back(Projection::computeMercatorNumbers(obsX[index], obsY[index], nexradState.getPn()));
            windBarbCircleColors.emplace_back(Color::red(rawColor), Color::green(rawColor), Color::blue(rawColor));
        }
    }
}

void NexradWidget::process(PolygonType polygonType) {
    const auto numbers = Watch::add(nexradState.getPn(), polygonType);
    polygons[polygonType] = QVector<QLineF>();
    for (auto position : range3(0, numbers.size(), 4)) {
        polygons[polygonType].push_back(QLineF(numbers[position], numbers[position + 1], numbers[position + 2], numbers[position + 3]));
    }
}

void NexradWidget::constructSwo() {
    for (auto riskLevelIndex : range(SwoDayOne::threatList.size())) {
        if (SwoDayOne::polygonBy.contains(riskLevelIndex)) {
            swoLinesMap[riskLevelIndex] = QVector<QLineF>();
            for (auto x : range3(0, SwoDayOne::polygonBy[riskLevelIndex].size(), 4)) {
                const auto floatList = SwoDayOne::polygonBy[riskLevelIndex];
                const auto coords1 = Projection::computeMercatorNumbers(floatList[x], floatList[x + 1], nexradState.getPn());
                const auto coords2 = Projection::computeMercatorNumbers(floatList[x + 2], floatList[x + 3], nexradState.getPn());
                swoLinesMap[riskLevelIndex].push_back(QLineF{coords1[0], coords1[1], coords2[0], coords2[1]});
            }
        } else {
            if (swoLinesMap.contains(riskLevelIndex)) {
                swoLinesMap[riskLevelIndex].clear();
            }
        }
    }
}

void NexradWidget::constructFire() {
    for (auto riskLevelIndex : range(FireDayOne::threatList.size())) {
        if (FireDayOne::polygonBy.contains(riskLevelIndex)) {
            fireLinesMap[riskLevelIndex] = QVector<QLineF>();
            for (auto x : range3(0, FireDayOne::polygonBy[riskLevelIndex].size(), 4)) {
                const auto floatList = FireDayOne::polygonBy[riskLevelIndex];
                const auto coords1 = Projection::computeMercatorNumbers(floatList[x], floatList[x + 1], nexradState.getPn());
                const auto coords2 = Projection::computeMercatorNumbers(floatList[x + 2], floatList[x + 3], nexradState.getPn());
                fireLinesMap[riskLevelIndex].push_back(QLineF{coords1[0], coords1[1], coords2[0], coords2[1]});
            }
        } else {
            if (fireLinesMap.contains(riskLevelIndex)) {
                fireLinesMap[riskLevelIndex].clear();
            }
        }
    }
}

void NexradWidget::constructWpcFronts() {
    for (auto& front : WpcFronts::fronts) {
        front.translate(nexradState.paneNumber, nexradState.getPn());
    }
    nexradRenderTextObject.addWpcPressureCenters();
}

void NexradWidget::constructSti() {
    stormTrackLines.clear();
    vector<double> floats;
    {
        const std::lock_guard<std::mutex> guard{*fileStorage.lock};
        floats = fileStorage.stiData;
    }
    for (auto x : range3(0, floats.size(), 4)) {
        stormTrackLines.push_back(QLineF{floats[x], floats[x + 1], floats[x + 2], floats[x + 3]});
    }
}

void NexradWidget::resizePolygons() {
    constructHi();
    constructTvs();
}

void NexradWidget::constructHi() {
    vector<double> floats;
    {
        const std::lock_guard<std::mutex> guard{*fileStorage.lock};
        floats = fileStorage.hiData;
    }
    const auto lengthOrig = 5.0;
    const auto length = lengthOrig / nexradState.zoom;
    hiPolygons.clear();
    for (auto x : range3(0, floats.size(), 2)) {
        const auto point0 = Projection::computeMercatorNumbers(floats[x], floats[x + 1], nexradState.getPn());
        const QPointF point1{point0[0], point0[1]};
        const QPointF point2{point0[0] - length, point0[1] - length};
        const QPointF point3{point0[0] + length, point0[1] - length};
        QPolygonF polygon;
        polygon.push_back(point1);
        polygon.push_back(point2);
        polygon.push_back(point3);
        hiPolygons.push_back(polygon);
    }
}

void NexradWidget::constructTvs() {
    vector<double> floats;
    {
        const std::lock_guard<std::mutex> guard{*fileStorage.lock};
        floats = fileStorage.tvsData;
    }
    const auto lengthOrig = 5.0;
    const auto length = lengthOrig / nexradState.zoom;
    tvsPolygons.clear();
    for (auto x : range3(0, floats.size(), 2)) {
        const auto point0 = Projection::computeMercatorNumbers(floats[x], floats[x + 1], nexradState.getPn());
        const QPointF point1{point0[0], point0[1]};
        const QPointF point2{point0[0] - length, point0[1] - length};
        const QPointF point3{point0[0] + length, point0[1] - length};
        QPolygonF polygon;
        polygon.push_back(point1);
        polygon.push_back(point2);
        polygon.push_back(point3);
        tvsPolygons.push_back(polygon);
    }
}

void NexradWidget::downloadDataForAnimation(int index) {
    {
        const std::lock_guard<std::mutex> guard{dataLock};
        if (index < 0 || static_cast<size_t>(index) >= nexradStateAnimation.levelDataList.size()) {
            return;   // fewer frames came back than were asked for
        }
        levelData = nexradStateAnimation.levelDataList[static_cast<size_t>(index)];
        levelData.rebind(&nexradState, &fileStorage);   // the frames were decoded against a scratch file store
        totalBins = levelData.totalBins;
        dataVersion += 1;
    }
    updateTitle();
}

void NexradWidget::processWarnings(PolygonType polygonGenericType) {
    const auto numbers = Warnings::add(nexradState.getPn(), polygonGenericType);
    polygons[polygonGenericType] = QVector<QLineF>();
    for (auto position : range3(0, numbers.size(), 4)) {
        polygons[polygonGenericType].push_back(QLineF{numbers[position], numbers[position + 1], numbers[position + 2], numbers[position + 3]});
    }
}

void NexradWidget::updateTitle() {
    setTitleMain();
    const std::lock_guard<std::mutex> guard{dataLock};
    // the status line names the product on screen: a super-resolution file (N0Q's N0B, N0U's N0G) says so
    auto product = nexradState.getRadarProduct();
    if (product.size() == 3 && ((levelData.radarBuffers.productCode == 153 && product[2] == 'Q') ||
                                (levelData.radarBuffers.productCode == 154 && product[2] == 'U'))) {
        product[2] = levelData.radarBuffers.productCode == 153 ? 'B' : 'G';
    }
    string historyLabel;
    if (historyTime().isValid() && levelData.scanEpochSec != 0) {
        historyLabel = QDateTime::fromSecsSinceEpoch(levelData.scanEpochSec, Qt::UTC).toString("yyyy-MM-dd HH:mm 'UTC'").toStdString();
    }
    radarStatusBox->setBox(levelData, product, nexradState.getRadarSite(), historyLabel);
}

void NexradWidget::changeProduct() {
    // nexradState.setRadarProduct(WString::split(product, ":")[0]);
    // FIXME TODO javafx does not do legend here
    // colorLegend.update(nexradState.getRadarProduct());
    nexradState.writePreferences();
    runJob([this] { downloadData(); }, [this] { draw(); });
}

void NexradWidget::toggleRadar() {
    toggleIndex += 1;
    switch (toggleIndex) {
        case 0:
            hideRadar = false;
            hideRoads = false;
            break;
        case 1:
            hideRadar = true;
            hideRoads = false;
            break;
        case 2:
            hideRadar = false;
            hideRoads = true;
            break;
        default:
            toggleIndex = 0;
            hideRadar = false;
            hideRoads = false;
            break;
    }
    draw();
}

void NexradWidget::draw() {
    updateTitle();
    update();
}
