// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef NEXRADDECODEFOURBIT_H
#define NEXRADDECODEFOURBIT_H

#include "objects/FileStorage.h"
#include "radar/RadarBuffers.h"

class NexradDecodeFourBit {
public:
    static int radial(RadarBuffers&, FileStorage *);
    static int raster(RadarBuffers&, FileStorage *);
};

#endif  // NEXRADDECODEFOURBIT_H
