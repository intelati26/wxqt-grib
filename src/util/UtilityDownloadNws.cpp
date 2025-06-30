// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "UtilityDownloadNws.h"
#include "common/GlobalVariables.h"
#include "objects/URL.h"
#include "objects/WString.h"
#include "util/UtilityIO.h"
#include "util/UtilityString.h"

string UtilityDownloadNws::getHourlyData(const LatLon& latLon) {
    const auto pointsData = getLocationPointData(latLon);
    const auto hourlyUrl = UtilityString::parse(pointsData, "\"forecastHourly\": \"(.*?)\"");
    return UtilityIO::getHtmlWithRetry(hourlyUrl, 1000);
}

string UtilityDownloadNws::getHourlyOldData(const LatLon& latLon) {
    return UtilityIO::getHtml("https://forecast.weather.gov/MapClick.php?lat=" +
            latLon.latForNws() + "&lon=" +
            latLon.lonForNws() + "&FcstType=digitalDWML");
}

string UtilityDownloadNws::get7DayData(const LatLon& latLon) {
    const auto pointsData = getLocationPointData(latLon);
    const auto forecastUrl = UtilityString::parse(pointsData, "\"forecast\": \"(.*?)\"");
    return UtilityIO::getHtmlWithRetry(forecastUrl, 3000);
}

string UtilityDownloadNws::getLocationPointData(const LatLon& latLon) {
    return UtilityIO::getHtml(GlobalVariables::nwsApiUrl + "/points/" + latLon.latForNws() + "," + latLon.lonForNws());
}

string UtilityDownloadNws::getCap(const string& sector) {
    if (sector == "us") {
        return getHtmlWithXml("https://api.weather.gov/alerts/active?region_type=land");
    }
    return getHtmlWithXml("https://api.weather.gov/alerts/active?state=" + WString::toUpper(sector));
}

string UtilityDownloadNws::getHtmlWithXml(const string& url) {
    return URL::getTextXmlAcceptHeader(url);
}

string UtilityDownloadNws::getLocationHtml(const LatLon& latLon) {
    return UtilityIO::getHtml("https://forecast.weather.gov/MapClick.php?lat=" + latLon.latForNws() + "&lon=" + latLon.lonForNws() + "&unit=0&lg=english&FcstType=dwml");
}
