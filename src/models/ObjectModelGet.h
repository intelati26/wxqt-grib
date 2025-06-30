// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef OBJECTMODELGET_H
#define OBJECTMODELGET_H

#include <string>
#include "ObjectModel.h"

using std::string;

class ObjectModelGet {
public:
    static void runStatus(ObjectModel&);
    static string imageUrl(ObjectModel&);
};

#endif  // OBJECTMODELGET_H
