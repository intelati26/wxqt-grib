// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef SITES_H
#define SITES_H

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include "Site.h"

using std::string;
using std::unordered_map;
using std::unique_ptr;
using std::vector;

class Sites {
public:
    Sites(const unordered_map<string, string>&, const unordered_map<string, string>&, const unordered_map<string, string>&, bool = false);
    string getNearest(const LatLon&);
    Site getNearestSite(const LatLon&, int order = 0);
    vector<string> getNearestList(const LatLon&, int);
    int getNearestInMiles(const LatLon&);
    vector<Site> sites;
    unordered_map<string, unique_ptr<Site>> byCode;
    vector<string> codeList;
    vector<string> nameList;

private:
    void checkValidityMaps();
    unordered_map<string, string> nameDict;
    unordered_map<string, string> latDict;
    unordered_map<string, string> lonDict;
};

#endif  // SITES_H
