// Tests of the GOES floater graticule finder with a made-up picture: random "clouds" with white one pixel lines every 5 degrees.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include "hurricane/FloaterGeo.h"

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL line %d: %s\n", __LINE__, #c); failures++; } } while (0)

static std::vector<unsigned char> picture(int w, int h, double lonLeft, double latTop, double perDegree, bool lines, unsigned seed) {
    std::srand(seed);
    std::vector<unsigned char> g(static_cast<size_t>(w) * static_cast<size_t>(h));
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            // a smooth scene of bright "clouds" and dark "sea" (no long straight edges) with a little noise
            const double base = 120.0 + 90.0 * std::sin(x * 0.013 + y * 0.007) * std::cos(y * 0.011 - x * 0.005);
            g[static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)] = static_cast<unsigned char>(std::clamp(static_cast<int>(base) + std::rand() % 5 - 2, 0, 255));
        }
    }
    if (lines) {
        for (double lon = std::ceil(lonLeft / 5.0) * 5.0; (lon - lonLeft) * perDegree < w; lon += 5.0) {
            const int x = static_cast<int>(std::floor((lon - lonLeft) * perDegree));
            for (int y = 0; y < h; y++) {
                g[static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)] = 240;
            }
        }
        for (double lat = std::floor(latTop / 5.0) * 5.0; (latTop - lat) * perDegree < h; lat -= 5.0) {
            const int y = static_cast<int>(std::floor((latTop - lat) * perDegree));
            for (int x = 0; x < w; x++) {
                g[static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)] = 240;
            }
        }
    }
    return g;
}

int main() {
    // a box 18 degrees wide centred a little off the storm (-89.58, 24.06) as in a real picture of Isaias
    const double lonLeft = -89.58 - 9.0, latTop = 24.06 + 9.0;
    for (int size : {500, 1000, 2000}) {
        const double ppd = size / 18.0;
        const auto g = picture(size, size, lonLeft, latTop, ppd, true, 7);
        const auto geo = FloaterGeo::locate(g.data(), size, size, -89.8, 24.0);   // the storm's position is only roughly known
        CHECK(geo.fromGrid);
        CHECK(std::abs(geo.pixelsPerDegree - ppd) < ppd * 0.01);
        CHECK(std::abs(geo.lonAtLeft - lonLeft) < 0.05);
        CHECK(std::abs(geo.latAtTop - latTop) < 0.05);
        CHECK(std::abs(geo.x(-90.0) - (-90.0 - lonLeft) * ppd) < 2.0);
        CHECK(std::abs(geo.lon(geo.x(-85.3)) + 85.3) < 1e-9);
    }
    // a wrong guess of up to two degrees still picks the right lines
    {
        const auto g = picture(1000, 1000, lonLeft, latTop, 1000 / 18.0, true, 3);
        const auto geo = FloaterGeo::locate(g.data(), 1000, 1000, -91.4, 25.9);
        CHECK(geo.fromGrid && std::abs(geo.lonAtLeft - lonLeft) < 0.05 && std::abs(geo.latAtTop - latTop) < 0.05);
    }
    // no lines: the box is assumed centred on the storm and 18 degrees across
    {
        const auto g = picture(1000, 1000, lonLeft, latTop, 1000 / 18.0, false, 5);
        const auto geo = FloaterGeo::locate(g.data(), 1000, 1000, -89.8, 24.0);
        CHECK(!geo.fromGrid && std::abs(geo.lonAtLeft + 98.8) < 1e-9 && std::abs(geo.pixelsPerDegree - 1000 / 18.0) < 1e-9);
    }
    // a southern and a western-hemisphere-crossing box
    {
        const double west = 178.0 - 9.0, top = -15.0 + 9.0;   // longitude beyond 180 east: 169 .. 187
        const auto g = picture(1000, 1000, west, top, 1000 / 18.0, true, 11);
        const auto geo = FloaterGeo::locate(g.data(), 1000, 1000, 178.2, -14.8);
        CHECK(geo.fromGrid && std::abs(geo.lonAtLeft - west) < 0.05 && std::abs(geo.latAtTop - top) < 0.05);
    }
    std::printf(failures ? "%d failures\n" : "all floater tests passed\n", failures);
    return failures ? 1 : 0;
}
