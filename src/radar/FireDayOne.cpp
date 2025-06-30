// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "FireDayOne.h"
#include "common/GlobalVariables.h"
#include "objects/LatLon.h"
#include "objects/WString.h"
#include "util/To.h"
#include "util/UtilityIO.h"
#include "util/UtilityList.h"
#include "util/UtilityString.h"

DownloadTimer FireDayOne::timer{"FIRE"};
unordered_map<int, vector<double>> FireDayOne::polygonBy;
const vector<QColor> FireDayOne::colors{
    QColor{255, 128, 0},
    QColor{255, 0, 0},
    QColor{255, 128, 255}
};
const vector<string> FireDayOne::threatList{
    "ELEV",
    "CRIT",
    "EXTM"
};

void FireDayOne::get() {
    if (timer.isRefreshNeeded()) {
        // const auto day = 1;
        // const auto html = UtilityIO::getHtml(GlobalVariables::nwsSPCwebsitePrefix + "/products/outlook/KWNSPTSDY" + To::string(day) + ".txt");
        const auto mainHtml = UtilityIO::getHtml("https://www.spc.noaa.gov/products/fire_wx/fwdy1.html");
        // CLICK FOR <a href="/products/fire_wx/2025/250111_1200_day1pts.txt">DAY 1 FIREWX AREAL OUTLINE PRODUCT (KWNSPFWFD1)</a>
        const auto arealOutlineUrl = UtilityString::parse(mainHtml, "a href=.(/products/fire_wx/[0-9]{4}/[0-9]{6}_[0-9]{4}_day1pts.txt).>DAY 1 FIREWX AREAL OUTLINE PRODUCT");
        // arealOutlineUrl: str = "/products/fire_wx/2025/250108_1200_day1pts.txt"
        const auto html = UtilityIO::getHtml(GlobalVariables::nwsSPCwebsitePrefix + arealOutlineUrl);
        auto htmlBlob = UtilityString::parse(html, "... CATEGORICAL ...(.*?&)&");
        htmlBlob = WString::replace(htmlBlob, "   ", " ");
        htmlBlob = WString::replace(htmlBlob, "  ", " ");
        for (auto m : range(threatList.size())) {
            const auto threatLevelCode = threatList[m];
            const auto htmlList = UtilityString::parseColumn(htmlBlob, UtilityString::substring(threatLevelCode, 1) + " (.*?)[A-Z&]");
            string data;
            for (const auto& polygon : htmlList) {
                const auto coordinates = UtilityString::parseColumn(polygon, "([0-9]{8}).*?");
                for (const auto& coord : coordinates) {
                    data += LatLon::fromWatchData(coord).printSpaceSeparated();
                }
                data += ":";
                data = WString::replace(data, " :", ":");
            }
            const auto polygons = WString::split(data, ":");
            //
            // for each polygon parse apart the numbers and then add even numbers to one list and odd numbers to the other list
            // from there transform into the normal dataset needed for drawing lines in the graphic renderer
            //
            if (polygons.size() > 1) {
                vector<double> warningList;
                for (const auto& polygon : polygons) {
                    if (!polygon.empty()) {
                        const auto numbers = WString::split(polygon, " ");
                        vector<double> x;
                        vector<double> y;
                        for (auto index : range(numbers.size())) {
                            if (index % 2 == 0) {
                                x.push_back(To::Double(numbers[index]));
                            } else {
                                y.push_back(To::Double(numbers[index]));
                            }
                        }
                        if (!x.empty() && !y.empty()) {
                            warningList.push_back(x[0]);
                            warningList.push_back(y[0]);
                            for (int j : range2(1, x.size() - 1)) {
                                if (x[j] < 99.0) {
                                    warningList.push_back(x[j]);
                                    warningList.push_back(y[j]);
                                    warningList.push_back(x[j]);
                                    warningList.push_back(y[j]);
                                } else {
                                    warningList.push_back(x[j - 1]);
                                    warningList.push_back(y[j - 1]);
                                    warningList.push_back(x[j + 1]);
                                    warningList.push_back(y[j + 1]);
                                }
                            }
                            warningList.push_back(x[x.size() - 1]);
                            warningList.push_back(y[x.size() - 1]);
                        }
                    }
                }
                polygonBy[m] = warningList;
            }
        }
    }
}
