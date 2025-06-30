// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

// Given the raw icon URL from NWS, determine if bitmap is on disk or must be created
// input examples
//  https://api.weather.gov/icons/land/day/rain_showers,60/rain_showers,30?size=medium
//  https://api.weather.gov/icons/land/night/bkn?size=medium
//  https://api.weather.gov/icons/land/day/tsra_hi,40?size=medium

#include "UtilityForecastIcon.h"
#include "common/GlobalVariables.h"
#include "objects/WString.h"
#include "ui/ForecastIcon.h"
#include "util/UtilityList.h"
#include "util/UtilityNwsIcon.h"
#include "util/UtilityString.h"

Pix UtilityForecastIcon::getIcon(const string& url) {
    auto fileName = getFilename(url);
    if (fileName.empty() || fileName == "n") {
        return ForecastIcon::blankBitmap();
    }
    if (contains(UtilityNwsIcon::iconMap, fileName + ".png")) {
        return ForecastIcon{fileName}.get();
    }
    return parseBitmapString(fileName);
}

// Given one string that does not have a match on disk, decode and return a bitmap with textual labels
// it could be composed of 2 bitmaps with one or more textual labels (if string has a "/") or just one bitmap with label
// input examples
//  rain_showers,70/tsra,80
//  ntsra,80
Pix UtilityForecastIcon::parseBitmapString(const string& url) {
    if (WString::contains(url, "/") || WString::contains(url, ";j=")) {
        auto items = WString::split(url, "/");
        if (items.size() > 1) {
            return getDualBitmapWithNumbers(items[0], items[1]);
        } else {
            // legacy add
            auto urlTmp = WString::replace(url, "i=", "");
            urlTmp = WString::replace(urlTmp, "j=", "");
            urlTmp = WString::replace(urlTmp, "ip=", "");
            urlTmp = WString::replace(urlTmp, "jp=", "");
            items = WString::split(urlTmp, ";");
            if (items.size() > 3) {
                return getDualBitmapWithNumbers(items[0] + items[2], items[1] + items[3]);
            } else if (items.size() > 2) {
                if (WString::contains(url, ";jp=")) {
                    return getDualBitmapWithNumbers(items[0], items[1] + items[2]);
                } else {
                    return getDualBitmapWithNumbers(items[0] + items[2], items[1]);
                }
            } else {
                return getDualBitmapWithNumbers(items[0], items[1]);
            }
            // legacy add end
        }
    }
    return getBitmapWithOneNumber(url);
}

// Given two strings return a custom bitmap made of two bitmaps with optional numeric label
// input examples
//  rain_showers,60 rain_showers,30
//  nrain_showers,80 nrain_showers,70
//  ntsra_hi,40 ntsra_hi
//  bkn rain
Pix UtilityForecastIcon::getDualBitmapWithNumbers(const string& iconLeftString, const string& iconRightString) {
    const auto leftTokens = WString::split(iconLeftString, ",");
    const auto rightTokens = WString::split(iconRightString, ",");
    auto leftNumber = leftTokens.size() > 1 ? leftTokens[1] : "";
    auto rightNumber = rightTokens.size() > 1 ? rightTokens[1] : "";
    string leftWeatherCondition;
    string rightWeatherCondition;
    if (!leftTokens.empty() && !rightTokens.empty()) {
        leftWeatherCondition = leftTokens[0];
        rightWeatherCondition = rightTokens[0];
    }
    // legacy add
    if (!WString::contains(iconLeftString, ",") && !WString::contains(iconRightString, ",")) {
        leftNumber = UtilityString::parse(iconLeftString, ".*?([0-9]+)");
        leftWeatherCondition = UtilityString::parse(iconLeftString, "([a-z_]+)");
        rightNumber = UtilityString::parse(iconRightString, ".*?([0-9]+)");
        rightWeatherCondition = UtilityString::parse(iconRightString, "([a-z_]+)");
    }
    // legacy add end
    ForecastIcon forecastIcon{leftWeatherCondition, rightWeatherCondition};
    forecastIcon.drawLeftText(leftNumber);
    forecastIcon.drawRightText(rightNumber);
    forecastIcon.drawVerticalBar();
    return forecastIcon.get();
}

// Given one string return a custom bitmap with numeric label
// input examples
// nrain_showers,80
// tsra_hi,40
Pix UtilityForecastIcon::getBitmapWithOneNumber(const string& iconString) {
    const auto items = WString::split(iconString, ",");
    auto number = items.size() > 1 ? items[1] : "";
    auto weatherCondition = !items.empty() ? items[0] : "";
    // legacy add
    if (!WString::contains(iconString, ",")) {
        number = UtilityString::parse(iconString, ".*?([0-9]+)");
        weatherCondition = UtilityString::parse(iconString, "([a-z_]+)");
    }
    // legacy add end
    ForecastIcon forecastIcon{weatherCondition};
    forecastIcon.drawSingleText(number);
    return forecastIcon.get();
}

string UtilityForecastIcon::getFilename(const string& url) {
    auto fileName = WString::replace(url, "?size=medium", "");
    fileName = WString::replace(fileName, "?size=small", "");
    fileName = WString::replace(fileName, "https://api.weather.gov/icons/land/", "");
    fileName = WString::replace(fileName, "/icons/land/", "");
    fileName = WString::replace(fileName, "day/", "");
    // legacy add
    fileName = WString::replace(fileName, "https://forecast.weather.gov/newimages/medium/", "");
    fileName = WString::replace(fileName, ".png", "");
    fileName = WString::replace(fileName, "https://forecast.weather.gov/DualImage.php?", "");
    fileName = WString::replace(fileName, "&amp", "");
    // legacy add end
    if (WString::contains(fileName, "night")) {
        fileName = WString::replace(fileName, "night//", "n");
        fileName = WString::replace(fileName, "night/", "n");
        fileName = WString::replace(fileName, "/", "/n");
    }
    return fileName;
}
