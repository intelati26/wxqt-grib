// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef MAPDRAW_H
#define MAPDRAW_H

#include <vector>
#include <QLineF>
#include <QPaintEvent>
#include <QPolygonF>
#include <QVector>
#include "objects/FileStorage.h"
#include "radar/MapState.h"
#include "radar/RadarGeometryTypeEnum.h"
#include "radar/MapTextObject.h"
#include "ui/TextViewMetal.h"

using std::vector;

class MapDraw {
public:
    MapDraw(MapState *, FileStorage *, MapTextObject *);
    void initGeom();
    void convertGeomData(RadarGeometryTypeEnum type);
    void initSurface(QPainter *, QPaintEvent *);
    void drawGenericCircles(double, const vector<QColor>&, const vector<vector<double>>&);
    void drawGenericLine(double, const QColor&, const QVector<QLineF>&);
    void drawGeomLine(RadarGeometryTypeEnum);
    void drawTriangles(vector<QPolygonF>&, const QColor&);
    void drawText(const QColor&, const vector<TextViewMetal>&);

private:
    MapState * mapState;
    FileStorage * fileStorage;
    MapTextObject * textObject;
    QPainter * painter;
};

#endif  // MAPDRAW_H
