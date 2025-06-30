// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "NexradLevelData.h"
#include "common/GlobalDictionaries.h"
#include "objects/ObjectDateTime.h"
#include "radar/NexradState.h"
#include "radar/NexradDecodeEightBit.h"
#include "radar/NexradDecodeFourBit.h"
#include "radar/NexradRaster.h"
#include "radar/NexradUtil.h"
#include "util/To.h"
#include "util/UtilityList.h"

NexradLevelData::NexradLevelData(NexradState * nexradState, FileStorage * fileStorge)
    : nexradState{nexradState}
    , fileStorage{fileStorge}
    , productCode{nexradState->getRadarProductId()}
{}

void NexradLevelData::decode() {
    productCode = nexradState->getRadarProductId();
    if (contains({30, 37, 38, 41, 56, 57, 78, 80, 181}, productCode)) {
        decodeAndPlotNexradLevel3FourBit();
    } else {
        decodeAndPlotNexradLevel3();
    }
}

void NexradLevelData::generateRadials() {
    productCode = nexradState->getRadarProductId();
    if (contains({37, 38}, productCode)) {
        totalBins = NexradRaster::create(radarBuffers);
    } else if (contains({30, 56, 78, 80, 181}, productCode)) {
        totalBins = NexradDecodeEightBit::createRadials(radarBuffers);
    } else if (productCode == 0) {
        totalBins = 0;
    } else {
        totalBins = NexradDecodeEightBit::andCreateRadials(radarBuffers, fileStorage);
    }
}

void NexradLevelData::decodeAndPlotNexradLevel3() {
    MemoryBuffer * dis;
    if (radarBuffers.animationIndex == -1) {
        dis = &fileStorage->memoryBuffer;
    } else {
        dis = &fileStorage->animationMemoryBuffer[radarBuffers.animationIndex];
    }
    if (dis->getCapacity() > 300) {
        dis->setPosition(0);
        while (dis->getShort() != -1) {}
        latitudeOfRadar = dis->getInt() / 1000.0;
        longitudeOfRadar = dis->getInt() / 1000.0;
        radarHeight = dis->getUnsignedShort();
        productCode = dis->getUnsignedShort();
        operationalMode = dis->getUnsignedShort();
        volumeCoveragePattern = dis->getUnsignedShort();
        sequenceNumber = dis->getUnsignedShort();
        volumeScanNumber = dis->getUnsignedShort();
        volumeScanDate = dis->getUnsignedShort();
        volumeScanTime = dis->getInt();
        writeTime(volumeScanDate, volumeScanTime);
        dis->skipBytes(10);
        elevationNumber = dis->getUnsignedShort();
        elevationAngle = dis->getShort();
        degree = elevationAngle / 10.0;
        halfWord3132 = dis->getFloat();
        NexradUtil::wxoglDspLegendMax = (255.0 / halfWord3132) * 0.01;
        dis->skipBytes(26);
        dis->skipBytes(30);
        seekStart = dis->getPosition();
        binSize = NexradUtil::getBinSize(productCode);
        numberOfRangeBins = NexradUtil::getNumberRangeBins(productCode);
        numberOfRadials = 360;
        if (productCode == 153 || productCode == 154) {
            numberOfRadials = 720;
        }
        radarBuffers.numberOfRangeBins = numberOfRangeBins;
        radarBuffers.numberOfRadials = numberOfRadials;
        radarBuffers.binSize = binSize;
        radarBuffers.productCode = productCode;
    }
}

void NexradLevelData::decodeAndPlotNexradLevel3FourBit() {
    switch (productCode) {
        case 181:
            radarBuffers.binWord = MemoryBuffer{360 * 720};
            break;
        case 78:
        case 80:
            radarBuffers.binWord = MemoryBuffer{360 * 592};
            break;
        case 37:
        case 38:
            radarBuffers.binWord = MemoryBuffer{464 * 464};
            break;
        default:
            radarBuffers.binWord = MemoryBuffer{360 * 230};
            break;
    }
    radarBuffers.radialStartAngle = MemoryBuffer{4 * 360};
    MemoryBuffer * dis;
    if (radarBuffers.animationIndex == -1) {
        dis = &fileStorage->memoryBuffer;
    } else {
        dis = &fileStorage->animationMemoryBuffer[radarBuffers.animationIndex];
    }
    if (dis->getCapacity() > 0) {
        dis->setPosition(0);
        dis->skipBytes(58);
        radarHeight = dis->getUnsignedShort();
        productCode = dis->getUnsignedShort();
        operationalMode = dis->getUnsignedShort();
        volumeCoveragePattern = dis->getUnsignedShort();
        sequenceNumber = dis->getUnsignedShort();
        volumeScanNumber = dis->getUnsignedShort();
        volumeScanDate = dis->getUnsignedShort();
        volumeScanTime = dis->getInt();
        writeTime(volumeScanDate, volumeScanTime);
        dis->skipBytes(94);
        if (productCode == 37 || productCode == 38 || productCode == 41 || productCode == 57) {
            numberOfRangeBins = NexradDecodeFourBit::raster(radarBuffers, fileStorage);
        } else {
            numberOfRangeBins = NexradDecodeFourBit::radial(radarBuffers, fileStorage);
        }
        binSize = NexradUtil::getBinSize(productCode);
        numberOfRadials = 360;
    } else {
        numberOfRangeBins = 230;
        numberOfRadials = 360;
    }
    radarBuffers.numberOfRangeBins = numberOfRangeBins;
    radarBuffers.numberOfRadials = numberOfRadials;
    radarBuffers.binSize = binSize;
    radarBuffers.productCode = productCode;
}

void NexradLevelData::writeTime(uint16_t volumeScanDate, int volumeScanTime) {
    const auto radarInfo = "Mode: "
        + To::string(operationalMode)
        + ", VCP: "
        + To::string(volumeCoveragePattern)
        + ", " + "Product: "
        + To::string(productCode)
        + ", " + "Height: "
        + To::string(radarHeight);
    const int64_t sec = (((volumeScanDate) - 1) * 3600 * 24) + volumeScanTime;
    const auto dateString = ObjectDateTime::getTimeFromPointAsString(sec);
    const auto radarInfoFinal = dateString + " " + radarInfo;
    this->radarInfo = radarInfoFinal;
    radarAgeMilli = static_cast<int>(ObjectDateTime::currentTimeMillis() - sec * 1000);
}

int NexradLevelData::decodeAndGenerateRadials() {
    radarBuffers.animationIndex = -1;
    decode();
    radarBuffers.initialize();
    generateRadials();
    return totalBins;
}
