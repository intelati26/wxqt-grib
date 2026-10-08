// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "gfs/GfsGrid.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <map>
#include <sstream>
#include <utility>

namespace GfsGrid {
namespace {
    constexpr float nan = std::numeric_limits<float>::quiet_NaN();
    constexpr double pi = 3.14159265358979323846;
    constexpr double earthRadius = 6371229.0;

    double wrap(double lon, double from) {   // lon moved into from .. from + 360
        while (lon < from) {
            lon += 360.0;
        }
        while (lon >= from + 360.0) {
            lon -= 360.0;
        }
        return lon;
    }
}

std::vector<IdxRecord> parseIdx(const std::string& text) {
    std::vector<IdxRecord> records;
    std::istringstream in{text};
    std::string line;
    while (std::getline(in, line)) {
        std::vector<std::string> parts;
        size_t from = 0;
        while (true) {
            const auto colon = line.find(':', from);
            if (colon == std::string::npos) {
                parts.push_back(line.substr(from));
                break;
            }
            parts.push_back(line.substr(from, colon - from));
            from = colon + 1;
        }
        if (parts.size() < 6 || parts[0].empty() || !std::isdigit(static_cast<unsigned char>(parts[0][0]))) {
            continue;
        }
        IdxRecord r;
        r.number = std::atoi(parts[0].c_str());
        r.start = std::atoll(parts[1].c_str());
        r.variable = parts[3];
        r.level = parts[4];
        r.forecast = parts[5];
        if (!records.empty()) {
            records.back().end = r.start - 1;
        }
        records.push_back(std::move(r));
    }
    return records;
}

// "" matches anything; "0-*" matches every forecast string starting with "0-"; otherwise the whole string
static bool matches(const std::string& have, const std::string& want) {
    if (want.empty()) {
        return true;
    }
    if (want.back() == '*') {
        return have.compare(0, want.size() - 1, want, 0, want.size() - 1) == 0;
    }
    return have == want;
}

const IdxRecord * find(const std::vector<IdxRecord>& records, const std::string& variable, const std::string& level, const std::string& forecast) {
    for (const auto& r : records) {
        if (r.variable == variable && r.level == level && matches(r.forecast, forecast)) {
            return &r;
        }
    }
    return nullptr;
}

float Grid::sample(double lon, double lat) const {
    if (empty()) {
        return nan;
    }
    const double fy = (lat0 - lat) / step;
    if (fy < 0.0 || fy > rows - 1) {
        return nan;
    }
    double fx = (wrap(lon, lon0) - lon0) / step;
    int x0 = static_cast<int>(std::floor(fx));
    const double tx = fx - x0;
    int x1 = x0 + 1;
    if (global()) {
        x0 %= columns;
        x1 %= columns;
    } else if (x0 < 0 || x1 > columns) {
        return nan;
    } else if (x1 >= columns) {
        x1 = columns - 1;
    }
    const int y0 = static_cast<int>(std::floor(fy));
    const int y1 = std::min(y0 + 1, rows - 1);
    const double ty = fy - y0;
    const float a = at(x0, y0), b = at(x1, y0), c = at(x0, y1), d = at(x1, y1);
    if (std::isnan(a) || std::isnan(b) || std::isnan(c) || std::isnan(d)) {
        return nan;
    }
    return static_cast<float>((a * (1 - tx) + b * tx) * (1 - ty) + (c * (1 - tx) + d * tx) * ty);
}

float Grid::sampleCubic(double lon, double lat) const {
    if (empty()) {
        return nan;
    }
    const double fy = (lat0 - lat) / step;
    if (fy < 0.0 || fy > rows - 1) {
        return nan;
    }
    const double fx = (wrap(lon, lon0) - lon0) / step;
    const int x1 = static_cast<int>(std::floor(fx));
    const int y1 = static_cast<int>(std::floor(fy));
    const double tx = fx - x1, ty = fy - y1;
    const auto weights = [] (double t, double w[4]) {
        w[0] = 0.5 * (-t * t * t + 2 * t * t - t);
        w[1] = 0.5 * (3 * t * t * t - 5 * t * t + 2);
        w[2] = 0.5 * (-3 * t * t * t + 4 * t * t + t);
        w[3] = 0.5 * (t * t * t - t * t);
    };
    double wx[4], wy[4];
    weights(tx, wx);
    weights(ty, wy);
    double sum = 0.0;
    for (int j = 0; j < 4; j++) {
        const int row = std::clamp(y1 - 1 + j, 0, rows - 1);
        double line = 0.0;
        for (int i = 0; i < 4; i++) {
            int col = x1 - 1 + i;
            if (global()) {
                col = ((col % columns) + columns) % columns;
            } else {
                col = std::clamp(col, 0, columns - 1);
            }
            const float value = at(col, row);
            if (std::isnan(value)) {
                return nan;
            }
            line += wx[i] * value;
        }
        sum += wy[j] * line;
    }
    return static_cast<float>(sum);
}

Grid anomaly(const Grid& model, const Grid& reference) {
    Grid out = model;
    for (int row = 0; row < model.rows; row++) {
        const double lat = model.lat0 - row * model.step;
        for (int col = 0; col < model.columns; col++) {
            const size_t i = static_cast<size_t>(row) * static_cast<size_t>(model.columns) + static_cast<size_t>(col);
            const float ref = reference.sampleCubic(model.lon0 + col * model.step, lat);
            out.values[i] = std::isnan(ref) ? nan : model.values[i] - ref;
        }
    }
    return out;
}

Grid speed(const Grid& u, const Grid& v) {
    Grid out = u;
    for (size_t i = 0; i < out.values.size(); i++) {
        out.values[i] = std::hypot(u.values[i], v.values[i]);
    }
    return out;
}

namespace {
    // The two horizontal derivatives the vorticity and divergence share, on the sphere: dx of one field and (1/cos) d(field * cos)/dy of another.
    // mode 0: vorticity  dv/dx - (1/cos) d(u cos)/dy        mode 1: divergence  du/dx + (1/cos) d(v cos)/dy
    Grid spherical(const Grid& u, const Grid& v, bool divergence) {
        Grid out = u;
        std::fill(out.values.begin(), out.values.end(), nan);
        const double dy = earthRadius * pi / 180.0 * u.step;
        for (int row = 1; row < u.rows - 1; row++) {
            const double lat = u.lat0 - row * u.step;
            const double cosLat = std::cos(lat * pi / 180.0);
            if (cosLat < 0.02) {
                continue;
            }
            const double cosNorth = std::cos((lat + u.step) * pi / 180.0), cosSouth = std::cos((lat - u.step) * pi / 180.0);
            const double dx = dy * cosLat;
            for (int col = 0; col < u.columns; col++) {
                const int left = col > 0 ? col - 1 : (u.global() ? u.columns - 1 : col);
                const int right = col < u.columns - 1 ? col + 1 : (u.global() ? 0 : col);
                const double span = (left == col || right == col) ? 1.0 : 2.0;
                const Grid& along = divergence ? u : v;     // differentiated along x
                const Grid& across = divergence ? v : u;    // differentiated along y (with the cosine)
                const double dAlong = (along.at(right, row) - along.at(left, row)) / (span * dx);
                // rows run south: row - 1 is the north neighbor
                const double dAcross = (across.at(col, row - 1) * cosNorth - across.at(col, row + 1) * cosSouth) / (2.0 * dy * cosLat);
                out.values[static_cast<size_t>(row) * static_cast<size_t>(u.columns) + static_cast<size_t>(col)] = static_cast<float>(divergence ? dAlong + dAcross : dAlong - dAcross);
            }
        }
        return out;
    }
}

Grid vorticity(const Grid& u, const Grid& v) {
    return spherical(u, v, false);
}

Grid divergence(const Grid& u, const Grid& v) {
    return spherical(u, v, true);
}

Grid difference(const Grid& a, const Grid& b) {
    Grid out = a;
    for (size_t i = 0; i < out.values.size(); i++) {
        out.values[i] = a.values[i] - b.values[i];
    }
    return out;
}

Grid scaled(const Grid& in, double scale, double offset) {
    Grid out = in;
    for (auto& value : out.values) {
        value = static_cast<float>(value * scale + offset);
    }
    return out;
}

Grid smoothed(const Grid& in, int passes) {
    Grid out = in;
    for (int pass = 0; pass < passes; pass++) {
        Grid next = out;
        for (int row = 0; row < out.rows; row++) {
            for (int col = 0; col < out.columns; col++) {
                double sum = 0.0;
                int count = 0;
                for (int dr = -1; dr <= 1; dr++) {
                    const int r = row + dr;
                    if (r < 0 || r >= out.rows) {
                        continue;
                    }
                    for (int dc = -1; dc <= 1; dc++) {
                        int c = col + dc;
                        if (c < 0 || c >= out.columns) {
                            if (!out.global()) {
                                continue;
                            }
                            c = (c + out.columns) % out.columns;
                        }
                        const float value = out.at(c, r);
                        if (!std::isnan(value)) {
                            sum += value;
                            count++;
                        }
                    }
                }
                next.values[static_cast<size_t>(row) * static_cast<size_t>(out.columns) + static_cast<size_t>(col)] = count ? static_cast<float>(sum / count) : nan;
            }
        }
        out = std::move(next);
    }
    return out;
}

std::vector<Line> contour(const Grid& g, double rawLevel, double west, double south, double east, double north) {
    // a value exactly on the level (common with whole numbers) would make lines meet at a grid point; leaning the level a hair keeps every crossing inside an edge
    const double level = rawLevel + (std::abs(rawLevel) * 1e-6 + 1e-6);
    struct Seg {
        Point a, b;
    };
    std::vector<Seg> segments;
    const int firstRow = std::max(0, static_cast<int>(std::floor((g.lat0 - north) / g.step)));
    const int lastRow = std::min(g.rows - 2, static_cast<int>(std::ceil((g.lat0 - south) / g.step)));
    const int firstCol = static_cast<int>(std::floor((west - g.lon0) / g.step));
    const int lastCol = static_cast<int>(std::ceil((east - g.lon0) / g.step));
    for (int row = firstRow; row <= lastRow; row++) {
        for (int c = firstCol; c <= lastCol; c++) {
            int c0 = c, c1 = c + 1;
            if (g.global()) {
                c0 = ((c0 % g.columns) + g.columns) % g.columns;
                c1 = ((c1 % g.columns) + g.columns) % g.columns;
            } else if (c0 < 0 || c1 >= g.columns) {
                continue;
            }
            // corners: 0 top-left, 1 top-right, 2 bottom-right, 3 bottom-left
            const float v[4] = {g.at(c0, row), g.at(c1, row), g.at(c1, row + 1), g.at(c0, row + 1)};
            if (std::isnan(v[0]) || std::isnan(v[1]) || std::isnan(v[2]) || std::isnan(v[3])) {
                continue;
            }
            const double x0 = g.lon0 + c * g.step, y0 = g.lat0 - row * g.step;
            const double cx[4] = {x0, x0 + g.step, x0 + g.step, x0};
            const double cy[4] = {y0, y0, y0 - g.step, y0 - g.step};
            int index = 0;
            for (int k = 0; k < 4; k++) {
                index |= (v[k] >= level ? 1 : 0) << k;
            }
            if (index == 0 || index == 15) {
                continue;
            }
            const auto edge = [&] (int k) {   // the crossing on the edge from corner k to k + 1
                int a = k, m = (k + 1) % 4;
                // the same arithmetic from both cells that share an edge, so the two ends meet exactly: always from the western (then northern) corner
                if (cx[m] < cx[a] || (cx[m] == cx[a] && cy[m] > cy[a])) {
                    std::swap(a, m);
                }
                const double t = v[m] == v[a] ? 0.5 : (level - v[a]) / (v[m] - v[a]);
                return Point{cx[a] + (cx[m] - cx[a]) * t, cy[a] + (cy[m] - cy[a]) * t};
            };
            const auto add = [&] (int e1, int e2) { segments.push_back({edge(e1), edge(e2)}); };
            switch (index) {
                case 1: case 14: add(3, 0); break;
                case 2: case 13: add(0, 1); break;
                case 3: case 12: add(3, 1); break;
                case 4: case 11: add(1, 2); break;
                case 6: case 9: add(0, 2); break;
                case 7: case 8: add(2, 3); break;
                case 5: case 10: {   // a saddle: decide by the center
                    const bool centerHigh = (v[0] + v[1] + v[2] + v[3]) / 4.0f >= level;
                    if (centerHigh == (index == 5)) {
                        add(3, 2);
                        add(0, 1);
                    } else {
                        add(3, 0);
                        add(1, 2);
                    }
                    break;
                }
                default: break;
            }
        }
    }
    // join the segments end to end (points are matched by their position to a millionth of a degree)
    const auto key = [] (const Point& p) { return std::make_pair(std::llround(p.lon * 1e6), std::llround(p.lat * 1e6)); };
    std::map<std::pair<long long, long long>, std::vector<size_t>> ends;
    for (size_t i = 0; i < segments.size(); i++) {
        ends[key(segments[i].a)].push_back(i);
        ends[key(segments[i].b)].push_back(i);
    }
    std::vector<bool> used(segments.size(), false);
    std::vector<Line> lines;
    const auto next = [&] (const Point& at) -> long long {
        for (auto i : ends[key(at)]) {
            if (!used[i]) {
                return static_cast<long long>(i);
            }
        }
        return -1;
    };
    for (size_t s = 0; s < segments.size(); s++) {
        if (used[s]) {
            continue;
        }
        used[s] = true;
        std::vector<Point> line{segments[s].a, segments[s].b};
        for (int direction = 0; direction < 2; direction++) {
            while (true) {
                const auto i = next(line.back());
                if (i < 0) {
                    break;
                }
                used[static_cast<size_t>(i)] = true;
                const auto& seg = segments[static_cast<size_t>(i)];
                line.push_back(key(seg.a) == key(line.back()) ? seg.b : seg.a);
            }
            std::reverse(line.begin(), line.end());
        }
        lines.push_back(std::move(line));
    }
    return lines;
}

std::vector<Extreme> extremes(const Grid& g, double radius, double west, double south, double east, double north) {
    std::vector<Extreme> found;
    const int reach = std::max(1, static_cast<int>(std::lround(radius / g.step)));
    const int firstRow = std::max(reach, static_cast<int>(std::ceil((g.lat0 - north) / g.step)));
    const int lastRow = std::min(g.rows - 1 - reach, static_cast<int>(std::floor((g.lat0 - south) / g.step)));
    for (int row = firstRow; row <= lastRow; row++) {
        const double lat = g.lat0 - row * g.step;
        for (int col = 0; col < g.columns; col++) {
            const double lon = wrap(g.lon0 + col * g.step, west);
            if (lon > east) {
                continue;
            }
            const float value = g.at(col, row);
            if (std::isnan(value)) {
                continue;
            }
            bool high = true, low = true;
            for (int dr = -reach; dr <= reach && (high || low); dr++) {
                for (int dc = -reach; dc <= reach; dc++) {
                    int c = col + dc;
                    if (c < 0 || c >= g.columns) {
                        if (!g.global()) {
                            continue;
                        }
                        c = (c + g.columns) % g.columns;
                    }
                    if (dr == 0 && dc == 0) {
                        continue;
                    }
                    const float other = g.at(c, row + dr);
                    high = high && value > other;
                    low = low && value < other;
                }
            }
            if (high || low) {
                found.push_back({lon, lat, value, high});
            }
        }
    }
    return found;
}
}
