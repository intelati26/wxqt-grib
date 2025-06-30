// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef UTILITYSPCMESO_H
#define UTILITYSPCMESO_H

#include <string>
#include <unordered_map>
#include <vector>
#include "objects/LatLon.h"
#include "ui/MenuTitle.h"

using std::string;
using std::unordered_map;
using std::vector;

class UtilitySpcMeso {
public:
    static string getNearest(const LatLon&);
    static vector<MenuTitle> titles;
    static const vector<string> favList;
    static const vector<string> imgSf;
    static const vector<string> products;
    static const vector<string> labels;
    static const vector<string> sectorCodes;
    static const vector<string> sectors;
    static const unordered_map<string, LatLon> sectorToLatLon;
    static const unordered_map<string, string> sectorMap;
    static const unordered_map<string, string> sectorMapForTitle;
};

#endif  // UTILITYSPCMESO_H
