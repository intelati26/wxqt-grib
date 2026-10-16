// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "climate/GridAnalysis.h"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <limits>

namespace {
    const double kmPerDegree = 111.195;
}

float GridAnalysis::Field::at(double lat, double lon) const {
    if (values.empty()) {
        return std::numeric_limits<float>::quiet_NaN();
    }
    const int column = static_cast<int>(std::floor((lon - spec.west) / spec.step));
    const int row = static_cast<int>(std::floor((spec.north - lat) / spec.step));
    if (column < 0 || column >= columns || row < 0 || row >= rows) {
        return std::numeric_limits<float>::quiet_NaN();
    }
    return values[static_cast<size_t>(row) * columns + column];
}

GridAnalysis::Field GridAnalysis::analyze(const std::vector<Point>& points, const Spec& spec) {
    Field field;
    field.spec = spec;
    field.columns = std::max(1, static_cast<int>(std::ceil((spec.east - spec.west) / spec.step)));
    field.rows = std::max(1, static_cast<int>(std::ceil((spec.north - spec.south) / spec.step)));
    field.values.assign(static_cast<size_t>(field.columns) * field.rows, std::numeric_limits<float>::quiet_NaN());
    if (points.empty()) {
        return field;
    }
    // the points in one degree buckets, so that a cell looks at its neighbourhood only
    // the widest reach of the fills: they may carry a station farther than the analysis proper does
    const double fillReach = spec.fillGapsKm > 0.0 ? std::max(spec.reachKm, spec.fillGapsKm * 1.8) : spec.reachKm;
    const double reachDegrees = fillReach / kmPerDegree;
    const double minCos = std::max(0.2, std::cos(std::max(std::abs(spec.south), std::abs(spec.north)) * std::numbers::pi / 180.0));
    const int padLon = static_cast<int>(std::ceil(reachDegrees / minCos)) + 1, padLat = static_cast<int>(std::ceil(reachDegrees)) + 1;
    const int bucketsLon = static_cast<int>(std::ceil(spec.east - spec.west)) + 2 * padLon + 1;
    const int bucketsLat = static_cast<int>(std::ceil(spec.north - spec.south)) + 2 * padLat + 1;
    std::vector<std::vector<int>> buckets(static_cast<size_t>(bucketsLon) * bucketsLat);
    const auto bucketOf = [&] (double lat, double lon, int& bx, int& by) {
        bx = static_cast<int>(std::floor(lon - spec.west)) + padLon;
        by = static_cast<int>(std::floor(lat - spec.south)) + padLat;
        return bx >= 0 && bx < bucketsLon && by >= 0 && by < bucketsLat;
    };
    for (size_t i = 0; i < points.size(); i++) {
        int bx, by;
        if (bucketOf(points[i].lat, points[i].lon, bx, by)) {
            buckets[static_cast<size_t>(by) * bucketsLon + bx].push_back(static_cast<int>(i));
        }
    }
    // the value of one cell: from the stations within `base` (longer where they are far apart, up to the widest reach), or nothing when too few / too far
    const auto cell = [&] (int row, int column, double base, double nearestLimit, int fewest, double maxReach, float& out) {
        const double r2 = maxReach * maxReach;
        const double degrees = maxReach / kmPerDegree;
        const double lat = spec.north - (row + 0.5) * spec.step;
        const double cosLat = std::max(0.2, std::cos(lat * std::numbers::pi / 180.0));
        const int spanLon = static_cast<int>(std::ceil(degrees / cosLat));
        const int spanLat = static_cast<int>(std::ceil(degrees));
        const double lon = spec.west + (column + 0.5) * spec.step;
        int bx, by;
        if (!bucketOf(lat, lon, bx, by)) {
            return false;
        }
        thread_local std::vector<std::pair<double, int>> near;
        near.clear();
        for (int y = std::max(0, by - spanLat); y <= std::min(bucketsLat - 1, by + spanLat); y++) {
            for (int x = std::max(0, bx - spanLon); x <= std::min(bucketsLon - 1, bx + spanLon); x++) {
                for (const int index : buckets[static_cast<size_t>(y) * bucketsLon + x]) {
                    const auto& p = points[static_cast<size_t>(index)];
                    const double dy = (p.lat - lat) * kmPerDegree;
                    const double dx = (p.lon - lon) * kmPerDegree * cosLat;
                    const double d2 = dx * dx + dy * dy;
                    if (d2 <= r2) {
                        near.push_back({std::sqrt(d2), index});
                    }
                }
            }
        }
        if (near.empty()) {
            return false;
        }
        double reach = maxReach;
        if (base > 0.0) {
            const size_t k = std::min<size_t>(near.size(), 4);
            std::nth_element(near.begin(), near.begin() + static_cast<long>(k - 1), near.end());
            reach = std::clamp(std::max(base, near[k - 1].first * 1.15), base, maxReach);
        }
        const double rr = reach * reach;
        double sum = 0.0, weights = 0.0, nearest = 1e9;
        int count = 0;
        for (const auto& [d, index] : near) {
            if (d > reach) {
                continue;
            }
            const double w = (rr - d * d) / (rr + d * d);
            sum += w * points[static_cast<size_t>(index)].value;
            weights += w;
            nearest = std::min(nearest, d);
            count++;
        }
        if (count < fewest || nearest > nearestLimit || weights <= 0.0) {
            return false;
        }
        out = static_cast<float>(sum / weights);
        return true;
    };
    std::vector<char> have(field.values.size(), 0);
    for (int row = 0; row < field.rows; row++) {
        for (int column = 0; column < field.columns; column++) {
            float value;
            if (cell(row, column, spec.baseKm, spec.nearestKm, spec.fewest, spec.reachKm, value)) {
                field.values[static_cast<size_t>(row) * field.columns + column] = value;
                have[static_cast<size_t>(row) * field.columns + column] = 1;
            }
        }
    }
    if (spec.fillGapsKm > 0.0) {
        // the holes: empty cells that cannot be reached from the edge of the grid through empty cells (a gap between stations that are far apart is surrounded by field; the sea and
        // the neighbouring countries are not). One that is bigger than a lake of about this reach (Lake Michigan) is left as it is. The wide reach fills them; the outer edge does not move.
        const size_t total = have.size();
        std::vector<char> outside(total, 0);
        std::vector<size_t> stack;
        const auto seed = [&] (int row, int column) {
            const size_t i = static_cast<size_t>(row) * field.columns + column;
            if (!have[i] && !outside[i]) {
                outside[i] = 1;
                stack.push_back(i);
            }
        };
        for (int column = 0; column < field.columns; column++) {
            seed(0, column);
            seed(field.rows - 1, column);
        }
        for (int row = 0; row < field.rows; row++) {
            seed(row, 0);
            seed(row, field.columns - 1);
        }
        while (!stack.empty()) {
            const size_t i = stack.back();
            stack.pop_back();
            const int row = static_cast<int>(i / field.columns), column = static_cast<int>(i % field.columns);
            if (row > 0) { seed(row - 1, column); }
            if (row + 1 < field.rows) { seed(row + 1, column); }
            if (column > 0) { seed(row, column - 1); }
            if (column + 1 < field.columns) { seed(row, column + 1); }
        }
        const double cellKm2 = spec.step * kmPerDegree * spec.step * kmPerDegree * std::cos(((spec.north + spec.south) / 2.0) * std::numbers::pi / 180.0);
        const size_t largest = static_cast<size_t>(std::numbers::pi * (spec.fillGapsKm * 0.75) * (spec.fillGapsKm * 0.75) / std::max(1e-9, cellKm2));
        std::vector<char> seen(total, 0);
        for (size_t start = 0; start < total; start++) {
            if (have[start] || outside[start] || seen[start]) {
                continue;
            }
            std::vector<size_t> hole;   // one enclosed hole
            stack.push_back(start);
            seen[start] = 1;
            while (!stack.empty()) {
                const size_t i = stack.back();
                stack.pop_back();
                hole.push_back(i);
                const int row = static_cast<int>(i / field.columns), column = static_cast<int>(i % field.columns);
                const auto push = [&] (int r, int c) {
                    const size_t j = static_cast<size_t>(r) * field.columns + c;
                    if (!have[j] && !outside[j] && !seen[j]) {
                        seen[j] = 1;
                        stack.push_back(j);
                    }
                };
                if (row > 0) { push(row - 1, column); }
                if (row + 1 < field.rows) { push(row + 1, column); }
                if (column > 0) { push(row, column - 1); }
                if (column + 1 < field.columns) { push(row, column + 1); }
            }
            if (hole.size() > largest) {
                continue;
            }
            for (const size_t i : hole) {
                float value;
                if (cell(static_cast<int>(i / field.columns), static_cast<int>(i % field.columns), 0.0, 1e9, 1, fillReach, value)) {
                    field.values[i] = value;
                }
            }
        }
        // the bays of the edge (each outermost station carries a circle, and where stations are far apart the edge falls back, as in the north-west of North Dakota): a closing with a
        // round element (the covered cells grown by a radius, then shrunk by it, by distances): bays and notches narrower than twice the radius fill, the outline stays round and nothing
        // is taken away
        const int radius3 = std::max(3, static_cast<int>(std::lround(spec.fillGapsKm * 1.45 / (spec.step * kmPerDegree) * 3.0)));   // in thirds of a cell (the distance below counts 3 per cell)
        const auto distance = [&] (const std::vector<char>& seeds, bool outsideIsSeed) {   // chamfer 3-4: the distance to the nearest seed, 3 per cell and 4 per diagonal step
            const int inf = 1 << 28;
            std::vector<int> d(total, inf);
            const auto at = [&] (int r, int c) -> int { return r < 0 || r >= field.rows || c < 0 || c >= field.columns ? (outsideIsSeed ? 0 : inf) : d[static_cast<size_t>(r) * field.columns + c]; };
            for (size_t i = 0; i < total; i++) {
                if (seeds[i]) {
                    d[i] = 0;
                }
            }
            for (int r = 0; r < field.rows; r++) {
                for (int c = 0; c < field.columns; c++) {
                    int& v = d[static_cast<size_t>(r) * field.columns + c];
                    v = std::min({v, at(r, c - 1) + 3, at(r - 1, c) + 3, at(r - 1, c - 1) + 4, at(r - 1, c + 1) + 4});
                }
            }
            for (int r = field.rows - 1; r >= 0; r--) {
                for (int c = field.columns - 1; c >= 0; c--) {
                    int& v = d[static_cast<size_t>(r) * field.columns + c];
                    v = std::min({v, at(r, c + 1) + 3, at(r + 1, c) + 3, at(r + 1, c + 1) + 4, at(r + 1, c - 1) + 4});
                }
            }
            return d;
        };
        std::vector<char> covered(total);
        for (size_t i = 0; i < total; i++) {
            covered[i] = !std::isnan(field.values[i]) ? 1 : 0;
        }
        const auto toCovered = distance(covered, false);
        std::vector<char> grown(total), notGrown(total);
        for (size_t i = 0; i < total; i++) {
            grown[i] = toCovered[i] <= radius3 ? 1 : 0;
            notGrown[i] = grown[i] ? 0 : 1;
        }
        const auto toGap = distance(notGrown, false);   // beyond the grid is not a gap: the grid's own edge does not eat the field
        for (size_t i = 0; i < total; i++) {
            if (std::isnan(field.values[i]) && grown[i] && toGap[i] > radius3) {   // inside the closing, not covered yet: but no farther from a station than the closing's own reach
                float value;
                if (cell(static_cast<int>(i / field.columns), static_cast<int>(i % field.columns), 0.0, spec.fillGapsKm * 1.8, 1, fillReach, value)) {
                    field.values[i] = value;
                }
            }
        }
    }
    return field;
}
