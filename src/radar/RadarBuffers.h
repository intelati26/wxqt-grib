// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef RADARBUFFERS_H
#define RADARBUFFERS_H

#include <cstdint>
#include <vector>
#include <QBrush>
#include <QColor>
#include <QPen>
#include <QPolygonF>
#include "objects/MemoryBuffer.h"

using std::vector;

class RadarBuffers {
public:
    RadarBuffers();
    void initialize();
    void setBackgroundColor();
    void putColorsByIndex(int);
    int animationIndex{-1};
    uint16_t numberOfRadials{};
    uint16_t numberOfRangeBins{};
    double binSize{};
    uint16_t productCode{};
    vector<QColor> color;
    vector<QPolygonF> rectPoints;
    vector<QPen> colorPens;
    vector<QBrush> colorBrushes;
    MemoryBuffer radialStartAngle;
    MemoryBuffer binWord;
};

#endif  // RADARBUFFERS_H
