// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "ObjectNhc.h"
#include "common/GlobalVariables.h"
#include "objects/WString.h"
#include "util/DownloadText.h"
#include "util/Utility.h"
#include "util/UtilityIO.h"
#include "util/UtilityList.h"
#include "util/UtilityString.h"

void ObjectNhc::getTextData() {
    statusList.clear();

    const auto url = GlobalVariables::nwsNhcWebsitePrefix + "/CurrentStorms.json";
    // const string url = "https://www.nhc.noaa.gov/productexamples/NHC_JSON_Sample.json";

    const auto html = UtilityIO::getHtml(url);
    ids = UtilityString::parseColumn(html, "\"id\": \"(.*?)\"");
    binNumbers = UtilityString::parseColumn(html, "\"binNumber\": \"(.*?)\"");
    names = UtilityString::parseColumn(html, "\"name\": \"(.*?)\"");
    classifications = UtilityString::parseColumn(html, "\"classification\": \"(.*?)\"");

    intensities = UtilityString::parseColumn(html, "\"intensity\": \"(.*?)\"");
    pressures = UtilityString::parseColumn(html, "\"pressure\": \"(.*?)\"");
    // sample data not quoted for these two;
    // intensities = UtilityString::parseColumn(html, "\"intensity\": (.*?),");
    // pressures = UtilityString::parseColumn(html, "\"pressure\": (.*?),");

    latitudes = UtilityString::parseColumn(html, "\"latitude\": \"(.*?)\"");
    longitudes = UtilityString::parseColumn(html, "\"longitude\": \"(.*?)\"");
    movementDirs = UtilityString::parseColumn(html, "\"movementDir\": (.*?),");
    movementSpeeds = UtilityString::parseColumn(html, "\"movementSpeed\": (.*?),");
    lastUpdates = UtilityString::parseColumn(html, "\"lastUpdate\": \"(.*?)\"");

    auto publicAdvisoriesChunk = UtilityString::parseColumn(html, "\"publicAdvisory\": \\{(.*?)\\}");
    auto forecastAdvisoriesChunk = UtilityString::parseColumn(html, "\"forecastAdvisory\": \\{(.*?)\\}");
    auto forecastDiscussionsChunk = UtilityString::parseColumn(html, "\"forecastDiscussion\": \\{(.*?)\\}");
    auto windSpeedProbabilitiesChunk = UtilityString::parseColumn(html, "\"windSpeedProbabilities\": \\{(.*?)\\}");
    for (const auto& chunk : publicAdvisoriesChunk) {
        auto token = UtilityString::parse(chunk, "\"url\": \"(.*?)\"");
        publicAdvisories.push_back(token);
        auto tokenNum = UtilityString::parse(chunk, "\"advNum\": \"(.*?)\"");
        publicAdvisoriesNumbers.push_back(tokenNum);
    }
    for (const auto& chunk : forecastAdvisoriesChunk) {
        auto token = UtilityString::parse(chunk, "\"url\": \"(.*?)\"");
        forecastAdvisories.push_back(token);
    }
    for (const auto& chunk : forecastDiscussionsChunk) {
        auto token = UtilityString::parse(chunk, "\"url\": \"(.*?)\"");
        forecastDiscussions.push_back(token);
    }
    for (const auto& chunk : windSpeedProbabilitiesChunk) {
        auto token = UtilityString::parse(chunk, "\"url\": \"(.*?)\"");
        windSpeedProbabilities.push_back(token);
    }
    for (const auto& adv : publicAdvisories) {
        auto productToken = WString::replace(WString::split(adv, "/").back() , ".shtml", "");
        auto text = DownloadText::byProduct(productToken);
        auto status = UtilityString::parse(WString::replace(text, "\n", " "), "(\\.\\.\\..*?\\.\\.\\.)");
        statusList.push_back(status);
    }
}

void ObjectNhc::showTextData() {
    for (auto index : range(ids.size())) {
        stormDataList.emplace_back(
            names[index],
            movementDirs[index],
            movementSpeeds[index],
            pressures[index],
            binNumbers[index],
            ids[index],
            lastUpdates[index],
            classifications[index],
            latitudes[index],
            longitudes[index],
            intensities[index],
            Utility::safeGet(statusList, index),
            Utility::safeGet(publicAdvisories, index));
    }
}
