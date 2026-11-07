// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "gfs/OmGrid.h"
#include <algorithm>
#include <cmath>

namespace {
    constexpr double pi = 3.14159265358979323846;

    int ringPoints(int n, int ring) {   // ring 0 is the northernmost
        const int fromPole = ring < n ? ring : 2 * n - 1 - ring;
        return 20 + 4 * fromPole;
    }

    // the 2n Gaussian latitudes, north to south: the roots of the Legendre polynomial of degree 2n, by Newton's method from the usual first guess
    std::vector<double> gaussianLatitudes(int n) {
        const int m = 2 * n;
        std::vector<double> lat(static_cast<size_t>(m));
        for (int i = 0; i < m / 2; i++) {
            double x = std::cos(pi * (i + 0.75) / (m + 0.5));
            for (int iteration = 0; iteration < 100; iteration++) {
                double p0 = 1.0, p1 = x;
                for (int k = 2; k <= m; k++) {
                    const double p2 = ((2.0 * k - 1.0) * x * p1 - (k - 1.0) * p0) / k;
                    p0 = p1;
                    p1 = p2;
                }
                const double derivative = m * (x * p1 - p0) / (x * x - 1.0);
                const double dx = p1 / derivative;
                x -= dx;
                if (std::abs(dx) < 1e-15) {
                    break;
                }
            }
            const double degrees = std::asin(x) * 180.0 / pi;
            lat[static_cast<size_t>(i)] = degrees;
            lat[static_cast<size_t>(m - 1 - i)] = -degrees;
        }
        return lat;
    }
}

size_t OmGrid::gaussianPoints(int n) {
    size_t total = 0;
    for (int ring = 0; ring < 2 * n; ring++) {
        total += static_cast<size_t>(ringPoints(n, ring));
    }
    return total;
}

bool OmGrid::gaussianToRegular(const std::vector<float>& values, int n, double step, double west, double south, double east, double north, GfsGrid::Grid& out, std::string& error) {
    if (values.size() != gaussianPoints(n)) {
        error = "a reduced Gaussian grid O" + std::to_string(n) + " has " + std::to_string(gaussianPoints(n)) + " points, the file " + std::to_string(values.size());
        return false;
    }
    const auto lats = gaussianLatitudes(n);
    std::vector<size_t> start(static_cast<size_t>(2 * n));
    size_t at = 0;
    for (int ring = 0; ring < 2 * n; ring++) {
        start[static_cast<size_t>(ring)] = at;
        at += static_cast<size_t>(ringPoints(n, ring));
    }
    // a value along a ring at a longitude (degrees east of 0), the ring's points evenly spaced and the last one next to the first
    const auto along = [&] (int ring, double lon) {
        const int count = ringPoints(n, ring);
        double x = std::fmod(lon, 360.0);
        if (x < 0) {
            x += 360.0;
        }
        const double position = x / 360.0 * count;
        const int left = static_cast<int>(std::floor(position)) % count;
        const double w = position - std::floor(position);
        const float a = values[start[static_cast<size_t>(ring)] + static_cast<size_t>(left)];
        const float b = values[start[static_cast<size_t>(ring)] + static_cast<size_t>((left + 1) % count)];
        if (a != a) return b;
        if (b != b) return a;
        return static_cast<float>(a + (b - a) * w);
    };
    out.columns = static_cast<int>(std::lround((east - west) / step)) + 1;
    out.rows = static_cast<int>(std::lround((north - south) / step)) + 1;
    out.lon0 = west;
    out.lat0 = north;
    out.step = step;
    out.values.assign(static_cast<size_t>(out.columns) * static_cast<size_t>(out.rows), std::nanf(""));
    int ring = 0;   // the ring just north of (or at) the latitude, walked south as the rows go
    for (int r = 0; r < out.rows; r++) {
        const double lat = north - r * step;
        while (ring < 2 * n - 2 && lats[static_cast<size_t>(ring + 1)] >= lat) {
            ring++;
        }
        // above the first ring or below the last: that ring's values
        const int a = lat >= lats[0] ? 0 : ring;
        const int b = lat >= lats[0] ? 0 : (lat <= lats[static_cast<size_t>(2 * n - 1)] ? 2 * n - 1 : ring + 1);
        const double wLat = a == b ? 0.0 : (lats[static_cast<size_t>(a)] - lat) / (lats[static_cast<size_t>(a)] - lats[static_cast<size_t>(b)]);
        for (int c = 0; c < out.columns; c++) {
            const double lon = west + c * step;
            const float va = along(a, lon);
            const float vb = a == b ? va : along(b, lon);
            out.values[static_cast<size_t>(r) * static_cast<size_t>(out.columns) + static_cast<size_t>(c)] = static_cast<float>(va + (vb - va) * wLat);
        }
    }
    return true;
}
