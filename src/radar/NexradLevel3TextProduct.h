// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef NEXRADLEVEL3TEXTPRODUCT_H
#define NEXRADLEVEL3TEXTPRODUCT_H

#include <string>
#include "objects/FileStorage.h"
#include "radar/ProjectionNumbers.h"

using std::string;

class NexradLevel3TextProduct {
public:
    static string download(const string&, const string&);
};

#endif  // NEXRADLEVEL3TEXTPRODUCT_H
