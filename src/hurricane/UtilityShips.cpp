// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "hurricane/UtilityShips.h"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <regex>
#include <set>
#include <sstream>

namespace {
    vector<string> words(const string& line) {
        vector<string> out;
        std::istringstream stream{line};
        string word;
        while (stream >> word) {
            out.push_back(word);
        }
        return out;
    }

    bool isNumber(const string& s) {
        if (s.empty()) {
            return false;
        }
        char * end = nullptr;
        std::strtod(s.c_str(), &end);
        return end != nullptr && *end == '\0';
    }

    string trim(const string& s) {
        const auto a = s.find_first_not_of(" \t\r");
        if (a == string::npos) {
            return "";
        }
        return s.substr(a, s.find_last_not_of(" \t\r") - a + 1);
    }

    double percentValue(string s) {
        s.erase(std::remove(s.begin(), s.end(), '%'), s.end());
        return isNumber(s) ? std::strtod(s.c_str(), nullptr) : UtilityShips::missing;
    }
}

UtilityShips::Ships UtilityShips::parse(const string& text) {
    Ships ships;
    std::istringstream stream{text};
    vector<string> lines;
    string line;
    while (std::getline(stream, line)) {
        lines.push_back(line);
    }
    static const std::set<string> noValue{"N/A", "LOST", "DIS", "NA"};
    size_t i = 0;
    for (; i < lines.size(); i++) {
        const auto& l = lines[i];
        // " *  ISAIAS      AL092026  10/07/26  06 UTC        *"
        static const std::regex header{R"(\*\s+(\S+)\s+([A-Z]{2}\d{6})\s+(\d\d)/(\d\d)/(\d\d)\s+(\d\d) UTC)"};
        std::smatch m;
        if (std::regex_search(l, m, header)) {
            ships.name = m[1];
            ships.id = m[2];
            ships.cycle = "20" + m[5].str() + m[3].str() + m[4].str() + m[6].str();
        }
        if (l.rfind("TIME (HR)", 0) == 0) {
            for (const auto& w : words(l.substr(9))) {
                if (isNumber(w)) {
                    ships.hours.push_back(std::atoi(w.c_str()));
                }
            }
            i++;
            break;
        }
    }
    if (ships.hours.empty()) {
        return ships;
    }
    const auto count = ships.hours.size();
    // the rows between the time line and the "FORECAST TRACK" line: a label, then one value per hour
    for (; i < lines.size(); i++) {
        const auto& l = lines[i];
        if (l.find("FORECAST TRACK FROM") != string::npos) {
            break;
        }
        const auto w = words(l);
        if (w.size() <= count) {
            continue;
        }
        const vector<string> values(w.end() - static_cast<long>(count), w.end());
        string label;
        for (size_t k = 0; k + count < w.size(); k++) {
            label += (k == 0 ? "" : " ") + w[k];
        }
        if (label == "Storm Type") {
            ships.stormType = values;
            continue;
        }
        vector<double> numbers;
        bool good = true;
        for (const auto& v : values) {
            if (noValue.contains(v)) {
                numbers.push_back(missing);
            } else if (isNumber(v)) {
                numbers.push_back(std::strtod(v.c_str(), nullptr));
            } else {
                good = false;
                break;
            }
        }
        if (good) {
            ships.rows[label] = numbers;
        }
    }
    for (; i < lines.size(); i++) {
        const auto& l = lines[i];
        static const std::regex prelim{R"(PRELIM RI PROB[^:]*:\s*([0-9.]+))"};
        static const std::regex prob{R"(SHIPS Prob RI for\s*(\d+)kt/\s*(\d+)hr RI threshold=\s*([0-9.]+)%\s+is\s+([0-9.]+) times climatological mean \(\s*([0-9.]+)%\))"};
        std::smatch m;
        if (std::regex_search(l, m, prelim)) {
            ships.preliminaryRi = std::strtod(m[1].str().c_str(), nullptr);
        } else if (std::regex_search(l, m, prob)) {
            RiProbability p;
            p.knots = std::atoi(m[1].str().c_str());
            p.hours = std::atoi(m[2].str().c_str());
            p.percent = std::strtod(m[3].str().c_str(), nullptr);
            p.times = std::strtod(m[4].str().c_str(), nullptr);
            p.climatology = std::strtod(m[5].str().c_str(), nullptr);
            ships.riLines.push_back(p);
        } else if (l.find("RI (kt / h)") != string::npos) {
            string cleaned = l;
            std::replace(cleaned.begin(), cleaned.end(), '|', ' ');
            for (const auto& w : words(cleaned)) {
                if (w.find('/') != string::npos && std::isdigit(static_cast<unsigned char>(w[0]))) {
                    ships.riThresholds.push_back(w);
                }
            }
        } else if (!ships.riThresholds.empty()) {
            const auto colon = l.find(':');
            if (colon != string::npos && colon < 16) {
                const auto name = trim(l.substr(0, colon));
                static const std::set<string> methods{"SHIPS-RII", "Logistic", "Bayesian", "Consensus", "DTOPS", "SDCON"};
                if (methods.contains(name)) {
                    vector<double> values;
                    for (const auto& w : words(l.substr(colon + 1))) {
                        values.push_back(percentValue(w));
                    }
                    if (values.size() == ships.riThresholds.size()) {
                        ships.riMatrix.emplace_back(name, values);
                    }
                }
            }
        }
    }
    ships.ok = !ships.rows.empty();
    return ships;
}

string UtilityShips::newestFile(const string& listing, const string& stormId) {
    if (stormId.size() < 8) {
        return {};
    }
    string basin = stormId.substr(0, 2);
    std::transform(basin.begin(), basin.end(), basin.begin(), [] (unsigned char c) { return static_cast<char>(std::toupper(c)); });
    const string tail = basin + stormId.substr(2, 2) + stormId.substr(6, 2) + "_ships.txt";   // AL0926_ships.txt
    string newest;
    size_t at = 0;
    while ((at = listing.find(tail, at)) != string::npos) {
        if (at >= 8) {
            const auto name = listing.substr(at - 8, 8 + tail.size());
            if (std::all_of(name.begin(), name.begin() + 8, [] (unsigned char c) { return std::isdigit(c); }) && name > newest) {
                newest = name;
            }
        }
        at += tail.size();
    }
    return newest;
}
