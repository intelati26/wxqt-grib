// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "SwoDayOne.h"
#include "common/GlobalVariables.h"
#include "objects/LatLon.h"
#include "objects/WString.h"
#include "util/To.h"
#include "util/UtilityIO.h"
#include "util/UtilityList.h"
#include "util/UtilityString.h"

DownloadTimer SwoDayOne::timer{"SWO"};
unordered_map<int, vector<double>> SwoDayOne::polygonBy;
const vector<QColor> SwoDayOne::colors{
    QColor{255, 0, 255},  // HIGH
    QColor{255, 0, 0},    // MDT
    QColor{255, 140, 0},  // ENH
    QColor{255, 255, 0},  // SLGT
    QColor{0, 100, 0},    // MRGL
};
const vector<string> SwoDayOne::threatList{"HIGH", "MDT", "ENH", "SLGT", "MRGL"};

void SwoDayOne::get() {
    if (timer.isRefreshNeeded()) {
        const auto day = 1;
        const auto html = UtilityIO::getHtml(GlobalVariables::nwsSPCwebsitePrefix + "/products/outlook/KWNSPTSDY" + To::string(day) + ".txt");
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
