// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef MAPWINDBARBS_H
#define MAPWINDBARBS_H

#include <vector>
#include "objects/FileStorage.h"
#include "radar/ProjectionNumbers.h"

using std::vector;

class MapWindBarbs {
public:
    static vector<double> decodeAndPlot(const ProjectionNumbers&, bool, FileStorage&);
};

#endif  // MAPWINDBARBS_H
