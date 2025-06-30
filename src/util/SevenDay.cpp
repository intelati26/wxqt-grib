// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "SevenDay.h"
#include "objects/WString.h"
#include "settings/UIPreferences.h"
#include "util/Forecast.h"
#include "util/Utility.h"
#include "util/UtilityDownloadNws.h"
#include "util/UtilityList.h"
#include "util/UtilityString.h"
#include "util/UtilityUS.h"

void SevenDay::process(const LatLon& latLon) {
    icons.clear();
    shortForecasts.clear();
    detailedForecasts.clear();
    if (UIPreferences::useNwsApi) {
        const auto html = UtilityDownloadNws::get7DayData(latLon);
        const auto names = UtilityString::parseColumn(html, "\"name\": \"(.*?)\",");
        const auto temperatures = UtilityString::parseColumn(html, "\"temperature\": (.*?),");
        const auto detailedLocalForecasts = UtilityString::parseColumn(html, "\"detailedForecast\": \"(.*?)\"");
        icons = UtilityString::parseColumn(html, "\"icon\": \"(.*?)\",");
        const auto shortLocalForecasts = UtilityString::parseColumn(html, "\"shortForecast\": \"(.*?)\",");
        vector<Forecast> forecasts;
        for (auto index : range(names.size())) {
            const auto name = Utility::safeGet(names, index);
            const auto temperature = Utility::safeGet(temperatures, index);
            const auto icon = Utility::safeGet(icons, index);
            const auto shortForecast = Utility::safeGet(shortLocalForecasts, index);
            const auto detailedForecast = Utility::safeGet(detailedLocalForecasts, index);
            forecasts.emplace_back(name, temperature, icon, shortForecast, detailedForecast);
        }
        for (const auto& forecast : forecasts) {
            detailedForecasts.push_back(forecast.name + ": " + forecast.detailedForecast);
            shortForecasts.push_back(forecast.name + ": " + forecast.shortForecast);
        }
    } else {
        const auto forecastStringList = UtilityUS::getCurrentConditionsUS(latLon);
        const auto& forecastString = forecastStringList[1];
        const auto& iconString = forecastStringList[0];
        const auto forecasts = WString::split(forecastString, "\n");
        const auto iconList = UtilityString::parseColumn(iconString, "<icon-link>(.*?)</icon-link>");
        for (size_t index : range(forecasts.size())) {
            if (!forecasts[index].empty()) {
                detailedForecasts.push_back(WString::strip(forecasts[index]));
                if (iconList.size() > index) {
                    icons.push_back(iconList[index]);
                } else {
                    icons.emplace_back("");
                }
            }
        }
    }
}
