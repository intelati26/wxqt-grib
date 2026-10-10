// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "hurricane/UtilityShapefile.h"
#include <cstring>

namespace {
    unsigned long be(const unsigned char * p) {
        return (static_cast<unsigned long>(p[0]) << 24) | (static_cast<unsigned long>(p[1]) << 16) | (static_cast<unsigned long>(p[2]) << 8) | p[3];
    }

    unsigned long le(const unsigned char * p, int n) {
        unsigned long value = 0;
        for (int i = n - 1; i >= 0; i--) {
            value = (value << 8) | p[i];
        }
        return value;
    }

    double real(const unsigned char * p) {
        double value;
        std::memcpy(&value, p, sizeof value);   // little endian doubles, as on every platform this runs on
        return value;
    }

    string trim(const string& s) {
        const auto a = s.find_first_not_of(" \0", 0, 2);
        if (a == string::npos) {
            return "";
        }
        return s.substr(a, s.find_last_not_of(" \0", string::npos, 2) - a + 1);
    }
}

bool UtilityShapefile::parse(const string& shp, const string& dbf, vector<Feature>& features) {
    features.clear();
    const auto * p = reinterpret_cast<const unsigned char *>(shp.data());
    const size_t size = shp.size();
    if (size < 100 || be(p) != 9994UL) {
        return false;
    }
    size_t at = 100;
    while (at + 8 <= size) {
        const size_t length = be(p + at + 4) * 2;   // in 16-bit words
        const size_t start = at + 8;
        if (start + length > size || length < 4) {
            break;
        }
        const auto type = static_cast<int>(le(p + start, 4));
        Feature feature;
        feature.shapeType = type;
        if (type == 1 && length >= 20) {
            feature.parts.push_back({{real(p + start + 4), real(p + start + 12)}});
        } else if ((type == 3 || type == 5) && length >= 44) {
            const size_t partCount = le(p + start + 36, 4);
            const size_t pointCount = le(p + start + 40, 4);
            const size_t partsAt = start + 44;
            const size_t pointsAt = partsAt + partCount * 4;
            if (pointsAt + pointCount * 16 > start + length) {
                return false;
            }
            for (size_t part = 0; part < partCount; part++) {
                const size_t from = le(p + partsAt + part * 4, 4);
                const size_t to = part + 1 < partCount ? le(p + partsAt + (part + 1) * 4, 4) : pointCount;
                vector<std::pair<double, double>> points;
                for (size_t i = from; i < to && i < pointCount; i++) {
                    points.emplace_back(real(p + pointsAt + i * 16), real(p + pointsAt + i * 16 + 8));
                }
                feature.parts.push_back(std::move(points));
            }
        }
        features.push_back(std::move(feature));
        at = start + length;
    }
    // the attribute table: header (record count, header length, record length), field descriptors, then fixed-width records
    const auto * d = reinterpret_cast<const unsigned char *>(dbf.data());
    if (dbf.size() >= 32) {
        const size_t records = le(d + 4, 4);
        const size_t headerLength = le(d + 8, 2);
        const size_t recordLength = le(d + 10, 2);
        struct Field {
            string name;
            size_t length;
        };
        vector<Field> fields;
        for (size_t o = 32; o + 32 <= dbf.size() && d[o] != 0x0D; o += 32) {
            fields.push_back({string{reinterpret_cast<const char *>(d + o), strnlen(reinterpret_cast<const char *>(d + o), 11)}, d[o + 16]});
        }
        for (size_t i = 0; i < records && i < features.size(); i++) {
            const size_t row = headerLength + i * recordLength;
            if (row + recordLength > dbf.size()) {
                break;
            }
            size_t o = row + 1;   // the first byte marks a deleted record
            for (const auto& f : fields) {
                features[i].attributes[f.name] = trim(dbf.substr(o, f.length));
                o += f.length;
            }
        }
    }
    return true;
}
