// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef SWODAYONE_H
#define SWODAYONE_H

#include <string>
#include <unordered_map>
#include <vector>
#include <QColor>
#include "objects/DownloadTimer.h"

using std::string;
using std::unordered_map;
using std::vector;

class SwoDayOne {
public:
    static void get();
    static DownloadTimer timer;
    static unordered_map<int, vector<double>> polygonBy;
    static const vector<QColor> colors;
    static const vector<string> threatList;
};

#endif  // SWODAYONE_H
