// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "NexradRenderUI.h"
#include <cmath>
#include <numbers>
#include "misc/AlertsDetail.h"
#include "misc/TextViewerStatic.h"
#include "objects/WString.h"
#include "radar/Warnings.h"
#include "radar/Watch.h"
#include "settings/UIPreferences.h"
#include "spc/SpcMcdWatchMpdViewer.h"
#include "util/DownloadText.h"

void NexradRenderUI::showPolygonText(Window * parent, const LatLon& location) {
    const auto url = Warnings::show(location);
    if (!url.empty()) {
        new AlertsDetail{parent, url};
    }
}

void NexradRenderUI::showNearestProduct(Window * parent, PolygonType type, const LatLon& location) {
    const auto txt = Watch::show(location, type);
    // https://www.spc.noaa.gov/products/md/mcd0922.gif
    // https://www.wpc.ncep.noaa.gov/metwatch/images/mcd0339.gif
    // https://www.spc.noaa.gov/products/watch/ww0283_radar.gif
    string url;
    switch (type) {
        case Mcd:
            url = "https://www.spc.noaa.gov/products/md/mcd" + txt + ".png";
            break;
        case Mpd:
            url = "https://www.wpc.ncep.noaa.gov/metwatch/images/mcd" + txt + ".gif";;
            break;
        case Watch:
            url = "https://www.spc.noaa.gov/products/watch/ww" + txt + "_radar.gif";
            break;
        default:
            break;
    }
    if (!txt.empty()) {
        new SpcMcdWatchMpdViewer{parent, url};
    }
}

LatLon NexradRenderUI::getLatLonFromScreenPosition(const NexradState& nexradState, double x, double y) {
    // const auto width = static_cast<double>(nexradState.originalWidth);
    // const auto height = static_cast<double>(nexradState.originalHeight);
    // const auto yModified = y;
    // const auto xModified = x;
    // auto density = (ortInt * 2.0) / width;
    // auto yMiddle = height / 4.0;
    // auto xMiddle = width / 2.0;
    // if (nexradState.numberOfPanes == 1) {
    //     yMiddle = height / 2.0;
    // }
    // const auto diffX = density * (xMiddle - xModified) / nexradState.zoom;
    // const auto diffY = density * (yMiddle - yModified) / nexradState.zoom;
    // const auto ppd = nexradState.getPn().oneDegreeScaleFactor;
    // const auto newX = nexradState.getPn().y() + (nexradState.xPos / nexradState.zoom + diffX) / ppd;
    // const auto test2 = 180.0 / std::numbers::pi * log(tan(std::numbers::pi / 4.0 + nexradState.getPn().x() * (std::numbers::pi / 180.0) / 2.0));
    // auto newY = test2 + (nexradState.yPos / nexradState.zoom + diffY) / ppd;
    // newY = (180.0 / std::numbers::pi * (2.0 * atan(exp(newY * std::numbers::pi / 180.0)) - std::numbers::pi / 2.0));
    // return {newY, -1.0f * newX};

    const auto width = static_cast<double>(nexradState.originalWidth);
    const auto height = static_cast<double>(nexradState.originalHeight);
    const auto yModified = y;
    const auto xModified = x;
    const auto densityMultiplier = UIPreferences::tiledWindows ? 2.0 : 1.0;  // wxpy diff
    auto density = densityMultiplier * 0.52 / nexradState.zoom;
    auto yMiddle = height / 2.0;
    auto xMiddle = width / 2.0;
    if (nexradState.numberOfPanes == 4) {
        yMiddle = height / 4.0;
        xMiddle = width / 4.0;
        density = 1.04 / nexradState.zoom;
    }
    const auto diffX = density * (xMiddle - xModified);
    const auto diffY = density * (yMiddle - yModified);
    const auto ppd = nexradState.getPn().getOneDegreeScaleFactor();
    const auto newX = nexradState.getPn().getLatLon().lon() + (nexradState.xPos / nexradState.zoom + diffX) / ppd;
    const auto test2 = 180.0 / std::numbers::pi * log(tan(std::numbers::pi / 4.0 + nexradState.getPn().getLatLon().lat() * (std::numbers::pi / 180.0) / 2.0));
    auto newY = test2 + (nexradState.yPos / nexradState.zoom + diffY) / ppd;
    newY = (180.0 / std::numbers::pi * (2.0 * atan(exp(newY * std::numbers::pi / 180.0)) - std::numbers::pi / 2.0));
    return {newY, -1.0 * newX};
}

void NexradRenderUI::showRadarStatusMessage(Window * parent, const string& radarSite) {
    auto message = DownloadText::byProduct("FTM" + WString::toUpper(radarSite));
    if (message.empty()) {
        message = "The current radar status for " + radarSite + " is not available.";
    }
    new TextViewerStatic{parent, message, radarSite};
}
