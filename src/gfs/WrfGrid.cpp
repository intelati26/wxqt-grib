// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "WrfGrid.h"
#include <algorithm>
#include <cmath>

bool WrfGrid::build(const std::vector<float>& lat, const std::vector<float>& lon, int columns, int rows, std::string& error) {
    if (columns < 3 || rows < 3 || lat.size() != static_cast<size_t>(columns) * static_cast<size_t>(rows) || lon.size() != lat.size()) {
        error = "the grid of the run is too small, or its latitudes and longitudes do not match it";
        return false;
    }
    sourceColumns = columns;
    sourceRows = rows;
    const auto at = [&] (const std::vector<float>& v, int i, int j) { return static_cast<double>(v[static_cast<size_t>(j) * static_cast<size_t>(columns) + static_cast<size_t>(i)]); };
    double south = 1e9, east = -1e9, minLon = 1e9, maxLat = -1e9;
    for (size_t k = 0; k < lat.size(); k++) {
        south = std::min(south, static_cast<double>(lat[k]));
        maxLat = std::max(maxLat, static_cast<double>(lat[k]));
        minLon = std::min(minLon, static_cast<double>(lon[k]));
        east = std::max(east, static_cast<double>(lon[k]));
    }
    // the cell of the output: about the spacing of the points in the middle, in degrees of latitude, to a multiple of a hundredth
    const int mi = columns / 2, mj = rows / 2;
    const double dLat = at(lat, mi + 1, mj) - at(lat, mi, mj), dLon = (at(lon, mi + 1, mj) - at(lon, mi, mj)) * std::cos(at(lat, mi, mj) * M_PI / 180.0);
    const double eLat = at(lat, mi, mj + 1) - at(lat, mi, mj), eLon = (at(lon, mi, mj + 1) - at(lon, mi, mj)) * std::cos(at(lat, mi, mj) * M_PI / 180.0);
    const double spacing = std::min(std::hypot(dLat, dLon), std::hypot(eLat, eLon));
    step = std::max(0.01, std::round(spacing * 100.0) / 100.0);
    west = std::floor(minLon / step) * step;
    north = std::ceil(maxLat / step) * step;
    outColumns = static_cast<int>(std::floor((east - west) / step)) + 2;
    outRows = static_cast<int>(std::floor((north - south) / step)) + 2;
    if (outColumns < 2 || outRows < 2 || static_cast<long>(outColumns) * outRows > 40000000L) {
        error = "the grid of the run does not fit a latitude / longitude grid";
        return false;
    }
    // the points, in buckets of about three cells of the output
    const double bucket = step * 3.0;
    const int bc = static_cast<int>(std::floor((east - west) / bucket)) + 2, br = static_cast<int>(std::floor((maxLat - south) / bucket)) + 2;
    std::vector<int> head(static_cast<size_t>(bc) * static_cast<size_t>(br) + 1, 0);
    const auto bucketOf = [&] (double la, double lo) {
        const int c = std::clamp(static_cast<int>(std::floor((lo - west) / bucket)), 0, bc - 1);
        const int r = std::clamp(static_cast<int>(std::floor((la - south) / bucket)), 0, br - 1);
        return static_cast<size_t>(r) * static_cast<size_t>(bc) + static_cast<size_t>(c);
    };
    for (size_t k = 0; k < lat.size(); k++) {
        head[bucketOf(lat[k], lon[k]) + 1]++;
    }
    for (size_t b = 1; b < head.size(); b++) {
        head[b] += head[b - 1];
    }
    std::vector<int> items(lat.size()), fill(head.begin(), head.end() - 1);
    for (size_t k = 0; k < lat.size(); k++) {
        items[static_cast<size_t>(fill[bucketOf(lat[k], lon[k])]++)] = static_cast<int>(k);
    }
    cells.assign(static_cast<size_t>(outColumns) * static_cast<size_t>(outRows), Cell{});
    for (int r = 0; r < outRows; r++) {
        const double la = north - r * step;
        const double cosLat = std::cos(la * M_PI / 180.0);
        for (int c = 0; c < outColumns; c++) {
            const double lo = west + c * step;
            const int bcol = std::clamp(static_cast<int>(std::floor((lo - west) / bucket)), 0, bc - 1);
            const int brow = std::clamp(static_cast<int>(std::floor((la - south) / bucket)), 0, br - 1);
            double best = 1e18;
            int nearest = -1;
            for (int rr = std::max(0, brow - 1); rr <= std::min(br - 1, brow + 1); rr++) {
                for (int cc = std::max(0, bcol - 1); cc <= std::min(bc - 1, bcol + 1); cc++) {
                    const size_t b = static_cast<size_t>(rr) * static_cast<size_t>(bc) + static_cast<size_t>(cc);
                    for (int n = head[b]; n < head[b + 1]; n++) {
                        const int k = items[static_cast<size_t>(n)];
                        const double dy = lat[static_cast<size_t>(k)] - la, dx = (lon[static_cast<size_t>(k)] - lo) * cosLat;
                        const double d = dx * dx + dy * dy;
                        if (d < best) {
                            best = d;
                            nearest = k;
                        }
                    }
                }
            }
            if (nearest < 0 || best > spacing * spacing * 4.0) {
                continue;
            }
            const int i = nearest % columns, j = nearest / columns;
            // the grid's own directions at that point (central differences), and the step from it to the cell along them
            const int i0 = std::max(0, i - 1), i1 = std::min(columns - 1, i + 1), j0 = std::max(0, j - 1), j1 = std::min(rows - 1, j + 1);
            const double xi = (at(lon, i1, j) - at(lon, i0, j)) * cosLat / (i1 - i0), yi = (at(lat, i1, j) - at(lat, i0, j)) / (i1 - i0);
            const double xj = (at(lon, i, j1) - at(lon, i, j0)) * cosLat / (j1 - j0), yj = (at(lat, i, j1) - at(lat, i, j0)) / (j1 - j0);
            const double det = xi * yj - xj * yi;
            if (std::abs(det) < 1e-12) {
                continue;
            }
            const double tx = (lo - at(lon, i, j)) * cosLat, ty = la - at(lat, i, j);
            const double fi = i + (tx * yj - ty * xj) / det, fj = j + (xi * ty - yi * tx) / det;
            if (fi < -0.5 || fi > columns - 0.5 || fj < -0.5 || fj > rows - 0.5) {
                continue;
            }
            Cell cell;
            cell.i = std::clamp(static_cast<int>(std::floor(fi)), 0, columns - 2);
            cell.j = std::clamp(static_cast<int>(std::floor(fj)), 0, rows - 2);
            cell.wx = static_cast<float>(std::clamp(fi - cell.i, 0.0, 1.0));
            cell.wy = static_cast<float>(std::clamp(fj - cell.j, 0.0, 1.0));
            cells[static_cast<size_t>(r) * static_cast<size_t>(outColumns) + static_cast<size_t>(c)] = cell;
        }
    }
    return true;
}

GfsGrid::Grid WrfGrid::apply(const std::vector<float>& values) const {
    GfsGrid::Grid g;
    g.columns = outColumns;
    g.rows = outRows;
    g.lon0 = west;
    g.lat0 = north;
    g.step = step;
    g.values.assign(cells.size(), std::nanf(""));
    if (values.size() != static_cast<size_t>(sourceColumns) * static_cast<size_t>(sourceRows)) {
        return g;
    }
    for (size_t k = 0; k < cells.size(); k++) {
        const auto& c = cells[k];
        if (c.i < 0) {
            continue;
        }
        const auto v = [&] (int i, int j) { return values[static_cast<size_t>(j) * static_cast<size_t>(sourceColumns) + static_cast<size_t>(i)]; };
        const float a = v(c.i, c.j) * (1 - c.wx) + v(c.i + 1, c.j) * c.wx;
        const float b = v(c.i, c.j + 1) * (1 - c.wx) + v(c.i + 1, c.j + 1) * c.wx;
        g.values[k] = a * (1 - c.wy) + b * c.wy;
    }
    return g;
}
