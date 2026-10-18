// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "gfs/NceiInventory.h"
#include <cctype>
#include <sstream>
#include <vector>
#include "objects/URL.h"

namespace {
    std::vector<std::string> split(const std::string& line) {
        std::vector<std::string> parts;
        size_t from = 0;
        while (true) {
            const auto colon = line.find(':', from);
            if (colon == std::string::npos) {
                parts.push_back(line.substr(from));
                return parts;
            }
            parts.push_back(line.substr(from, colon - from));
            from = colon + 1;
        }
    }

    void replaceAll(std::string& text, const std::string& from, const std::string& to) {
        for (size_t at = text.find(from); at != std::string::npos; at = text.find(from, at + to.size())) {
            text.replace(at, from.size(), to);
        }
    }

    // wgrib's words for a level -> NOAA's
    std::string level(std::string l) {
        if (l == "sfc") return "surface";
        if (l == "MSL") return "mean sea level";
        if (l == "atmos col") return "entire atmosphere";
        if (l == "trop") return "tropopause";
        if (l == "max wind") return "max wind";
        replaceAll(l, "above gnd", "above ground");
        replaceAll(l, "below gnd", "below ground");
        return l;
    }

    // "3hr fcst" -> "3 hour fcst", "0-3hr acc" -> "0-3 hour acc fcst", "anl" stays
    std::string forecast(std::string f) {
        replaceAll(f, "hr", " hour");
        replaceAll(f, "  ", " ");
        if (f != "anl" && f.find("fcst") == std::string::npos && !f.empty() && std::isdigit(static_cast<unsigned char>(f[0]))) {
            f += " fcst";
        }
        return f;
    }
}

bool NceiInventory::handles(const std::string& url) {
    return url.size() > 4 && url.compare(url.size() - 4, 4, ".inv") == 0 && url.find("ncei.noaa.gov/oa/prod-model/") != std::string::npos;
}

std::string NceiInventory::convert(const std::string& text) {
    std::ostringstream out;
    std::istringstream in{text};
    std::string line;
    while (std::getline(in, line)) {
        auto parts = split(line);
        if (parts.size() < 7 || parts[2].compare(0, 2, "D=") != 0) {   // (not a wgrib line: GRIB2's own are kept as they are)
            out << line << "\n";
            continue;
        }
        auto where = level(parts[4]);
        if (where == "entire atmosphere" && parts[3] == "PWAT") {   // (NOAA's own words for the precipitable water)
            where = "entire atmosphere (considered as a single layer)";
        }
        out << parts[0] << ":" << parts[1] << ":d=" << parts[2].substr(2) << ":" << parts[3] << ":" << where << ":" << forecast(parts[6]) << ":\n";
    }
    return out.str();
}

QByteArray NceiInventory::index(const std::string& url) {
    const auto raw = URL::getBytesManaged(url);
    if (raw.isEmpty()) {
        return {};
    }
    const auto text = std::string{raw.constData(), static_cast<size_t>(raw.size())};
    if (text.find(":D=") == std::string::npos) {
        return raw;
    }
    const auto converted = convert(text);
    return QByteArray{converted.data(), static_cast<qsizetype>(converted.size())};
}
