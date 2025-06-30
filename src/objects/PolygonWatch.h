// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef POLYGONWATCH_H
#define POLYGONWATCH_H

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include "objects/DataStorage.h"
#include "objects/DownloadTimer.h"
#include "objects/LatLon.h"
#include "radar/PolygonType.h"

using std::string;
using std::unique_ptr;
using std::unordered_map;
using std::vector;

class PolygonWatch {
public:
    explicit PolygonWatch(PolygonType);
    void download();
    PolygonType type;
    DataStorage latLonList;
    DataStorage numberList;

    static void load();
    static string getShortName(const PolygonType&);
    static string getLatLon(const string&);
    static LatLon getLatLonFromString(const string&);
    static string storeWatchMcdLatLon(const string&);

    static unordered_map<PolygonType, unique_ptr<PolygonWatch>> byType;
    static DataStorage watchLatlonCombined;
    static const unordered_map<PolygonType, string> namesByEnumId;
    static const unordered_map<PolygonType, int> colorDefaultByType;
    static const unordered_map<PolygonType, string> colorPrefByType;
    static const vector<PolygonType> polygonList;

private:
    string getUrl() const;
    string getPrefTokenStorage() const;
    string prefTokenEnabled() const;
    string getPrefTokenNumberList() const;
    string getPrefTokenLatLon() const;
    string getTypeName() const;
    void update();
    DownloadTimer timer;
    DataStorage storage;

public:
    int colorInt;
    bool isEnabled;
};

#endif  // POLYGONWATCH_H
