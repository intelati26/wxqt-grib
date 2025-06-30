// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef RADARGEOMINFO_H
#define RADARGEOMINFO_H

#include <string>
#include <unordered_map>
#include <vector>
#include <QColor>
#include "radar/RadarGeometryTypeEnum.h"

using std::string;
using std::unordered_map;
using std::vector;

class RadarGeomInfo {
public:
    explicit RadarGeomInfo(RadarGeometryTypeEnum);
    void update();
    RadarGeometryTypeEnum type;
    vector<float> lineData;
    vector<unsigned char> colorData;
    int colorInt{};
    QColor qcolor;
    bool isEnabled;
    int lineSizeDefault{10};
    double lineSize{};
    double lineFactor{20.0};
    static void loadData(const string&, vector<float>&);
    static const unordered_map<RadarGeometryTypeEnum, string> typeToFileName;
    static const unordered_map<RadarGeometryTypeEnum, string> prefToken;
    static const unordered_map<RadarGeometryTypeEnum, string> defaultPref;
    static const unordered_map<RadarGeometryTypeEnum, string> prefTokenLineSize;
    static const unordered_map<RadarGeometryTypeEnum, string> prefTokenColorInt;
    static const unordered_map<RadarGeometryTypeEnum, int> prefTokenColorIntDefault;
};

#endif  // RADARGEOMINFO_H
