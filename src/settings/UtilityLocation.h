// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef UTILITYLOCATION_H
#define UTILITYLOCATION_H

#include <string>
#include <unordered_map>
#include <vector>
#include "objects/LatLon.h"

using std::string;
using std::unordered_map;
using std::vector;

class UtilityLocation {
public:
    static LatLon getCenterOfPolygon(const vector<LatLon>&);
    static string getNearest(const LatLon&, const unordered_map<string, LatLon>&);
    static string getNearestCity(const LatLon&);
};

#endif  // UTILITYLOCATION_H
