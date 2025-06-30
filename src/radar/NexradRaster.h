// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef NEXRADRASTER_H
#define NEXRADRASTER_H

#include "radar/RadarBuffers.h"

class NexradRaster {
public:
    static int create(RadarBuffers&);
};

#endif  // NEXRADRASTER_H
