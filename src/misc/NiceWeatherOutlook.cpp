// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "misc/NiceWeatherOutlook.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <QDateTime>
#include <QString>
#include "gfs/GfsChart.h"
#include "gfs/GfsRender.h"
#include "misc/ImageViewer.h"
#include "objects/FutureVoid.h"
#include "ui/ActivityLabel.h"
#include "ui/UiStandards.h"
#include "util/Utility.h"

namespace {
    const std::vector<std::string> dayLabels{"Day 1", "Day 2", "Day 3", "Day 4", "Day 5", "Day 6", "Day 7", "Day 8"};
    const char * const levelNames[6] = {"Below Decent", "Decent (1)", "Fair (2)", "Nice (3)", "Beautiful (4)", "Perfect (5/5)"};

    bool insideUnitedStates(double lon, double lat) {
        return GfsChart::insideUnitedStates(lon, lat);
    }

    struct Region {
        const char * name;
        double west, east, south, north;
    };
    // the regions, in the order that a cell takes the first it is in
    const std::vector<Region>& regions() {
        static const std::vector<Region> list{
            {"Pacific Northwest", -125.0, -116.0, 42.0, 50.0},
            {"California and Nevada", -125.0, -114.0, 32.0, 42.0},
            {"Rockies and Great Basin", -116.0, -104.0, 37.0, 50.0},
            {"Desert Southwest", -114.0, -103.0, 30.0, 37.0},
            {"Northern Plains", -104.0, -94.0, 42.0, 50.0},
            {"Central Plains", -104.0, -94.0, 37.0, 42.0},
            {"Southern Plains", -104.0, -93.0, 25.0, 37.0},
            {"Great Lakes", -94.0, -80.0, 41.0, 50.0},
            {"Ohio and Tennessee Valleys", -94.0, -80.0, 35.0, 41.0},
            {"Southeast and Gulf Coast", -94.0, -78.0, 24.0, 35.0},
            {"Northeast and Mid-Atlantic", -80.0, -66.0, 35.0, 48.0},
        };
        return list;
    }

    // how the grid of levels is colored: the area at each level, and the regions
    std::string summary(const GfsGrid::Grid& grid) {
        std::array<double, 6> area{};
        double total = 0.0;
        struct Sum {
            double area{0}, level{0}, nicer{0};
        };
        std::vector<Sum> sums(regions().size());
        for (int row = 0; row < grid.rows; row++) {
            const double lat = grid.lat0 - row * grid.step;
            const double weight = std::cos(lat * M_PI / 180.0);
            for (int column = 0; column < grid.columns; column++) {
                const float value = grid.values[static_cast<size_t>(row) * static_cast<size_t>(grid.columns) + static_cast<size_t>(column)];
                if (std::isnan(value)) {
                    continue;
                }
                double lon = grid.lon0 + column * grid.step;
                if (lon > 180.0) {
                    lon -= 360.0;
                }
                if (!insideUnitedStates(lon, lat)) {
                    continue;
                }
                const int level = std::clamp(static_cast<int>(std::lround(value)), 0, 5);
                area[static_cast<size_t>(level)] += weight;
                total += weight;
                for (size_t r = 0; r < regions().size(); r++) {
                    const auto& region = regions()[r];
                    if (lon >= region.west && lon < region.east && lat >= region.south && lat < region.north) {
                        sums[r].area += weight;
                        sums[r].level += weight * level;
                        sums[r].nicer += level >= 3 ? weight : 0.0;
                        break;
                    }
                }
            }
        }
        if (total <= 0.0) {
            return "No forecast for this day yet.";
        }
        std::string out = "How much of the contiguous United States is at each level (by area):\n\n";
        for (int level = 5; level >= 0; level--) {
            const double share = 100.0 * area[static_cast<size_t>(level)] / total;
            out += std::string{levelNames[level]} + ": " + QString::number(share, 'f', 1).toStdString() + "%\n";
        }
        const double niceOrBetter = 100.0 * (area[3] + area[4] + area[5]) / total;
        const double beautiful = 100.0 * (area[4] + area[5]) / total;
        out += QString{"\nNice or better: %1% of the area. Beautiful or better: %2%.\n"}.arg(niceOrBetter, 0, 'f', 0).arg(beautiful, 0, 'f', 0).toStdString();
        std::vector<size_t> order;
        for (size_t r = 0; r < sums.size(); r++) {
            if (sums[r].area > 0.0) {
                order.push_back(r);
            }
        }
        std::sort(order.begin(), order.end(), [&sums] (size_t a, size_t b) { return sums[a].level / sums[a].area > sums[b].level / sums[b].area; });
        out += "\nThe regions, the best first (the average level, then the share of the region at Nice or better):\n\n";
        for (const size_t r : order) {
            out += std::string{regions()[r].name} + ": " + QString::number(sums[r].level / sums[r].area, 'f', 1).toStdString() + ", " + QString::number(100.0 * sums[r].nicer / sums[r].area, 'f', 0).toStdString() + "% Nice or better\n";
        }
        out += "\nThe levels are made from the National Blend of Models' apparent temperature, dewpoint, cloud cover, wind gusts and chance of rain through the hours of the local daytime, smoothed over 50 miles, "
               "and the air quality: the near-surface fine particles of the RRFS (days 1 to 3) and GEOS-FP, and EPA's AirNow forecasts and observations for the cities (about five days out). A top level needs every one of them to be good; the further the day, the lower the level the forecast can reach (a day 3 map stops at Beautiful, "
               "day 4 and later at Nice). The levels are Ryan Hall, Y'all's idea; the scoring is our own, a work in progress.";
        return out;
    }

    struct Result {
        QByteArray png;
        std::string summary;
        QDateTime valid;
        std::string error;
    };
}

std::string NiceWeatherOutlook::summaryOf(const GfsGrid::Grid& grid) {
    return summary(grid);
}

std::string NiceWeatherOutlook::headlineOf(const GfsGrid::Grid& grid) {
    double nice = 0.0, beautiful = 0.0, total = 0.0;
    for (int row = 0; row < grid.rows; row++) {
        const double lat = grid.lat0 - row * grid.step;
        const double weight = std::cos(lat * M_PI / 180.0);
        for (int column = 0; column < grid.columns; column++) {
            const float value = grid.values[static_cast<size_t>(row) * static_cast<size_t>(grid.columns) + static_cast<size_t>(column)];
            double lon = grid.lon0 + column * grid.step;
            if (lon > 180.0) {
                lon -= 360.0;
            }
            if (std::isnan(value) || !insideUnitedStates(lon, lat)) {
                continue;
            }
            const int level = std::clamp(static_cast<int>(std::lround(value)), 0, 5);
            total += weight;
            nice += level >= 3 ? weight : 0.0;
            beautiful += level >= 4 ? weight : 0.0;
        }
    }
    if (total <= 0.0) {
        return "no forecast yet";
    }
    return QString{"Nice or better %1%, Beautiful or better %2%"}.arg(100.0 * nice / total, 0, 'f', 0).arg(100.0 * beautiful / total, 0, 'f', 0).toStdString();
}

NiceWeatherOutlook::NiceWeatherOutlook(Window * parent, int day)
    : Window{parent}
    , comboDay{this, dayLabels}
    , header{this}
    , photo{this}
    , text{this}
    , sw{this, leftBox, text}
    , shortcutImage{QKeySequence{"1"}, this}
{
    setTitle("Nice Weather Outlook");
    photo.imageSize = UiStandards::tileImage;
    leftBox.addWidget(comboDay);
    leftBox.addWidget(header);
    leftBox.addWidget(photo);
    leftBox.addStretch();
    leftBox.addWidgetReal(new ActivityLabel{this});   // what is downloading for the chart (and a click for the network list)
    comboDay.setIndex(static_cast<size_t>(std::clamp(day, 0, 7)));
    comboDay.connect([this] { showDay(); });
    photo.connect([this, parent] { new ImageViewer{parent, photo.bytes}; });
    shortcutImage.connect([this, parent] { new ImageViewer{parent, photo.bytes}; });
    showDay();
}

void NiceWeatherOutlook::showDay() {
    const int index = std::clamp(static_cast<int>(comboDay.getIndex()), 0, 7);
    const int mine = ++generation;
    setTitle("Nice Weather Outlook, " + dayLabels[static_cast<size_t>(index)]);
    header.setText("Loading " + dayLabels[static_cast<size_t>(index)] + " ...");
    photo.setBytes(QByteArray{});
    text.setText(std::string{});
    auto result = std::make_shared<Result>();
    new FutureVoid{this, [result, index] {
        GfsRender::Session session;
        GfsRender::Variant variant;
        variant.width = 1100;
        GfsChart::Probe probe;
        result->png = GfsRender::png(session, "NBM", "nice_weather", "CONUS", "", 24 * (index + 1), {}, result->error, &probe, variant);
        if (probe.valid()) {
            result->summary = summary(probe.fields.front().grid);
            result->valid = probe.validUtc;
        }
    }, [this, result, mine, index] {
        if (mine != generation) {
            return;
        }
        if (result->png.isEmpty()) {
            header.setText(dayLabels[static_cast<size_t>(index)] + ": no chart" + (result->error.empty() ? std::string{} : " (" + result->error + ")"));
            return;
        }
        header.setText(dayLabels[static_cast<size_t>(index)] + (result->valid.isValid() ? "   Valid: " + result->valid.toString("ddd yyyy-MM-dd").toStdString() + " (the daytime, by the sun's time of each place)" : std::string{}) + "   Source: National Blend of Models");
        photo.setBytes(result->png);
        text.setText(result->summary);
    }};
}
