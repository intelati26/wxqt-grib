// Tests of the GFS chart numerics: index lines, sampling, vorticity, contours, highs and lows.
#include <cmath>
#include <cstdio>
#include "gfs/GfsGrid.h"

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL line %d: %s\n", __LINE__, #c); failures++; } } while (0)

using namespace GfsGrid;

static Grid make(int columns, int rows, double lon0, double lat0, double step) {
    Grid g;
    g.columns = columns;
    g.rows = rows;
    g.lon0 = lon0;
    g.lat0 = lat0;
    g.step = step;
    g.values.assign(static_cast<size_t>(columns) * static_cast<size_t>(rows), 0.0f);
    return g;
}

int main() {
    const std::string idx =
        "1:0:d=2026100800:PRMSL:mean sea level:6 hour fcst:\n"
        "2:1028159:d=2026100800:HGT:500 mb:6 hour fcst:\n"
        "3:2000000:d=2026100800:APCP:surface:0-6 hour acc fcst:\n";
    const auto records = parseIdx(idx);
    CHECK(records.size() == 3);
    CHECK(records[0].start == 0 && records[0].end == 1028158);
    CHECK(records[2].end == -1 && records[2].forecast == "0-6 hour acc fcst");
    CHECK(find(records, "HGT", "500 mb") && find(records, "HGT", "500 mb")->number == 2);
    CHECK(!find(records, "HGT", "700 mb"));
    // a trailing * matches the start of the forecast text: the running total, whatever it is called ("0-6 hour acc", "0-1 day acc")
    const auto totals = parseIdx("1:0:d=2026100800:APCP:surface:6-9 hour acc fcst:\n2:100:d=2026100800:APCP:surface:0-9 hour acc fcst:\n3:200:d=2026100800:APCP:surface:0-1 day acc fcst:\n");
    CHECK(find(totals, "APCP", "surface", "0-*") && find(totals, "APCP", "surface", "0-*")->number == 2);
    CHECK(find(totals, "APCP", "surface", "6-*")->number == 1);
    CHECK(!find(totals, "APCP", "surface", "12-*"));
    CHECK(find(records, "APCP", "surface", "0-6 hour acc fcst"));

    // sampling: a field equal to the longitude, wrapping on a global grid
    auto g = make(1440, 721, 0.0, 90.0, 0.25);
    for (int r = 0; r < g.rows; r++) {
        for (int c = 0; c < g.columns; c++) {
            g.values[static_cast<size_t>(r) * 1440 + static_cast<size_t>(c)] = static_cast<float>(c * 0.25);
        }
    }
    CHECK(std::abs(g.sample(100.1, 40.0) - 100.1) < 0.01);
    CHECK(std::abs(g.sample(-80.0, 40.0) - 280.0) < 0.01);               // west longitudes wrap
    CHECK(std::isnan(g.sample(10.0, 95.0)));
    CHECK(!std::isnan(g.sample(359.9, 0.0)));

    // vorticity of a solid shear: v rises 10 m/s per degree of longitude, u is zero
    auto u = make(120, 121, 0.0, 70.0, 0.25), v = u;
    for (int r = 0; r < v.rows; r++) {
        for (int c = 0; c < v.columns; c++) {
            v.values[static_cast<size_t>(r) * 120 + static_cast<size_t>(c)] = static_cast<float>(10.0 * c * 0.25);
        }
    }
    auto z = vorticity(u, v);
    const double expected = 10.0 / (3.14159265358979 / 180.0 * 6371229.0 * std::cos(55.0 * 3.14159265358979 / 180.0));   // at 55N
    CHECK(std::abs(z.at(60, 60) / expected - 1.0) < 0.01);
    // and u falling to the north is positive too
    v.values.assign(v.values.size(), 0.0f);
    for (int r = 0; r < u.rows; r++) {
        for (int c = 0; c < u.columns; c++) {
            u.values[static_cast<size_t>(r) * 120 + static_cast<size_t>(c)] = static_cast<float>(-10.0 * (70.0 - r * 0.25));
        }
    }
    z = vorticity(u, v);
    CHECK(z.at(60, 60) > 0.0f);

    // a hill: contours of a cone are closed circles of the right radius, and the hill is one high
    auto h = make(81, 81, -10.0, 10.0, 0.25);
    for (int r = 0; r < h.rows; r++) {
        for (int c = 0; c < h.columns; c++) {
            const double x = -10.0 + c * 0.25, y = 10.0 - r * 0.25;
            h.values[static_cast<size_t>(r) * 81 + static_cast<size_t>(c)] = static_cast<float>(100.0 - std::hypot(x, y) * 5.0);
        }
    }
    const auto lines = contour(h, 75.0, -10.0, -10.0, 10.0, 10.0);   // radius 5 degrees
    CHECK(lines.size() == 1);
    if (!lines.empty()) {
        CHECK(lines[0].size() > 60);
        CHECK(std::abs(lines[0].front().lon - lines[0].back().lon) < 1e-6 && std::abs(lines[0].front().lat - lines[0].back().lat) < 1e-6);   // closed
        double lo = 1e9, hi = -1e9;
        for (const auto& p : lines[0]) {
            const double r = std::hypot(p.lon, p.lat);
            lo = std::min(lo, r);
            hi = std::max(hi, r);
        }
        CHECK(lo > 4.95 && hi < 5.05);
    }
    CHECK(contour(h, 500.0, -10, -10, 10, 10).empty());
    const auto peaks = extremes(h, 2.0, -10, -10, 10, 10);
    CHECK(peaks.size() == 1 && peaks[0].high && std::abs(peaks[0].lon) < 0.01);

    const auto soft = smoothed(h, 2);
    CHECK(soft.at(40, 40) < h.at(40, 40) && soft.at(40, 40) > 95.0f);
    std::printf(failures ? "%d failures\n" : "all GFS tests passed\n", failures);
    return failures ? 1 : 0;
}
