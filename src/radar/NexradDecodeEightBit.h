// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef NEXRADDECODEEIGHTBIT_H
#define NEXRADDECODEEIGHTBIT_H

#include "objects/FileStorage.h"
#include "radar/RadarBuffers.h"

class NexradDecodeEightBit {
public:
    static int andCreateRadials(RadarBuffers&, FileStorage *);
    static int createRadials(RadarBuffers&);
};

#endif  // NEXRADDECODEEIGHTBIT_H
