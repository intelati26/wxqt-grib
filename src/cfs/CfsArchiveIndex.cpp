// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "cfs/CfsArchiveIndex.h"
#include <cmath>
#include <map>
#include <mutex>
#include <sstream>
#include <vector>
#include "objects/URL.h"
#include "util/PermanentCache.h"

namespace {
    uint32_t be32(const uchar * p) {
        return (static_cast<uint32_t>(p[0]) << 24) | (static_cast<uint32_t>(p[1]) << 16) | (static_cast<uint32_t>(p[2]) << 8) | p[3];
    }

    uint64_t be64(const uchar * p) {
        return (static_cast<uint64_t>(be32(p)) << 32) | be32(p + 4);
    }

    std::string nameOf(int discipline, int category, int number) {
        if (discipline == 0) {
            switch (category * 100 + number) {
                case 0: return "TMP";
                case 6: return "DPT";
                case 4: return "TMAX";
                case 5: return "TMIN";
                case 100: return "SPFH";
                case 101: return "RH";
                case 103: return "PWAT";
                case 107: return "PRATE";
                case 300: return "PRES";
                case 301: return "PRMSL";
                case 305: return "HGT";
                case 202: return "UGRD";
                case 203: return "VGRD";
                case 208: return "VVEL";
                default: break;
            }
        }
        return "UNK" + std::to_string(discipline) + "_" + std::to_string(category) + "_" + std::to_string(number);
    }

    std::string levelOf(int type, double value) {
        switch (type) {
            case 1: return "surface";
            case 10: return "entire atmosphere (considered as a single layer)";
            case 100: return std::to_string(static_cast<long>(std::lround(value / 100.0))) + " mb";
            case 101: return "mean sea level";
            case 103: return std::to_string(static_cast<long>(std::lround(value))) + " m above ground";
            default: return "level" + std::to_string(type);
        }
    }

    // the keeper of the indexes: read and written whole, a few KB each
    const PermanentCache& store() {
        static const PermanentCache cache{"climate"};
        return cache;
    }

    std::string keyOf(const std::string& url) {
        const auto at = url.find("/fileServer/");
        std::string name = at == std::string::npos ? url : url.substr(at + 12);
        for (auto& c : name) {
            if (c == '?' || c == ':') {
                c = '_';
            }
        }
        return "cfs/index/" + name;
    }
}

bool CfsArchiveIndex::handles(const std::string& url) {
    const bool ends = (url.size() > 9 && url.compare(url.size() - 9, 9, ".grb2.idx") == 0) || (url.size() > 10 && url.compare(url.size() - 10, 10, ".grib2.idx") == 0);
    return ends && url.find("ncei.noaa.gov/thredds/fileServer/model-cfs-allfile-") != std::string::npos;
}

std::string CfsArchiveIndex::scan(const QByteArray& file) {
    std::ostringstream out;
    const auto * d = reinterpret_cast<const uchar *>(file.constData());
    const size_t size = static_cast<size_t>(file.size());
    size_t offset = 0;
    int n = 0;
    while (offset + 16 < size && d[offset] == 'G' && d[offset + 1] == 'R' && d[offset + 2] == 'I' && d[offset + 3] == 'B') {
        const int discipline = d[offset + 6];
        const uint64_t length = be64(d + offset + 8);
        if (length < 20 || offset + length > size) {
            break;
        }
        size_t p = offset + 16;
        int year = 0, month = 0, day = 0, hour = 0;
        std::vector<std::pair<std::string, std::string>> fields;   // a message may hold several (the wind's U and V): each is a section 4 and what follows it
        while (p + 5 <= offset + length - 4) {
            const uint32_t sectionLength = be32(d + p);
            const int section = d[p + 4];
            if (sectionLength < 5) {
                break;
            }
            if (section == 1 && p + 19 <= size) {
                year = (d[p + 12] << 8) | d[p + 13];
                month = d[p + 14];
                day = d[p + 15];
                hour = d[p + 16];
            } else if (section == 4 && p + 28 <= size) {
                const auto name = nameOf(discipline, d[p + 9], d[p + 10]);
                int scale = d[p + 23];
                if (scale > 127) {
                    scale = -(scale - 128);
                }
                fields.emplace_back(name, levelOf(d[p + 22], static_cast<double>(be32(d + p + 24)) * std::pow(10.0, -scale)));
            }
            p += sectionLength;
            if (section == 8) {
                break;
            }
        }
        char date[16];
        std::snprintf(date, sizeof date, "%04d%02d%02d%02d", year, month, day, hour);
        n++;
        if (fields.empty()) {
            fields.emplace_back("UNK", "surface");
        }
        for (size_t k = 0; k < fields.size(); k++) {
            out << n << (fields.size() > 1 ? "." + std::to_string(k + 1) : std::string{}) << ":" << offset << ":d=" << date << ":" << fields[k].first << ":" << fields[k].second << ":anl:\n";
        }
        offset += length;
    }
    return out.str();
}

QByteArray CfsArchiveIndex::index(const std::string& url) {
    static std::mutex lock;   // one file is scanned at a time: the screen asks for several variables at once and each is a big download
    std::lock_guard<std::mutex> guard{lock};
    const auto key = keyOf(url);
    if (store().has(key)) {
        return QByteArray::fromStdString(store().read(key));
    }
    const auto file = URL::getBytesManaged(url.substr(0, url.size() - 4));
    if (file.size() < 100 || !file.startsWith("GRIB")) {
        return {};
    }
    const auto text = scan(file);
    if (!text.empty()) {
        store().write(key, text);
    }
    return QByteArray::fromStdString(text);
}
