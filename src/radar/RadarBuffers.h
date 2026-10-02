// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef RADARBUFFERS_H
#define RADARBUFFERS_H

#include <array>
#include <cstdint>
#include <initializer_list>
#include <vector>
#include <QBrush>
#include <QColor>
#include <QPen>
#include <QPointF>
#include "objects/MemoryBuffer.h"

using std::vector;

class RadarBuffers {
public:
    RadarBuffers();
    void initialize();
    void setBackgroundColor();
    // one filled quadrilateral (a run of bins of one level in one radial) in map units; level picks its colour from the palette
    void addQuad(std::initializer_list<QPointF> corners, int level);
    struct Quad {
        float x[4];
        float y[4];
    };
    int animationIndex{-1};
    uint16_t numberOfRadials{};
    uint16_t numberOfRangeBins{};
    double binSize{};
    uint16_t productCode{};
    vector<Quad> quads;                           // 32 bytes each (a QPolygonF per bin cost about 130)
    vector<uint8_t> levels;                       // the colour level of each quad
    const QBrush& brushOf(int level) const { return brushOfLevel[static_cast<size_t>(level)]; }
    MemoryBuffer radialStartAngle;
    MemoryBuffer binWord;

private:
    std::array<QBrush, 256> brushOfLevel;         // made on first use
    std::array<bool, 256> brushMade{};
};

#endif  // RADARBUFFERS_H
