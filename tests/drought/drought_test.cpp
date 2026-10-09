// Manual checks of the drought shapes: the Monitor's KMZ of 29 September 2026 and the Census state boundaries, against the Monitor's own statistics for that week
#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <QGuiApplication>
#include "drought/UtilityDrought.h"

static std::string slurp(const std::string& path) {
    std::ifstream in{path, std::ios::binary};
    std::stringstream s;
    s << in.rdbuf();
    return s.str();
}

#define CHECK(cond, text) do { if (!(cond)) { std::printf("FAILED: %s\n", text); return 1; } } while (0)

int main(int argc, char ** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app{argc, argv};
    const std::string dir = argc > 1 ? argv[1] : "fixtures";
    UtilityDrought::Monitor monitor;
    std::string error;
    CHECK(UtilityDrought::parseKmz(slurp(dir + "/usdm_20260929.kmz"), monitor, error), error.c_str());
    CHECK(monitor.valid == QDate(2026, 9, 29), "the valid date of the map");
    for (int c = 0; c < 5; c++) {
        CHECK(!monitor.shapes[c].empty(), "every category has shapes");
    }
    std::vector<UtilityDrought::Area> states;
    CHECK(UtilityDrought::parseAreas(slurp(dir + "/cb_2023_us_state_20m.zip"), false, false, states, error), error.c_str());
    CHECK(states.size() == 49, "the 48 states and the District of Columbia");
    const auto raster = UtilityDrought::rasterize(monitor, -125.0, 24.0, -66.5, 50.0);
    const auto all = UtilityDrought::share(raster, UtilityDrought::mask(raster, states));
    // the Monitor's own statistics for the contiguous US that week: none 21.64, D0 78.36, D1 59.38, D2 33.78, D3 11.53, D4 1.59 (percent of the area)
    const double expected[5] = {78.36, 59.38, 33.78, 11.53, 1.59};
    std::printf("contiguous US: none %.2f, D0 %.2f, D1 %.2f, D2 %.2f, D3 %.2f, D4 %.2f   (the Monitor: 21.64, 78.36, 59.38, 33.78, 11.53, 1.59)\n", all.none, all.atLeast[0], all.atLeast[1],
                all.atLeast[2], all.atLeast[3], all.atLeast[4]);
    for (int c = 0; c < 5; c++) {
        CHECK(std::abs(all.atLeast[c] - expected[c]) < 1.5, "the share of the area in each category is within 1.5 points of the Monitor's");
    }
    for (const auto& s : states) {
        if (s.name == "Colorado") {
            std::vector<UtilityDrought::Area> one{s};
            const auto co = UtilityDrought::share(raster, UtilityDrought::mask(raster, one));
            std::printf("Colorado: none %.1f, D0 %.1f, D1 %.1f, D2 %.1f, D3 %.1f, D4 %.1f, bounds %.1f..%.1f, %.1f..%.1f\n", co.none, co.atLeast[0], co.atLeast[1], co.atLeast[2], co.atLeast[3],
                        co.atLeast[4], s.west, s.east, s.south, s.north);
            CHECK(s.west < -108 && s.west > -110 && s.east > -103 && s.east < -101, "Colorado's bounds");
        }
    }
    const auto same = UtilityDrought::change(raster, raster);
    for (const auto v : same) {
        CHECK(v == 0, "no change from a map to itself");
    }
    std::printf("all drought tests passed\n");
    return 0;
}
