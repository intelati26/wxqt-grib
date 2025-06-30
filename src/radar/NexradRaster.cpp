// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "NexradRaster.h"
#include <QPointF>
#include <QPolygonF>
#include <QVector>
#include "radarcolorpalette/ColorPalette.h"
#include "util/UtilityList.h"

int NexradRaster::create(RadarBuffers& radarBuffers) {
    auto totalBins = 0;
    auto numberOfRows = 464;
    auto binsPerRow = 464;
    auto scaleFactor = 2.0;
    switch (radarBuffers.productCode) {
        case 38:
            numberOfRows = 232;
            binsPerRow = 232;
            scaleFactor = 8.0;
            break;
        case 41:
        case 57:
            numberOfRows = 116;
            binsPerRow = 116;
            scaleFactor = 8.0;
            break;
        default:
            break;
    }
    radarBuffers.setBackgroundColor();
    const auto halfPoint = numberOfRows / 2.0;
    for (auto g : range(numberOfRows)) {
        for (auto bin : range(binsPerRow)) {
            const auto curLevel = radarBuffers.binWord.getByIndex(g * binsPerRow + bin);
            radarBuffers.rectPoints.emplace_back(QVector<QPointF>{
                QPointF{(bin - halfPoint) * scaleFactor, (g - halfPoint) * scaleFactor},
                QPointF{(bin - halfPoint) * scaleFactor, (g + 1.0 - halfPoint) * scaleFactor},
                QPointF{(bin + 1.0 - halfPoint) * scaleFactor, (g + 1.0 - halfPoint) * scaleFactor},
                QPointF{(bin + 1.0 - halfPoint) * scaleFactor, (g - halfPoint) * scaleFactor}});
            radarBuffers.putColorsByIndex(curLevel);
            totalBins += 1;
        }
    }
    return totalBins;
}
