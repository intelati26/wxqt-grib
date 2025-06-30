// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef NEXRADLEVEL3HAILINDEX_H
#define NEXRADLEVEL3HAILINDEX_H

#include <string>
#include "objects/FileStorage.h"
#include "radar/ProjectionNumbers.h"

using std::string;

class NexradLevel3HailIndex {
public:
    static void decode(const ProjectionNumbers&, FileStorage&);

private:
    static const string pattern;
};

#endif  // NEXRADLEVEL3HAILINDEX_H
