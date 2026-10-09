// Manual checks of the drought shapes: the Monitor's KMZ of 29 September 2026 and the Census state boundaries, against the Monitor's own statistics for that week
#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <QGuiApplication>
#include "drought/DroughtHistory.h"
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
    // thinning, keeping and the bytes between runs
    UtilityDrought::Ring line;   // the edge of a square, 25 points a side, each trembling by 0.0001 degree
    for (int side = 0; side < 4; side++) {
        for (int i = 0; i < 25; i++) {
            const double t = i / 25.0, wobble = 0.0001 * (i % 2);
            const double x[4] = {t, 1.0 + wobble, 1.0 - t, wobble}, y[4] = {wobble, t, 1.0 + wobble, 1.0 - t};
            line.emplace_back(x[side], y[side]);
        }
    }
    line.push_back(line.front());
    CHECK(UtilityDrought::simplify(line, 0.01).size() < 12, "a trembling edge is thinned to its corners");
    CHECK(UtilityDrought::simplify(line, 0.00001).size() == line.size(), "a fine tolerance keeps the points");
    const auto bytes = UtilityDrought::serialize(states);
    std::vector<UtilityDrought::Area> back;
    CHECK(UtilityDrought::deserialize(bytes, back) && back.size() == states.size() && back[5].name == states[5].name && std::abs(back[5].west - states[5].west) < 0.001, "areas survive being stored");
    CHECK(UtilityDrought::newestWarningAreaFile("<a href=\"/source/gis/Shapefiles/WSOM/w_18mr25.zip\"> <a href=\"/source/gis/Shapefiles/WSOM/w_16ap26.zip\">") == "w_16ap26.zip", "the newest warning area file");
    CHECK(UtilityDrought::newestWarningAreaFile("<a href=\"/x/w_30de25.zip\"> <a href=\"/x/w_02ja26.zip\">") == "w_02ja26.zip", "the year decides before the month");
    const auto spc = UtilityDrought::parseSpcPolygons("39.94 -100.51 40.2 -99.0 39.0 -99.5 :38.0 -97.0 38.5 -96.0 37.5 -96.2:", "2347:2348:", "MCD", "Mesoscale Discussion", "SPC");
    CHECK(spc.size() == 2 && spc[0].id == "MCD2347" && spc[1].name == "Mesoscale Discussion 2348" && spc[0].west < -100 && spc[0].north > 40, "SPC polygons");
    const auto sectors = UtilityDrought::spcMesoanalysisSectors();
    CHECK(sectors.size() == 11, "the SPC mesoanalysis sectors");
    for (const auto& s : sectors) {
        if (s.id == "SPCMESO14") {   // Central Plains: Kansas lies in it
            CHECK(s.west < -97 && s.east > -95 && s.south < 37 && s.north > 39, "the Central Plains sector holds Kansas");
        }
        CHECK(s.east - s.west > 8 && s.east - s.west < 35 && s.north - s.south > 6 && s.north - s.south < 20, "a sector is some 10 to 25 degrees across");
    }
    // the history file: rows survive being written and read, empty columns stay empty
    std::vector<DroughtHistory::Row> history(2);
    history[0].month = "2026-08";
    history[0].rain = 41.3;
    history[0].normal = 66.0;
    history[0].departure = -24.7;
    history[0].percent = 63;
    history[0].rainRank = 22;
    history[0].temperature = 1.25;
    history[0].temperatureRank = 88;
    history[1].month = "2026-09";
    history[1].rain = 80.0;
    history[1].mapDate = "20260929";
    for (int k = 0; k < 5; k++) {
        history[1].d[k] = 80.0 - 20.0 * k;
    }
    history[1].dsci = 200;
    const auto csv = DroughtHistory::toCsv("Colorado", history);
    const auto again = DroughtHistory::fromCsv(csv);
    CHECK(again.size() == 2 && again[0].month == "2026-08" && std::abs(again[0].rain - 41.3) < 0.01 && std::abs(again[0].temperature - 1.25) < 0.001 && !again[0].hasDrought(), "history rows are read back");
    CHECK(again[1].hasDrought() && !again[1].hasWeather() == false && std::isnan(again[1].departure) && again[1].mapDate == "20260929" && std::abs(again[1].d[2] - 40.0) < 0.01, "empty columns stay empty, filled ones come back");
    CHECK(csv.find("# wxqt drought history: Colorado") == 0 && csv.find("\nmonth,rain_mm") != std::string::npos, "the file says what it is and has a header");
    const auto same = UtilityDrought::change(raster, raster);
    for (const auto v : same) {
        CHECK(v == 0, "no change from a map to itself");
    }
    std::printf("all drought tests passed\n");
    return 0;
}
