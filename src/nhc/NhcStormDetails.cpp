// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include <regex>
#include <algorithm>
#include "NhcStormDetails.h"
#include <QImage>
#include "objects/WString.h"
#include "util/CrashLog.h"
#include "util/To.h"
#include "util/UtilityIO.h"
#include "util/UtilityMath.h"
#include "util/UtilityString.h"

NhcStormDetails::NhcStormDetails(
    const string& name,
    const string& movementDir,
    const string& movementSpeed,
    const string& pressure,
    const string& binNumber,
    const string& stormId,
    [[maybe_unused]] const string& lastUpdate,
    const string& classification,
    const string& lat,
    const string& lon,
    const string& intensity,
    const string& status,
    const string& advisoryUrl)
    : name{name}
    , movementDir{movementDir}
    , movementSpeed{movementSpeed}
    , pressure{pressure}
    , binNumber{binNumber}
    , stormId{stormId}
    , classification{classification}
    , lat{lat}
    , lon{lon}
    , intensity{intensity}
    , status{status}
    , advisoryUrl{advisoryUrl}
    , center{lat + " " + lon}
    , goesUrl{"https://cdn.star.nesdis.noaa.gov/FLOATER/data/" + WString::toUpper(stormId) + "/GEOCOLOR/latest.jpg"}
    , movement{UtilityMath::bearingToDirection(To::Int(movementDir)) + "(" + movementDir + ") at " + movementSpeed + " mph"}
    , modBinNumber{WString::replace(WString::toUpper(UtilityString::substring(stormId, 0, 4)), "AL", "AT")}
    , baseUrl{"https://www.nhc.noaa.gov/storm_graphics/" + modBinNumber + "/" + WString::toUpper(stormId)}
    , graphicsPageUrl{"https://www.nhc.noaa.gov/graphics_" + WString::toLower(binNumber) + ".shtml"}
    , advisoryNumber{WString::replace(WString::split(advisoryUrl, "/").back(), ".shtml", "")}
{
    // the pictures' addresses, as the storm's graphics page lists them, e.g.
    // storm_graphics/EP18/refresh/EP182026_5day_cone_sm+png/011447_5day_cone_sm.png
    const auto page = UtilityIO::getHtml(graphicsPageUrl);
    const auto prefix = WString::toUpper(stormId);
    const std::regex pattern{"storm_graphics/[A-Za-z0-9]+/refresh/[A-Za-z0-9_+]+/[0-9]+_[A-Za-z0-9_]+\\.(?:png|gif)"};
    vector<string> found;
    for (auto it = std::sregex_iterator(page.begin(), page.end(), pattern); it != std::sregex_iterator(); ++it) {
        auto url = "https://www.nhc.noaa.gov/" + it->str();
        // the page only links 60x48 icons ("_sm"); the full-size picture has the same address without it
        if (url.find("_sm+png/") != string::npos) {
            url = WString::replace(WString::replace(url, "_sm+png/", "+png/"), "_sm.png", ".png");
        }
        if (url.find(prefix) != string::npos && std::find(found.begin(), found.end(), url) == found.end()) {
            found.push_back(url);
        }
    }
    // most useful first: cones, then winds, then arrival times and probabilities, then rainfall
    static const vector<string> order{"5day_cone.png", "3day_cone.png", "5day_expCone", "current_wind", "wind_history", "earliest_reasonable_toa", "most_likely_toa", "wind_probs_34", "wind_probs_50", "wind_probs_64", "INTQPF"};
    const auto rank = [] (const string& url) {
        for (size_t i = 0; i < order.size(); i += 1) {
            if (url.find(order[i]) != string::npos) {
                return i;
            }
        }
        return order.size();
    };
    std::stable_sort(found.begin(), found.end(), [&rank] (const string& a, const string& b) { return rank(a) < rank(b); });
    graphicUrls = found;
    for (const auto& url : found) {
        if (url.find("5day_cone.png") != string::npos) {
            coneUrl = url;
            break;
        }
    }
    if (coneUrl.empty()) {
        for (const auto& url : found) {
            if (url.find("3day_cone.png") != string::npos) {
                coneUrl = url;
                break;
            }
        }
    }
    if (coneUrl.empty() && found.empty()) {
        coneUrl = baseUrl + "_5day_cone.png";   // the fixed address, if the page could not be read
    }
    if (!coneUrl.empty()) {
        coneBytes = UtilityIO::downloadAsByteArray(coneUrl);
        const auto width = QImage::fromData(coneBytes).width();
        CrashLog::write("NHC cone " + stormId + ": " + coneUrl + " -> " + std::to_string(coneBytes.size()) + " bytes, " + std::to_string(width) + " px wide, graphics page " + std::to_string(page.size()) + " bytes, " + std::to_string(found.size()) + " pictures listed");
        if (width < 300) {   // an icon, or nothing: the plain full-size address NHC has always used
            const auto fixed = UtilityIO::downloadAsByteArray(baseUrl + "_5day_cone.png");
            if (QImage::fromData(fixed).width() > width) {
                coneBytes = fixed;
                coneUrl = baseUrl + "_5day_cone.png";
            }
        }
    }
}

string NhcStormDetails::forTopHeader() const {
    return movement + ", " + pressure + " mb, " + intensity + " mph";
}
