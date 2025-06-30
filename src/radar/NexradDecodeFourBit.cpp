// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "NexradDecodeFourBit.h"
// #include <cmath>
// #include <numbers>
#include "objects/MemoryBuffer.h"
#include "util/UtilityIO.h"
#include "util/UtilityList.h"

int NexradDecodeFourBit::radial(RadarBuffers& radarBuffers, FileStorage * fileStorage) {
    MemoryBuffer * dis;
    if (radarBuffers.animationIndex == -1) {
        dis = &fileStorage->memoryBuffer;
    } else {
        dis = &fileStorage->animationMemoryBuffer[radarBuffers.animationIndex];
    }
    dis->setPosition(0);
    if (dis->getCapacity() > 0) {
        dis->skipBytes(170);
        const auto numberOfRangeBins = dis->getUnsignedShort();
        dis->skipBytes(6);
        dis->getUnsignedShort();
        radarBuffers.radialStartAngle.setPosition(0);
        radarBuffers.binWord.setPosition(0);
        for ([[maybe_unused]] auto i : range(360)) {
            auto numberOfRleHalfWord = dis->getUnsignedShort();
            radarBuffers.radialStartAngle.putFloat(450.0 - (dis->getUnsignedShort() / 10.0));
            dis->skipBytes(2);
            for (auto unused1 = 0; unused1 < numberOfRleHalfWord * 2; unused1++) {
                const auto bin = dis->get();
                const auto numOfBins = static_cast<int>(bin >> 4);
                for (auto unused2 = 0; unused2 < numOfBins; unused2++) {
                    radarBuffers.binWord.put(static_cast<int>(bin % 16));
                }
            }
        }
        return numberOfRangeBins;
    } else {
        return 230;
    }
}

int NexradDecodeFourBit::raster(RadarBuffers& radarBuffers, FileStorage * fileStorage) {
    MemoryBuffer * dis;
    if (radarBuffers.animationIndex == -1) {
        dis = &fileStorage->memoryBuffer;
    } else {
        dis = &fileStorage->animationMemoryBuffer[radarBuffers.animationIndex];
    }
    dis->setPosition(0);
    if (dis->getCapacity() > 0) {
        dis->skipBytes(172);
        // let iCoordinateStart = dis.getUnsignedShort()
        // let jCoordinateStart = dis.getUnsignedShort()
        // let xScaleInt = dis.getUnsignedShort()
        // let xScaleFractional = dis.getUnsignedShort()
        // let yScaleInt = dis.getUnsignedShort()
        // let yScaleFractional = dis.getUnsignedShort()
        // let numberOfRows = dis.getUnsignedShort()
        // let packingDescriptor = dis.getUnsignedShort()
        dis->skipBytes(12);
        auto numberOfRows = dis->getUnsignedShort();
        //auto packingDescriptor = dis->getUnsignedShort();
        dis->skipBytes(2);
        // 464 rows in NCR
        // 232 rows in NCZ
        for ([[maybe_unused]] auto radial : range(numberOfRows)) {
            const auto numberOfBytes = dis->getUnsignedShort();
            for (auto i0 = 0; i0 < numberOfBytes; i0 += 1) {
                const auto bin = dis->get();
                const auto numOfBins = static_cast<int>(bin >> 4);
                for (auto i1 = 0; i1 < numOfBins; i1 += 1) {
                    radarBuffers.binWord.put(bin % 16);
                }
            }
        }
    }
    return 0;
}
