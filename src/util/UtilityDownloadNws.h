// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef UTILITYDOWNLOADNWS_H
#define UTILITYDOWNLOADNWS_H

#include <string>
#include "objects/LatLon.h"

using std::string;

class UtilityDownloadNws {
public:
    static string getHourlyData(const LatLon&);
    static string getHourlyOldData(const LatLon&);
    static string getLocationPointData(const LatLon&);
    static string get7DayData(const LatLon&);
    static string getCap(const string&);
    static string getHtmlWithXml(const string&);
    static string getLocationHtml(const LatLon&);
};

#endif  // UTILITYDOWNLOADNWS_H
