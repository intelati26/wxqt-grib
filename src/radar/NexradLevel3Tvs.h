// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef NEXRADLEVEL3TVS_H
#define NEXRADLEVEL3TVS_H

#include "objects/FileStorage.h"
#include "radar/ProjectionNumbers.h"

class NexradLevel3Tvs {
public:
    static void decode(const ProjectionNumbers&, FileStorage&);
};

#endif  // NEXRADLEVEL3TVS_H
