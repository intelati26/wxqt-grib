// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "UtilitySpcSwo.h"
#include "common/GlobalVariables.h"
#include "util/To.h"
#include "util/UtilityIO.h"
#include "util/UtilityList.h"
#include "util/UtilityString.h"

vector<string> UtilitySpcSwo::getSwoStateUrl(string state, int dayInt) {
    auto day = To::string(dayInt);
    switch (dayInt) {
        case 1:
        case 2:
            return {GlobalVariables::nwsSPCwebsitePrefix + "/partners/outlooks/state/images/" + state + "_swody" + day + ".png",
                        GlobalVariables::nwsSPCwebsitePrefix + "/partners/outlooks/state/images/" + state + "_swody" + day + "_TORN.png",
                        GlobalVariables::nwsSPCwebsitePrefix + "/partners/outlooks/state/images/" + state + "_swody" + day + "_HAIL.png",
                        GlobalVariables::nwsSPCwebsitePrefix + "/partners/outlooks/state/images/" + state + "_swody" + day + "_WIND.png"};
        case 3:
            return {
                GlobalVariables::nwsSPCwebsitePrefix + "/partners/outlooks/state/images/" + state + "_swody" + day + ".png",
                GlobalVariables::nwsSPCwebsitePrefix + "/partners/outlooks/state/images/" + state + "_swody" + day + "_PROB.png"};
        case 4:
        case 5:
        case 6:
        case 7:
        case 8:
        case 48:
            return {
                    GlobalVariables::nwsSPCwebsitePrefix + "/partners/outlooks/state/images/" + state + "_swody" + "4" + "_PROB.png",
                    GlobalVariables::nwsSPCwebsitePrefix + "/partners/outlooks/state/images/" + state + "_swody" + "5" + "_PROB.png",
                    GlobalVariables::nwsSPCwebsitePrefix + "/partners/outlooks/state/images/" + state + "_swody" + "6" + "_PROB.png",
                    GlobalVariables::nwsSPCwebsitePrefix + "/partners/outlooks/state/images/" + state + "_swody" + "7" + "_PROB.png",
                    GlobalVariables::nwsSPCwebsitePrefix + "/partners/outlooks/state/images/" + state + "_swody" + "8" + "_PROB.png"};
        default:
            return {};
    }
}

string UtilitySpcSwo::getImageUrlsDays48(int day) {
    return GlobalVariables::nwsSPCwebsitePrefix + "/products/exper/day4-8/day" + To::string(day) + "prob.gif";
}

vector<string> UtilitySpcSwo::getImageUrls(int day) {
    vector<string> urls;
    if (day == 48) {
        for (auto dayInt : range2(4, 9)) {
            urls.push_back(GlobalVariables::nwsSPCwebsitePrefix + "/products/exper/day4-8/day" + To::string(dayInt) + "prob.gif");
        }
        return urls;
    }
    const auto html = UtilityIO::getHtml(GlobalVariables::nwsSPCwebsitePrefix + "/products/outlook/day" + To::string(day) + "otlk.html");
    const auto time = UtilityString::parse(html, "show_tab\\(.otlk_([0-9]{4}).\\)");
    if (day == 1 || day == 2) {
        auto baseUrl = GlobalVariables::nwsSPCwebsitePrefix + "/products/outlook/day" + To::string(day) + "probotlk_";
        urls.push_back(GlobalVariables::nwsSPCwebsitePrefix + "/products/outlook/day" + To::string(day) + "otlk_" + time + ".gif");
        for (const auto& urlEnd : {"_torn.gif", "_hail.gif", "_wind.gif"}) {
            urls.push_back(baseUrl + time + urlEnd);
        }
    } else {
        for (const auto& urlEnd : {"otlk_", "prob_"}) {
            urls.push_back(GlobalVariables::nwsSPCwebsitePrefix + "/products/outlook/day" + To::string(day) + urlEnd + time + ".gif");
        }
    }
    return urls;
}
