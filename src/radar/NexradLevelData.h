// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef NEXRADLEVELDATA_H
#define NEXRADLEVELDATA_H

#include <cstdint>
#include <string>
#include "objects/FileStorage.h"
#include "radar/NexradState.h"
#include "radar/RadarBuffers.h"

using std::string;

class NexradLevelData {
public:
    NexradLevelData(NexradState *, FileStorage *);
    void decode();
    void generateRadials();
    int decodeAndGenerateRadials();
    // after the data has been moved in from another object (a decode done off to the side): point at this widget's state again
    void rebind(NexradState * state, FileStorage * storage) { nexradState = state; fileStorage = storage; }
    double binSize{};
    int numberOfRangeBins{916};
    int numberOfRadials{360};
    RadarBuffers radarBuffers;
    int radarHeight{};
    double degree{};
    uint16_t operationalMode{};
    uint16_t volumeCoveragePattern{};
    int16_t elevationAngle{};
    int totalBins{};
    string radarInfo;
    int radarAgeMilli{};

private:
    void decodeAndPlotNexradLevel3();
    void decodeAndPlotNexradLevel3FourBit();
    void writeTime(uint16_t, int);
    NexradState * nexradState;
    FileStorage * fileStorage;
    uint16_t productCode{};
    double latitudeOfRadar{};
    double longitudeOfRadar{};
    uint16_t sequenceNumber{};
    uint16_t volumeScanNumber{};
    uint16_t elevationNumber{};
    float halfWord3132{};
    int64_t seekStart{};
    uint16_t volumeScanDate{};
    int volumeScanTime{};
};

#endif  // NEXRADLEVELDATA_H
