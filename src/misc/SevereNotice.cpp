// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "SevereNotice.h"
#include "common/GlobalVariables.h"
#include "objects/PolygonWatch.h"
#include "objects/WString.h"
#include "util/To.h"

SevereNotice::SevereNotice(PolygonType type)
    : type{type}
{}

void SevereNotice::getBitmaps() {
    urls.clear();
    const auto html = PolygonWatch::byType[type]->numberList.getValue();
    const auto numberList = WString::split(html, ":");
    for (const auto& number : numberList) {
        if (!number.empty()) {
            string url;
            switch (type) {
                case Mcd:
                    url = GlobalVariables::nwsSPCwebsitePrefix + "/products/md/mcd" + number + ".png";
                    break;
                case Watch:
                    url = GlobalVariables::nwsSPCwebsitePrefix + "/products/watch/ww" + number + "_radar.gif";
                    break;
                case Mpd:
                    url = GlobalVariables::nwsWPCwebsitePrefix + "/metwatch/images/mcd" + number + ".gif";
                    break;
                default:
                    break;
            }
            urls.push_back(url);
        }
    }
}

string SevereNotice::getShortName() const {
    return PolygonWatch::getShortName(type);
}

string SevereNotice::getCount() const {
    return To::string(getCountAsInt());
}

int SevereNotice::getCountAsInt() const {
    return static_cast<int>(urls.size());
}
