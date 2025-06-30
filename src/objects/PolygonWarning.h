// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef POLYGONWARNING_H
#define POLYGONWARNING_H

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include "objects/DataStorage.h"
#include "objects/DownloadTimer.h"
#include "radar/PolygonType.h"

using std::string;
using std::unique_ptr;
using std::unordered_map;
using std::vector;

class PolygonWarning {
public:
    explicit PolygonWarning(PolygonType);
    void download();
    string getData() const;
    string getTypeName() const;
    string prefTokenStorage() const;
    string prefTokenEnabled() const;
    string prefTokenColor() const;
    int color() const;
    string name() const;
    string urlToken() const;
    string getUrl() const;
    int getCount() const;
    void update();
    PolygonType type;
    bool isEnabled;

    static void load();
    static string getLongName(PolygonType);
    static string getShortName(PolygonType);
    static bool areAnyEnabled();
    static bool isCountNonZero();
    static const string pVtec;
    static const string baseUrl;
    static const unordered_map<PolygonType, int> defaultColors;
    static const unordered_map<PolygonType, string> longName;
    static const vector<PolygonType> polygonList;
    static unordered_map<PolygonType, unique_ptr<PolygonWarning>> byType;
    static const unordered_map<PolygonType, string> namesByEnumId;

private:
    DownloadTimer timer;
    DataStorage storage;

public:
    int colorInt;
};

#endif  // POLYGONWARNING_H
