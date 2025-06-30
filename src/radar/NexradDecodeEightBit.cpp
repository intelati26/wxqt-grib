// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "NexradDecodeEightBit.h"
#include <numbers>
#include <cmath>
#include <QPointF>
#include <QPolygonF>
#include <QVector>
#include "objects/MemoryBuffer.h"
#include "util/UtilityIO.h"
#include "util/UtilityList.h"

int NexradDecodeEightBit::andCreateRadials(RadarBuffers& radarBuffers, FileStorage * fileStorage) {
    const auto k180DivPi{180.0 / std::numbers::pi};
    auto totalBins = 0;
    MemoryBuffer * disFirst;
    if (radarBuffers.animationIndex == -1) {
        disFirst = &fileStorage->memoryBuffer;
    } else {
        disFirst = &fileStorage->animationMemoryBuffer[radarBuffers.animationIndex];
    }
    disFirst->setPosition(0);
    if (disFirst->getCapacity() < 500) {
        return 0;
    }
    while (disFirst->getShort() != -1) {}
    disFirst->skipBytes(100);
    auto dis2 = UtilityIO::uncompress(disFirst->getBackingPointer() + disFirst->getPosition(), disFirst->getCapacity() - disFirst->getPosition());
    dis2.skipBytes(30);
    radarBuffers.setBackgroundColor();
    auto angleNext{0.0};
    auto angle0{0.0};
    const auto numberOfRadials{radarBuffers.numberOfRadials};
    const auto yShift{-1.0};
    for (auto radial : range(numberOfRadials)) {
        const auto numberOfRleHalfWords{dis2.getUnsignedShort()};
        const auto angle{450.0 - (dis2.getUnsignedShort() / 10.0)};
        dis2.skipBytes(2);
        if (radial < static_cast<size_t>(numberOfRadials - 1)) {
            dis2.mark(dis2.getPosition());
            dis2.skipBytes(static_cast<int>(numberOfRleHalfWords) + 2);
            angleNext = 450.0 - (dis2.getUnsignedShort() / 10.0);
            dis2.reset();
        }
        auto level{0};
        auto levelCount{0};
        auto binStart{radarBuffers.binSize};
        if (radial == 0) {
            angle0 = angle;
        }
        auto angleV{angleNext};
        if (radial >= static_cast<size_t>(numberOfRadials - 1)) {
            angleV = angle0;
        }
        const auto angleVCos{cos(angleV / k180DivPi)};
        const auto angleVSin{sin(angleV / k180DivPi)};
        const auto angleCos{cos(angle / k180DivPi)};
        const auto angleSin{sin(angle / k180DivPi)};
        for (auto bin = 0; bin < numberOfRleHalfWords; bin += 1) {
            const auto curLevel{static_cast<int>(dis2.get())};
            if (bin == 0) {
                level = curLevel;
            }
            if (curLevel == level) {
                levelCount += 1;
            } else {
                radarBuffers.rectPoints.emplace_back(
                    QVector<QPointF>{
                        QPointF{binStart * angleVCos, yShift * binStart * angleVSin},
                        QPointF{(binStart + radarBuffers.binSize * levelCount) * angleVCos, yShift * (binStart + radarBuffers.binSize * levelCount) * angleVSin},
                        QPointF{(binStart + radarBuffers.binSize * levelCount) * angleCos, yShift * (binStart + radarBuffers.binSize * levelCount) * angleSin},
                        QPointF{binStart * angleCos, yShift * binStart * angleSin}});
                radarBuffers.putColorsByIndex(level);
                totalBins += 1;
                level = curLevel;
                binStart = bin * radarBuffers.binSize;
                levelCount = 1;
            }
        }
    }
    return totalBins;
}

int NexradDecodeEightBit::createRadials(RadarBuffers& radarBuffers) {
    const auto k180DivPi{180.0 / std::numbers::pi};
    auto totalBins{0};
    auto bI{0};
    auto radarBlackHole{4.0};
    auto radarBlackHoleAdd{4.0};
    radarBuffers.setBackgroundColor();
    radarBuffers.radialStartAngle.setPosition(0);
    radarBuffers.binWord.setPosition(0);
    const auto yShift{-1.0};
    switch (radarBuffers.productCode) {
        case 19:
        case 30:
        case 56:
            radarBlackHole = 1.0;
            radarBlackHoleAdd = 0.0;
            break;
        default:
            break;
    }
    for (auto g : range(radarBuffers.numberOfRadials)) {
        // since radial_start is constructed natively as opposed to read in
        // from bigendian file we have to use getFloatNative
        const auto angle{radarBuffers.radialStartAngle.getFloatByIndex(g * 4)};
        auto level{static_cast<int>(radarBuffers.binWord.getByIndex(bI))};
        auto levelCount{0};
        auto binStart{radarBlackHole};
        auto angleV = radarBuffers.radialStartAngle.getFloatByIndex(g * 4 + 4);
        if (g >= static_cast<size_t>(radarBuffers.numberOfRadials - 1)) {
            angleV = radarBuffers.radialStartAngle.getFloatByIndex(0);
        }
        const auto angleVCos{cos(angleV / k180DivPi)};
        const auto angleVSin{sin(angleV / k180DivPi)};
        const auto angleCos{cos(angle / k180DivPi)};
        const auto angleSin{sin(angle / k180DivPi)};
        for (auto bin = 0; bin < radarBuffers.numberOfRangeBins; bin += 1) {
            const auto curLevel{static_cast<int>(radarBuffers.binWord.getByIndex(bI))};
            bI += 1;
            if (curLevel == level) {
                levelCount += 1;
            } else {
                radarBuffers.rectPoints.emplace_back(
                    QVector<QPointF>{
                        QPointF{binStart * angleVCos, yShift * binStart * angleVSin},
                        QPointF{(binStart + radarBuffers.binSize * levelCount) * angleVCos, yShift * (binStart + radarBuffers.binSize * levelCount) * angleVSin},
                        QPointF{(binStart + radarBuffers.binSize * levelCount) * angleCos, yShift * (binStart + radarBuffers.binSize * levelCount) * angleSin},
                        QPointF{binStart * angleCos, yShift * binStart * angleSin}});
                radarBuffers.putColorsByIndex(level);
                totalBins += 1;
                level = curLevel;
                binStart = bin * radarBuffers.binSize + radarBlackHoleAdd;
                levelCount = 1;
            }
        }
    }
    return totalBins;
}
