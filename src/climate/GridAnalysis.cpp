// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "climate/GridAnalysis.h"
#include <algorithm>
#include <cmath>
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
    const double reachDegrees = spec.reachKm / kmPerDegree;
    const double minCos = std::max(0.2, std::cos(std::max(std::abs(spec.south), std::abs(spec.north)) * M_PI / 180.0));
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
    const double r2 = spec.reachKm * spec.reachKm;
    for (int row = 0; row < field.rows; row++) {
        const double lat = spec.north - (row + 0.5) * spec.step;
        const double cosLat = std::max(0.2, std::cos(lat * M_PI / 180.0));
        const int spanLon = static_cast<int>(std::ceil(reachDegrees / cosLat));
        const int spanLat = static_cast<int>(std::ceil(reachDegrees));
        for (int column = 0; column < field.columns; column++) {
            const double lon = spec.west + (column + 0.5) * spec.step;
            int bx, by;
            if (!bucketOf(lat, lon, bx, by)) {
                continue;
            }
            double sum = 0.0, weights = 0.0, nearest = 1e9;
            int count = 0;
            for (int y = std::max(0, by - spanLat); y <= std::min(bucketsLat - 1, by + spanLat); y++) {
                for (int x = std::max(0, bx - spanLon); x <= std::min(bucketsLon - 1, bx + spanLon); x++) {
                    for (const int index : buckets[static_cast<size_t>(y) * bucketsLon + x]) {
                        const auto& p = points[static_cast<size_t>(index)];
                        const double dy = (p.lat - lat) * kmPerDegree;
                        const double dx = (p.lon - lon) * kmPerDegree * cosLat;
                        const double d2 = dx * dx + dy * dy;
                        if (d2 > r2) {
                            continue;
                        }
                        const double w = (r2 - d2) / (r2 + d2);
                        sum += w * p.value;
                        weights += w;
                        nearest = std::min(nearest, std::sqrt(d2));
                        count++;
                    }
                }
            }
            if (count >= spec.fewest && nearest <= spec.nearestKm && weights > 0.0) {
                field.values[static_cast<size_t>(row) * field.columns + column] = static_cast<float>(sum / weights);
            }
        }
    }
    return field;
}
