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
#include "util/Activity.h"
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
                case 600 + 1: return "TCDC";
                case 200: return "WDIR";
                case 201: return "WIND";
                case 222: return "GUST";
                case 1900: return "VIS";
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
            case 200: return "entire atmosphere (considered as a single layer)";
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
    if (url.size() > 4 && url.compare(url.size() - 4, 4, ".idx") == 0 && (url.find("noaa-rtma-pds.") != std::string::npos || url.find("noaa-urma-pds.") != std::string::npos)) {
        return true;   // the RTMA and URMA analyses of the open data buckets: an index where the bucket has one, else made from the messages' headers
    }
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

// the index of a file made from the head of each message (a few hundred bytes of each, by ranged reads): the messages follow one another, each says how long it is
std::string CfsArchiveIndex::scanRemote(const std::string& fileUrl) {
    std::ostringstream out;
    unsigned long long offset = 0;
    for (int n = 1; n <= 80; n++) {
        const auto part = URL::getBytesRangeManaged(fileUrl, static_cast<long long>(offset), static_cast<long long>(offset) + 399);
        if (part.size() < 40 || !part.startsWith("GRIB")) {
            break;
        }
        const auto * d = reinterpret_cast<const uchar *>(part.constData());
        const size_t size = static_cast<size_t>(part.size());
        const int discipline = d[6];
        const uint64_t length = be64(d + 8);
        if (length < 20) {
            break;
        }
        int year = 0, month = 0, day = 0, hour = 0;
        std::string name = "UNK", level = "surface";
        size_t p = 16;
        while (p + 5 <= size) {
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
                name = nameOf(discipline, d[p + 9], d[p + 10]);
                int scale = d[p + 23];
                if (scale > 127) {
                    scale = -(scale - 128);
                }
                level = levelOf(d[p + 22], static_cast<double>(be32(d + p + 24)) * std::pow(10.0, -scale));
                break;
            }
            p += sectionLength;
        }
        char date[16];
        std::snprintf(date, sizeof date, "%04d%02d%02d%02d", year, month, day, hour);
        out << n << ":" << offset << ":d=" << date << ":" << name << ":" << level << ":anl:\n";
        offset += length;
    }
    return out.str();
}

QByteArray CfsArchiveIndex::index(const std::string& url) {
    if (url.find("noaa-rtma-pds.") != std::string::npos || url.find("noaa-urma-pds.") != std::string::npos) {
        const auto real = URL::getBytesManaged(url);   // (the bucket's own index, where it has one)
        if (real.startsWith("1:")) {
            return real;
        }
        static std::mutex scanLock;
        std::lock_guard<std::mutex> guard{scanLock};
        const auto key = keyOf(url);
        if (store().has(key)) {
            return QByteArray::fromStdString(store().read(key));
        }
        const auto text = scanRemote(url.substr(0, url.size() - 4));
        if (!text.empty()) {
            store().write(key, text);
        }
        return QByteArray::fromStdString(text);
    }
    static std::mutex lock;   // one file is scanned at a time: the screen asks for several variables at once and each is a big download
    std::lock_guard<std::mutex> guard{lock};
    const auto key = keyOf(url);
    if (store().has(key)) {
        return QByteArray::fromStdString(store().read(key));
    }
    // the file in pieces, so that the status line can say how far it is (it is 14 to 60 MB, the first time for each variable and month)
    const auto fileUrl = url.substr(0, url.size() - 4);
    const auto slash = fileUrl.rfind('/');
    const std::string what = slash == std::string::npos ? fileUrl : fileUrl.substr(slash + 1);
    struct Note {
        ~Note() { Activity::setNote({}); }
    } note;
    QByteArray file;
    constexpr long long piece = 4LL * 1024 * 1024;
    while (true) {
        Activity::setNote("reading NCEI's " + what + " (the first time for this month, kept after): " + std::to_string(file.size() / (1024 * 1024)) + " MB so far");
        const auto part = URL::getBytesRangeManaged(fileUrl, file.size(), file.size() + piece - 1);
        if (part.isEmpty() || (file.isEmpty() && !part.startsWith("GRIB"))) {
            break;
        }
        file += part;
        if (part.size() < piece) {
            break;
        }
    }
    if (file.size() < 100 || !file.startsWith("GRIB")) {
        return {};
    }
    Activity::setNote("scanning " + what + " for its messages...");
    const auto text = scan(file);
    if (!text.empty()) {
        store().write(key, text);
    }
    return QByteArray::fromStdString(text);
}
