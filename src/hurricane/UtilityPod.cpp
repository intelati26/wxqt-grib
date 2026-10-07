// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "hurricane/UtilityPod.h"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <regex>
#include <sstream>

namespace {
    string trim(const string& s) {
        const auto a = s.find_first_not_of(" \t\r");
        if (a == string::npos) {
            return "";
        }
        return s.substr(a, s.find_last_not_of(" \t\r") - a + 1);
    }

    bool startsWith(const string& s, const string& prefix) {
        return s.compare(0, prefix.size(), prefix) == 0;
    }

    // the text of one lettered line in a column: the line "A. 07/1200Z" -> "07/1200Z"
    void assign(UtilityPod::Flight& f, char letter, const string& value) {
        switch (letter) {
            case 'A': f.fixTimes = value; break;
            case 'B': f.mission = value; break;
            case 'C': f.departure = value; break;
            case 'D':
                f.position = value;
                f.hasPosition = UtilityPod::parsePosition(value, f.lat, f.lon);
                break;
            case 'E': f.onStation = value; break;
            case 'F': f.altitude = value; break;
            case 'G': f.type = value; break;
            case 'H': f.wra = value; break;
            case 'I': f.remarks = value; break;
            default: break;
        }
    }
}

bool UtilityPod::parsePosition(const string& text, double& lat, double& lon) {
    static const std::regex position{R"((\d+(?:\.\d+)?)([NS])\s+(\d+(?:\.\d+)?)([EW]))"};
    std::smatch m;
    if (!std::regex_search(text, m, position)) {
        return false;
    }
    lat = std::strtod(m[1].str().c_str(), nullptr) * (m[2] == "S" ? -1.0 : 1.0);
    lon = std::strtod(m[3].str().c_str(), nullptr) * (m[4] == "W" ? -1.0 : 1.0);
    return true;
}

UtilityPod::Pod UtilityPod::parse(const string& text) {
    Pod pod;
    std::istringstream stream{text};
    vector<string> lines;
    string line;
    while (std::getline(stream, line)) {
        lines.push_back(line);
    }
    enum class Where { Header, Atlantic, Pacific, Done } where = Where::Header;
    Requirement * requirement = nullptr;
    vector<Flight *> columns;           // the flights of the current block, by column
    size_t splitAt = string::npos;      // where the second column starts in this block
    bool inNotes = false;
    static const std::regex numbered{R"(^\s{0,6}(\d+)\.\s+(.*)$)"};
    static const std::regex flightHeader{R"(FLIGHT\s+(\w+)\s*-\s*(.*?)\s{2,}|FLIGHT\s+(\w+)\s*-\s*(.*)$)"};
    for (const auto& raw : lines) {
        const auto l = trim(raw);
        if (l.empty()) {
            continue;
        }
        if (startsWith(l, "NOUS42")) {
            pod.issued = l;
        } else if (l.find("TCPOD NUMBER") != string::npos) {
            const auto dots = l.rfind('.');
            pod.number = dots == string::npos ? "" : trim(l.substr(dots + 1));
            pod.ok = true;
        } else if (startsWith(l, "VALID ")) {
            pod.valid = trim(l.substr(6));
        } else if (startsWith(l, "I.") && l.find("ATLANTIC") != string::npos) {
            where = Where::Atlantic;
            requirement = nullptr;
            inNotes = false;
            continue;
        } else if (startsWith(l, "II.") && l.find("PACIFIC") != string::npos) {
            where = Where::Pacific;
            requirement = nullptr;
            inNotes = false;
            continue;
        } else if (startsWith(l, "$$") || startsWith(l, "NNNN")) {
            where = Where::Done;
        }
        if (where != Where::Atlantic && where != Where::Pacific) {
            continue;
        }
        auto& list = where == Where::Atlantic ? pod.atlantic : pod.pacific;
        std::smatch m;
        // a numbered item: "1. SUSPECT AREA AL92 ..." (a requirement), "2. SUCCEEDING DAY OUTLOOK:", "3. REMARK: ..." (notes)
        if (std::regex_match(raw, m, numbered) && raw.find_first_not_of(' ') < 7) {
            const auto body = trim(m[2]);
            columns.clear();
            if (startsWith(body, "NEGATIVE RECONNAISSANCE")) {
                (where == Where::Atlantic ? pod.noAtlantic : pod.noPacific) = true;
                requirement = nullptr;
                inNotes = false;
            } else if (startsWith(body, "SUCCEEDING DAY OUTLOOK") || startsWith(body, "OUTLOOK FOR SUCCEEDING DAY") || startsWith(body, "REMARK")) {
                requirement = nullptr;
                inNotes = true;
                pod.notes.push_back((where == Where::Atlantic ? "Atlantic: " : "Pacific: ") + body);
            } else {
                list.push_back(Requirement{body, {}});
                requirement = &list.back();
                inNotes = false;
            }
            continue;
        }
        if (inNotes) {
            if (!pod.notes.empty()) {
                pod.notes.back() += " " + l;
            }
            continue;
        }
        if (requirement == nullptr) {
            continue;
        }
        if (l.find("FLIGHT ") != string::npos && l.find(" - ") != string::npos && startsWith(l, "FLIGHT")) {
            // "FLIGHT ONE - TEAL 71          FLIGHT TWO - NOAA 43"
            const auto second = raw.find("FLIGHT ", raw.find("FLIGHT ") + 6);
            const auto firstAt = raw.find("FLIGHT ");
            splitAt = second;
            columns.clear();
            const auto make = [&] (const string& part) {
                std::smatch f;
                static const std::regex one{R"(FLIGHT\s+(\w+)\s*-\s*(.*))"};
                const auto t = trim(part);
                Flight flight;
                if (std::regex_match(t, f, one)) {
                    flight.ordinal = f[1];
                    flight.aircraft = trim(f[2]);
                }
                requirement->flights.push_back(flight);
            };
            const size_t before = requirement->flights.size();
            make(second == string::npos ? raw.substr(firstAt) : raw.substr(firstAt, second - firstAt));
            if (second != string::npos) {
                make(raw.substr(second));
            }
            // pointers into the vector stay valid only until the next push_back: take them after both are added
            columns.push_back(&requirement->flights[before]);
            if (second != string::npos) {
                columns.push_back(&requirement->flights[before + 1]);
            }
            continue;
        }
        // a lettered line: "A. 07/1200Z                   A. 07/1800Z"
        if (l.size() >= 2 && l[1] == '.' && std::isupper(static_cast<unsigned char>(l[0])) && !columns.empty()) {
            const char letter = l[0];
            const auto first = raw.find(string{letter} + ".");
            string left = raw.substr(first + 2);
            string right;
            if (columns.size() > 1 && splitAt != string::npos && raw.size() > splitAt) {
                right = raw.substr(splitAt);
                left = raw.substr(first + 2, splitAt > first + 2 ? splitAt - first - 2 : 0);
                const auto at = right.find(string{letter} + ".");
                right = at == string::npos ? "" : right.substr(at + 2);
            }
            assign(*columns[0], letter, trim(left));
            if (columns.size() > 1) {
                assign(*columns[1], letter, trim(right));
            }
            continue;
        }
        // a continuation of the last value (long remarks): add it to the last lettered field of the first column
    }
    pod.ok = pod.ok && !pod.valid.empty();
    return pod;
}
