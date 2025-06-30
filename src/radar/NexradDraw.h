// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef NEXRADDRAW_H
#define NEXRADDRAW_H

#include <vector>
#include <QLineF>
#include <QPaintEvent>
#include <QPolygonF>
#include <QVector>
#include "objects/FileStorage.h"
#include "radar/NexradState.h"
#include "radar/RadarGeometryTypeEnum.h"
#include "radar/NexradRenderTextObject.h"
#include "ui/TextViewMetal.h"

using std::vector;

class NexradDraw {
public:
    NexradDraw(NexradState *, FileStorage *, NexradRenderTextObject *);
    void initGeom();
    void convertGeomData(RadarGeometryTypeEnum type);
    void initSurface(QPainter *, QPaintEvent *);
    void drawGenericCircles(double, const vector<QColor>&, const vector<vector<double>>&);
    void drawGenericLine(double, const QColor&, const QVector<QLineF>&);
    void drawGeomLine(RadarGeometryTypeEnum);
    void drawTriangles(vector<QPolygonF>&, const QColor&);
    void drawText(const QColor&, const vector<TextViewMetal>&);

private:
    NexradState * nexradState;
    FileStorage * fileStorage;
    NexradRenderTextObject * textObject;
    QPainter * painter;
};

#endif  // NEXRADDRAW_H
