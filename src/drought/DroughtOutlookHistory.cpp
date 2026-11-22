// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "drought/DroughtOutlookHistory.h"
#include <cmath>
#include "util/PermanentCache.h"
#include "objects/URL.h"

namespace DroughtOutlookHistory {
    const std::vector<Kind>& kinds() {
        static const std::vector<Kind> list{{"610temp", "6-10 day temperature", true}, {"814temp", "8-14 day temperature", true}, {"610prcp", "6-10 day precipitation", false}, {"814prcp", "8-14 day precipitation", false}};
        return list;
    }

    int bin(const std::string& category, int probability) {
        if (category == "Normal") {
            return 8;
        }
        const int step = probability < 40 ? 0 : probability < 50 ? 1 : probability < 60 ? 2 : probability < 70 ? 3 : probability < 80 ? 4 : probability < 90 ? 5 : 6;
        return category == "Above" ? 1 + step : category == "Below" ? 9 + step : 0;
    }

    std::string label(int bin, bool temperature) {
        static const char * const range[7] = {"33% to 40%", "40% to 50%", "50% to 60%", "60% to 70%", "70% to 80%", "80% to 90%", "90% to 100%"};
        if (bin == 8) {
            return temperature ? "Near Normal" : "Near Normal";
        }
        if (bin >= 1 && bin <= 7) {
            return std::string{range[bin - 1]} + " Above";
        }
        if (bin >= 9 && bin <= 15) {
            return std::string{range[bin - 9]} + " Below";
        }
        return "None";
    }

    const std::array<int, 15>& stackOrder() {
        static const std::array<int, 15> order{15, 14, 13, 12, 11, 10, 9, 8, 1, 2, 3, 4, 5, 6, 7};
        return order;
    }

    UtilityDrought::Raster geometry() {
        UtilityDrought::Raster r;
        r.west = west;
        r.north = north;
        r.step = step;
        r.columns = static_cast<int>(std::ceil((east - west) / step));
        r.rows = static_cast<int>(std::ceil((north - south) / step));
        r.category.assign(static_cast<size_t>(r.columns) * static_cast<size_t>(r.rows), 0);
        return r;
    }

    bool day(const Kind& kind, const QDate& issued, UtilityDrought::Raster& out, std::string& error) {
        const PermanentCache store{"drought"};
        const auto date = issued.toString("yyyyMMdd").toStdString();
        const auto name = "outlooks/" + kind.key + "_" + date + "_v1.bin";
        out = geometry();
        const auto kept = store.read(name);
        if (kept.size() == out.category.size()) {
            std::copy(kept.begin(), kept.end(), out.category.begin());
            return true;
        }
        const auto kmz = URL::getBytes("https://ftp.cpc.ncep.noaa.gov/GIS/us_tempprcpfcst/" + kind.key + "_" + date + ".kmz").toStdString();
        UtilityDrought::ProbabilityOutlook outlook;
        if (kmz.size() < 20000 || !UtilityDrought::parseProbabilityKmz(kmz, outlook, error)) {
            if (error.empty()) {
                error = "no " + kind.name + " outlook for " + date;
            }
            return false;
        }
        std::vector<std::vector<UtilityDrought::Polygon>> zones(15);
        for (auto& zone : outlook.zones) {
            const int b = bin(zone.category, zone.probability);
            if (b >= 1) {
                for (auto& shape : zone.shapes) {
                    zones[static_cast<size_t>(b - 1)].push_back(std::move(shape));
                }
            }
        }
        out = UtilityDrought::rasterizeZones(zones, west, south, east, north, step);
        store.write(name, std::string{out.category.begin(), out.category.end()});
        return true;
    }

    std::array<double, categories> shares(const UtilityDrought::Raster& grid, const std::vector<uint8_t>& mask) {
        std::array<double, categories> sum{};
        double total = 0.0;
        for (int y = 0; y < grid.rows; y++) {
            const double weight = std::cos((grid.north - (y + 0.5) * grid.step) * M_PI / 180.0);
            for (int x = 0; x < grid.columns; x++) {
                const size_t at = static_cast<size_t>(y) * static_cast<size_t>(grid.columns) + static_cast<size_t>(x);
                if (at < mask.size() && mask[at] != 0) {
                    sum[std::min<size_t>(grid.category[at], categories - 1)] += weight;
                    total += weight;
                }
            }
        }
        if (total > 0.0) {
            for (auto& v : sum) {
                v = 100.0 * v / total;
            }
        }
        return sum;
    }
}
