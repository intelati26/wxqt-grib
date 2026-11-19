// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "gfs/GribScanIndex.h"
#include <chrono>
#include <cmath>
#include <cstdint>
#include <map>
#include <mutex>
#include <sstream>
#include <vector>
#include <QDateTime>
#include "objects/NetManager.h"
#include "util/Activity.h"
#include "util/PermanentCache.h"

namespace {
    uint32_t be32(const uchar * p) {
        return (static_cast<uint32_t>(p[0]) << 24) | (static_cast<uint32_t>(p[1]) << 16) | (static_cast<uint32_t>(p[2]) << 8) | p[3];
    }
    uint64_t be64(const uchar * p) {
        return (static_cast<uint64_t>(be32(p)) << 32) | be32(p + 4);
    }
    int be16(const uchar * p) {
        return (p[0] << 8) | p[1];
    }

    // GRIB2's discipline / category / number to the names NOAA's inventories (wgrib2) use. What is not here is named from its numbers ("var0_13_194"): the file's own words are in its message, but
    // the names the charts ask for are these, so a parameter that matters is added here when a chart needs it.
    std::string nameOf(int discipline, int category, int number) {
        static const std::map<int, const char *> names{
            // discipline 0: meteorological products. key = discipline * 1000000 + category * 1000 + number
            {0, "TMP"}, {1, "VTMP"}, {2, "POT"}, {3, "EPOT"}, {4, "TMAX"}, {5, "TMIN"}, {6, "DPT"}, {7, "DEPR"}, {10, "LHTFL"}, {11, "SHTFL"}, {21, "APTMP"},
            {1000, "SPFH"}, {1001, "RH"}, {1002, "MIXR"}, {1003, "PWAT"}, {1007, "PRATE"}, {1008, "APCP"}, {1010, "ACPCP"}, {1011, "SNOD"}, {1013, "WEASD"},
            {1022, "CLMR"}, {1023, "ICMR"}, {1024, "RWMR"}, {1025, "SNMR"}, {1192, "CRAIN"}, {1193, "CFRZR"}, {1194, "CICEP"}, {1195, "CSNOW"},
            {2000, "WDIR"}, {2001, "WIND"}, {2002, "UGRD"}, {2003, "VGRD"}, {2004, "STRM"}, {2005, "VPOT"}, {2008, "VVEL"}, {2009, "DZDT"}, {2010, "ABSV"}, {2012, "RELV"}, {2022, "GUST"},
            {3000, "PRES"}, {3001, "PRMSL"}, {3005, "HGT"}, {3018, "HPBL"},
            {4007, "DSWRF"}, {4008, "USWRF"}, {5003, "DLWRF"}, {5004, "ULWRF"},
            {6001, "TCDC"}, {6003, "LCDC"}, {6004, "MCDC"}, {6005, "HCDC"}, {6006, "CWAT"},
            {7006, "CAPE"}, {7007, "CIN"}, {7008, "HLCY"},
            {13193, "PMTF"}, {13194, "PMTC"}, {14193, "OZCON"}, {14201, "OZMAX8"}, {14202, "PDMAX1"},
            {19000, "VIS"}, {19001, "ALBDO"},
            {20000, "MASSDEN"}, {20001, "COLMD"},
        };
        const auto found = names.find(category * 1000 + number);
        if (discipline == 0 && found != names.end()) {
            return found->second;
        }
        return "var" + std::to_string(discipline) + "_" + std::to_string(category) + "_" + std::to_string(number);
    }

    std::string number(double v) {
        std::ostringstream out;
        out << v;
        return out.str();
    }

    // the level, in NOAA's words
    std::string levelOf(int type, double first, int secondType, double second) {
        const auto whole = [] (double v) { return std::to_string(static_cast<long>(std::lround(v))); };
        switch (type) {
            case 1: return "surface";
            case 2: return "cloud base";
            case 3: return "cloud top";
            case 4: return "0C isotherm";
            case 6: return "max wind";
            case 7: return "tropopause";
            case 8: return "top of atmosphere";
            case 10: case 200: return "entire atmosphere (considered as a single layer)";
            case 100:
                if (secondType == 100) {
                    return whole(first / 100.0) + "-" + whole(second / 100.0) + " mb";
                }
                return whole(first / 100.0) + " mb";
            case 101: return "mean sea level";
            case 102: return number(first) + " m above mean sea level";
            case 103:
                if (secondType == 103) {
                    return number(first) + "-" + number(second) + " m above ground";
                }
                return number(first) + " m above ground";
            case 104: return number(first) + " sigma level";
            case 105: return whole(first) + " hybrid level";
            case 106:
                if (secondType == 106) {
                    return number(first) + "-" + number(second) + " m below ground";
                }
                return number(first) + " m below ground";
            case 107: return whole(first) + " K isentropic level";
            case 108:
                if (secondType == 108) {
                    return whole(first / 100.0) + "-" + whole(second / 100.0) + " mb above ground";
                }
                return whole(first / 100.0) + " mb above ground";
            case 109: return number(first) + " PV surface";
            case 220: return "planetary boundary layer";
            default: return "level" + std::to_string(type) + (first != 0.0 ? "=" + number(first) : std::string{});
        }
    }

    // a time in the unit the file gives it ("indicator of the unit of time", code table 4.4), as hours; -1 for a unit that is not a whole number of hours
    double hoursOf(int unit, long value) {
        switch (unit) {
            case 0: return value / 60.0;
            case 1: return static_cast<double>(value);
            case 2: return value * 24.0;
            case 10: return value * 3.0;
            case 11: return value * 6.0;
            case 12: return value * 12.0;
            case 13: return value / 3600.0;
            default: return -1.0;
        }
    }

    std::string hourText(double hours) {
        return hours == std::floor(hours) ? std::to_string(static_cast<long>(hours)) : number(std::round(hours * 100.0) / 100.0);
    }

    // one message's line: `p` points at the message's bytes (at least its first sections), `size` how many are there, `offset` where the message is in the file
    struct Line {
        bool ok{false};
        std::string text;   // without the message's number
        std::string date;
    };

    // The sections of the head of a message, read from `bytesAt` (which gives `count` bytes at an offset inside the message, from what has been read or by one more read): section 1 for the
    // reference time, section 4 for the field. The message is left as soon as both are known.
    template <typename BytesAt>
    Line describe(int discipline, uint64_t length, BytesAt&& bytesAt) {
        Line line;
        uint64_t p = 16;
        int year = 0, month = 0, day = 0, hour = 0;
        bool haveReference = false;
        while (p + 5 <= length - 4) {
            const auto head = bytesAt(p, 5);
            if (head.size() < 5) {
                return line;
            }
            const auto * h = reinterpret_cast<const uchar *>(head.constData());
            const uint32_t sectionLength = be32(h);
            const int section = h[4];
            if (sectionLength < 5) {
                return line;
            }
            if (section == 1) {
                const auto s = bytesAt(p, 19);
                if (s.size() < 19) {
                    return line;
                }
                const auto * d = reinterpret_cast<const uchar *>(s.constData());
                year = be16(d + 12);
                month = d[14];
                day = d[15];
                hour = d[16];
                haveReference = true;
            } else if (section == 4) {
                const auto s = bytesAt(p, 59);
                if (s.size() < 34) {
                    return line;
                }
                const auto * d = reinterpret_cast<const uchar *>(s.constData());
                const int templateNumber = be16(d + 7);
                const int category = d[9], parameter = d[10];
                const int unit = d[17];
                const long lead = static_cast<int32_t>(be32(d + 18));
                const int firstType = d[22];
                int scale = d[23];
                if (scale > 127) {
                    scale = -(scale - 128);
                }
                const double first = static_cast<double>(be32(d + 24)) * std::pow(10.0, -scale);
                const int secondType = d[28];
                int scale2 = d[29];
                if (scale2 > 127) {
                    scale2 = -(scale2 - 128);
                }
                const double second = static_cast<double>(be32(d + 30)) * std::pow(10.0, -scale2);
                std::string forecast;
                const double start = hoursOf(unit, lead);
                if (templateNumber == 8 && s.size() >= 58) {   // an interval: the average, the accumulation, the maximum or the minimum of the hours from the forecast time on
                    const int process = d[46];
                    const double length = hoursOf(d[48], static_cast<long>(be32(d + 49)));
                    const char * word = process == 0 ? "ave" : process == 1 ? "acc" : process == 2 ? "max" : process == 3 ? "min" : "stat";
                    forecast = start < 0.0 && unit != 1 ? "?" : hourText(start) + "-" + hourText(start + std::max(0.0, length)) + " hour " + word + " fcst";
                } else if (start == 0.0) {
                    forecast = "anl";
                } else {
                    forecast = hourText(start) + " hour fcst";
                }
                char date[16];
                std::snprintf(date, sizeof date, "%04d%02d%02d%02d", year, month, day, hour);
                line.date = date;
                line.text = ":" + nameOf(discipline, category, parameter) + ":" + levelOf(firstType, first, secondType, second) + ":" + forecast + ":";
                line.ok = haveReference;
                return line;
            }
            p += sectionLength;
            if (section == 8) {
                break;
            }
        }
        return line;
    }

    const PermanentCache& store() {
        static const PermanentCache cache{"gribscan"};
        return cache;
    }

    std::string keyOf(std::string url) {
        const auto scheme = url.find("://");
        if (scheme != std::string::npos) {
            url.erase(0, scheme + 3);
        }
        for (auto& c : url) {
            if (c == '?' || c == ':' || c == '*' || c == '"' || c == '<' || c == '>' || c == '|') {
                c = '_';
            }
        }
        return url + ".idx";
    }

    std::mutex scanMutex;
    std::map<std::string, std::string> kept;   // the indexes of this session (those of files that are not old enough to keep)
}

bool GribScanIndex::handles(const std::string& url) {
    const std::string s{suffix};
    return url.size() > s.size() && url.compare(url.size() - s.size(), s.size(), s) == 0;
}

std::string GribScanIndex::scan(const QByteArray& file, long long from) {
    std::ostringstream out;
    const auto * d = reinterpret_cast<const uchar *>(file.constData());
    const uint64_t size = static_cast<uint64_t>(file.size());
    uint64_t offset = 0;
    int n = 0;
    while (offset + 16 <= size && d[offset] == 'G' && d[offset + 1] == 'R' && d[offset + 2] == 'I' && d[offset + 3] == 'B' && d[offset + 7] == 2) {
        const uint64_t length = be64(d + offset + 8);
        if (length < 20 || offset + length > size) {
            break;
        }
        const auto line = describe(d[offset + 6], length, [&] (uint64_t at, int count) {
            return file.mid(static_cast<qsizetype>(offset + at), count);
        });
        n++;
        if (line.ok) {
            out << n << ":" << (static_cast<long long>(offset) + from) << ":d=" << line.date << line.text << "\n";
        }
        offset += length;
    }
    return out.str();
}

QByteArray GribScanIndex::index(const std::string& indexUrl) {
    const std::string fileUrl = indexUrl.substr(0, indexUrl.size() - std::string{suffix}.size());
    const auto key = keyOf(fileUrl);
    std::lock_guard<std::mutex> guard{scanMutex};   // one file at a time: the charts of a run all ask for the same index at once
    if (store().has(key)) {
        return QByteArray::fromStdString(store().read(key));
    }
    if (const auto found = kept.find(key); found != kept.end()) {
        return QByteArray::fromStdString(found->second);
    }
    struct Note {
        ~Note() { Activity::setNote({}); }
    } note;
    const auto slash = fileUrl.rfind('/');
    const std::string what = slash == std::string::npos ? fileUrl : fileUrl.substr(slash + 1);
    std::ostringstream out;
    uint64_t offset = 0;
    int n = 0;
    bool complete = false;
    QDateTime modified;
    constexpr int head = 700;   // the first sections of a message are well inside this (a grid with a long list of rows is read on)
    while (n < 20000) {
        Activity::setNote("indexing " + what + " (the first time, kept after): message " + std::to_string(n + 1));
        const auto first = NetManager::get(fileUrl, QByteArray::fromStdString("bytes=" + std::to_string(offset) + "-" + std::to_string(offset + head - 1)));
        if (first.status == 416) {
            complete = true;   // past the end of the file: every message was read
            break;
        }
        if (first.status != 206 || first.bytes.size() < 16 || !first.bytes.startsWith("GRIB") || static_cast<uchar>(first.bytes[7]) != 2) {
            break;   // not a GRIB2 message (GRIB1 is not read here), a server that does not do ranges, or an answer that failed: what was read is returned, and not kept
        }
        if (first.lastModified.isValid()) {
            modified = first.lastModified;
        }
        const auto * d = reinterpret_cast<const uchar *>(first.bytes.constData());
        const uint64_t length = be64(d + 8);
        if (length < 20) {
            break;
        }
        std::map<uint64_t, QByteArray> pieces;   // what has been read of this message: from its start, then more where a section lies past the first read
        pieces[0] = first.bytes;
        const auto bytesAt = [&] (uint64_t at, int count) -> QByteArray {
            for (const auto& [start, bytes] : pieces) {
                if (at >= start && at + count <= start + static_cast<uint64_t>(bytes.size())) {
                    return bytes.mid(static_cast<qsizetype>(at - start), count);
                }
            }
            if (at + 1 > length) {
                return {};
            }
            const auto more = NetManager::get(fileUrl, QByteArray::fromStdString("bytes=" + std::to_string(offset + at) + "-" + std::to_string(offset + at + 399)));
            if (more.status != 206 || more.bytes.isEmpty()) {
                return {};
            }
            pieces[at] = more.bytes;
            return more.bytes.left(count);
        };
        const auto line = describe(d[6], length, bytesAt);
        n++;
        if (line.ok) {
            out << n << ":" << offset << ":d=" << line.date << line.text << "\n";
        }
        offset += length;
    }
    const auto text = out.str();
    if (text.empty()) {
        return {};
    }
    // a published file does not change: the index is kept for good when the file is complete and has not been touched lately (a file still being written grows: its index would be short)
    if (complete && modified.isValid() && modified.secsTo(QDateTime::currentDateTimeUtc()) > 1800) {
        store().write(key, text);
    } else if (complete) {
        kept[key] = text;
    }
    return QByteArray::fromStdString(text);
}
