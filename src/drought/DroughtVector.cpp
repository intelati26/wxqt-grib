// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "drought/DroughtVector.h"
#include <cmath>
#include <cstdint>

namespace {
    void putVarint(std::string& out, uint64_t value) {
        while (value >= 0x80) {
            out.push_back(static_cast<char>((value & 0x7f) | 0x80));
            value >>= 7;
        }
        out.push_back(static_cast<char>(value));
    }

    void putSigned(std::string& out, int64_t value) {
        putVarint(out, value < 0 ? (static_cast<uint64_t>(-(value + 1)) << 1) | 1 : static_cast<uint64_t>(value) << 1);
    }

    bool getVarint(const std::string& in, size_t& at, uint64_t& value) {
        value = 0;
        for (int shift = 0; shift < 64 && at < in.size(); shift += 7) {
            const auto b = static_cast<unsigned char>(in[at++]);
            value |= static_cast<uint64_t>(b & 0x7f) << shift;
            if (!(b & 0x80)) {
                return true;
            }
        }
        return false;
    }

    bool getSigned(const std::string& in, size_t& at, int64_t& value) {
        uint64_t raw = 0;
        if (!getVarint(in, at, raw)) {
            return false;
        }
        value = (raw & 1) ? -static_cast<int64_t>(raw >> 1) - 1 : static_cast<int64_t>(raw >> 1);
        return true;
    }

    // the ring thinned, snapped and without repeated points; empty when fewer than 4 points (a closed ring of a triangle) are left
    std::vector<std::pair<int64_t, int64_t>> shrink(const UtilityDrought::Ring& ring) {
        const auto thin = UtilityDrought::simplify(ring, DroughtVector::tolerance);
        std::vector<std::pair<int64_t, int64_t>> points;
        for (const auto& p : thin) {
            const std::pair<int64_t, int64_t> q{std::llround(p.first / DroughtVector::quantum), std::llround(p.second / DroughtVector::quantum)};
            if (points.empty() || points.back() != q) {
                points.push_back(q);
            }
        }
        return points.size() < 4 ? std::vector<std::pair<int64_t, int64_t>>{} : points;
    }
}

namespace DroughtVector {
    // "DV1", the valid date (yyyymmdd, 4 bytes), then for each category D0 to D4: the polygons (count), each its rings (count: the outer ring, then the holes), each ring its points (count, the
    // first as whole numbers of quanta, then the differences), all varints, the numbers signed ones
    std::string encode(const UtilityDrought::Monitor& monitor) {
        std::string out{"DV1"};
        const auto date = static_cast<uint32_t>(monitor.valid.year() * 10000 + monitor.valid.month() * 100 + monitor.valid.day());
        for (int i = 0; i < 4; ++i) {
            out.push_back(static_cast<char>((date >> (8 * i)) & 0xff));
        }
        for (const auto& category : monitor.shapes) {
            std::string body;
            uint64_t polygons = 0;
            for (const auto& polygon : category) {
                std::string shape;
                uint64_t rings = 0;
                for (size_t r = 0; r < polygon.size(); ++r) {
                    const auto points = shrink(polygon[r]);
                    if (points.empty()) {
                        if (r == 0) {
                            break;   // (a polygon without an outer ring is nothing)
                        }
                        continue;
                    }
                    putVarint(shape, points.size());
                    int64_t x = 0, y = 0;
                    for (const auto& p : points) {
                        putSigned(shape, p.first - x);
                        putSigned(shape, p.second - y);
                        x = p.first;
                        y = p.second;
                    }
                    ++rings;
                }
                if (rings > 0) {
                    putVarint(body, rings);
                    body += shape;
                    ++polygons;
                }
            }
            putVarint(out, polygons);
            out += body;
        }
        return out;
    }

    bool decode(const std::string& bytes, UtilityDrought::Monitor& out) {
        if (bytes.size() < 8 || bytes.compare(0, 3, "DV1") != 0) {
            return false;
        }
        uint32_t date = 0;
        for (int i = 0; i < 4; ++i) {
            date |= static_cast<uint32_t>(static_cast<unsigned char>(bytes[3 + static_cast<size_t>(i)])) << (8 * i);
        }
        out.valid = QDate{static_cast<int>(date / 10000), static_cast<int>(date / 100 % 100), static_cast<int>(date % 100)};
        size_t at = 7;
        for (auto& category : out.shapes) {
            category.clear();
            uint64_t polygons = 0;
            if (!getVarint(bytes, at, polygons)) {
                return false;
            }
            for (uint64_t p = 0; p < polygons; ++p) {
                uint64_t rings = 0;
                if (!getVarint(bytes, at, rings)) {
                    return false;
                }
                UtilityDrought::Polygon polygon;
                for (uint64_t r = 0; r < rings; ++r) {
                    uint64_t count = 0;
                    if (!getVarint(bytes, at, count) || count > bytes.size()) {
                        return false;
                    }
                    UtilityDrought::Ring ring;
                    ring.reserve(count);
                    int64_t x = 0, y = 0;
                    for (uint64_t i = 0; i < count; ++i) {
                        int64_t dx = 0, dy = 0;
                        if (!getSigned(bytes, at, dx) || !getSigned(bytes, at, dy)) {
                            return false;
                        }
                        x += dx;
                        y += dy;
                        ring.emplace_back(static_cast<double>(x) * quantum, static_cast<double>(y) * quantum);
                    }
                    polygon.push_back(std::move(ring));
                }
                category.push_back(std::move(polygon));
            }
        }
        return true;
    }
}
