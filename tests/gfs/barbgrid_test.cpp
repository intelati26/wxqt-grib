// Tests of the barb grid (BarbGrid): the step grows as the picture zooms out, the rows toward the poles keep every second, third ... point, and the nearest node of a place is a point of the grid.
#include <QSettings>
#include "util/MyApplication.h"
#include <cmath>
#include <cstdio>
#include <vector>
#include "gfs/BarbGrid.h"

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL line %d: %s\n", __LINE__, #c); failures++; } } while (0)

int main() {
    MyApplication::preferences = new QSettings{"/tmp/wxqt_barbgrid_test_prefs.ini", QSettings::IniFormat};
    MyApplication::preferences->clear();
    // the step: no finer than the barbs can be kept apart, and coarser as the pixels per degree fall
    double last = 0.0;
    for (const double perDegree : {200.0, 100.0, 50.0, 20.0, 10.0, 4.0, 1.0}) {
        const auto look = BarbGrid::look(60.0, perDegree);
        CHECK(look.step * perDegree >= look.spacing * 0.92 - 1e-9 || look.step == 30.0);   // (30 degrees is the coarsest: below that many pixels a degree the barbs come closer)
        CHECK(look.step >= last);
        last = look.step;
    }
    CHECK(BarbGrid::look(360.0, 3.0).length < BarbGrid::look(60.0, 3.0).length);   // smaller on the whole earth
    CHECK(BarbGrid::look(60.0, 50.0).length == BarbGrid::look(60.0, 5.0).length);
    // the rows: every point at the equator, every second at 60 degrees (cos 0.5), more toward the pole, never more than 8
    CHECK(BarbGrid::lonStep(2.0, 0.0) == 2.0);
    CHECK(BarbGrid::lonStep(2.0, 60.0) == 4.0);
    CHECK(BarbGrid::lonStep(2.0, 89.0) == 16.0);
    CHECK(BarbGrid::lonStep(2.0, -60.0) == BarbGrid::lonStep(2.0, 60.0));
    // the nearest node is a point of the grid, and a point of the grid is its own nearest
    for (const double step : {1.0, 2.5, 5.0}) {
        for (const double lat : {-47.3, 0.4, 33.3, 60.1}) {
            for (const double lon : {-120.2, -0.4, 77.7}) {
                const auto [la, lo] = BarbGrid::nearestNode(step, lat, lon);
                CHECK(std::abs(la / step - std::round(la / step)) < 1e-9);
                const double ls = BarbGrid::lonStep(step, la);
                CHECK(std::abs(lo / ls - std::round(lo / ls)) < 1e-9);
                CHECK(std::abs(la - lat) <= step / 2.0 + 1e-9);
                const auto again = BarbGrid::nearestNode(step, la, lo);
                CHECK(again.first == la && again.second == lo);
            }
        }
    }
    // the walk: the same points, in the same order, as the loops it replaced (the picture's over its box, the map layer's and the globe's over the world)
    for (const double step : {0.5, 1.0, 2.5, 5.0, 10.0, 30.0}) {
        std::vector<std::pair<double, double>> old, now;
        for (double lat = std::ceil(-85.0 / step) * step; lat <= 85.0; lat += step) {
            const double ls = BarbGrid::lonStep(step, lat);
            for (double lon = std::ceil(-180.0 / ls) * ls; lon < 180.0 - 1e-6; lon += ls) {
                old.emplace_back(lat, lon);
            }
        }
        BarbGrid::forEachWorldNode(step, [&now] (double lat, double lon) { now.emplace_back(lat, lon); });
        CHECK(!old.empty() && old == now);
        old.clear();
        now.clear();
        const double south = -12.3, north = 63.9, west = -141.7, east = -47.2;   // a picture's box
        for (double lat = std::ceil(std::max(-85.0, south) / step) * step; lat <= std::min(85.0, north); lat += step) {
            const double ls = BarbGrid::lonStep(step, lat);
            for (double lon = std::ceil(west / ls) * ls; lon <= east; lon += ls) {
                old.emplace_back(lat, lon);
            }
        }
        BarbGrid::forEachNode(step, south, north, west, east, [&now] (double lat, double lon) { now.emplace_back(lat, lon); });
        CHECK(!old.empty() && old == now);
        for (const auto& [lat, lon] : now) {
            CHECK(lat >= south - 1e-9 - step && lat <= north + 1e-9 && lon >= west - 1e-9 && lon <= east + 1e-9);
        }
    }
    CHECK(BarbGrid::nearestNode(5.0, 31.0, 11.0) == std::make_pair(30.0, 10.0));
    std::printf(failures == 0 ? "OK\n" : "FAILED: %d\n", failures);
    return failures == 0 ? 0 : 1;
}
