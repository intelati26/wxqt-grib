// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef METAR_H
#define METAR_H

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include "objects/FileStorage.h"
#include "objects/LatLon.h"
#include "objects/Site.h"
#include "objects/Sites.h"

using std::string;
using std::unordered_map;
using std::unique_ptr;
using std::vector;

class Metar {
public:
    static void initialize();
    static void getStateMetarArrayForWXOGL(const string&, FileStorage&);
    static Site findClosestObservation(const LatLon&, int = 0);
    static unique_ptr<Sites> sites;

private:
    static string getNearbyObsSites(const string&);
    static vector<string> condense(const vector<string>&);
};

#endif  // METAR_H
