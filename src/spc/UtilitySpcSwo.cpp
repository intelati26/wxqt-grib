// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "UtilitySpcSwo.h"
#include <regex>
#include "common/GlobalVariables.h"
#include "util/To.h"
#include "util/UtilityIO.h"
#include "util/UtilityList.h"
#include "util/UtilityString.h"

vector<string> UtilitySpcSwo::getSwoStateUrl(string state, int dayInt) {
    auto day = To::string(dayInt);
    switch (dayInt) {
        case 1:
        case 2:
            return {GlobalVariables::nwsSPCwebsitePrefix + "/partners/outlooks/state/images/" + state + "_swody" + day + ".png",
                        GlobalVariables::nwsSPCwebsitePrefix + "/partners/outlooks/state/images/" + state + "_swody" + day + "_TORN.png",
                        GlobalVariables::nwsSPCwebsitePrefix + "/partners/outlooks/state/images/" + state + "_swody" + day + "_HAIL.png",
                        GlobalVariables::nwsSPCwebsitePrefix + "/partners/outlooks/state/images/" + state + "_swody" + day + "_WIND.png"};
        case 3:
            return {
                GlobalVariables::nwsSPCwebsitePrefix + "/partners/outlooks/state/images/" + state + "_swody" + day + ".png",
                GlobalVariables::nwsSPCwebsitePrefix + "/partners/outlooks/state/images/" + state + "_swody" + day + "_PROB.png"};
        case 4:
        case 5:
        case 6:
        case 7:
        case 8:
        case 48:
            return {
                    GlobalVariables::nwsSPCwebsitePrefix + "/partners/outlooks/state/images/" + state + "_swody" + "4" + "_PROB.png",
                    GlobalVariables::nwsSPCwebsitePrefix + "/partners/outlooks/state/images/" + state + "_swody" + "5" + "_PROB.png",
                    GlobalVariables::nwsSPCwebsitePrefix + "/partners/outlooks/state/images/" + state + "_swody" + "6" + "_PROB.png",
                    GlobalVariables::nwsSPCwebsitePrefix + "/partners/outlooks/state/images/" + state + "_swody" + "7" + "_PROB.png",
                    GlobalVariables::nwsSPCwebsitePrefix + "/partners/outlooks/state/images/" + state + "_swody" + "8" + "_PROB.png"};
        default:
            return {};
    }
}

string UtilitySpcSwo::getImageUrlsDays48(int day) {
    return GlobalVariables::nwsSPCwebsitePrefix + "/products/exper/day4-8/day" + To::string(day) + "prob.gif";
}

vector<string> UtilitySpcSwo::getImageUrls(int day) {
    vector<string> urls;
    if (day == 48) {
        for (auto dayInt : range2(4, 9)) {
            urls.push_back(GlobalVariables::nwsSPCwebsitePrefix + "/products/exper/day4-8/day" + To::string(dayInt) + "prob.gif");
        }
        return urls;
    }
    const auto html = UtilityIO::getHtml(GlobalVariables::nwsSPCwebsitePrefix + "/products/outlook/day" + To::string(day) + "otlk.html");
    const auto time = UtilityString::parse(html, "show_tab\\(.otlk_([0-9]{4}).\\)");
    if (day == 1 || day == 2) {
        auto baseUrl = GlobalVariables::nwsSPCwebsitePrefix + "/products/outlook/day" + To::string(day) + "probotlk_";
        urls.push_back(GlobalVariables::nwsSPCwebsitePrefix + "/products/outlook/day" + To::string(day) + "otlk_" + time + ".png");
        for (const auto& urlEnd : {"_torn.png", "_hail.png", "_wind.png"}) {
            urls.push_back(baseUrl + time + urlEnd);
        }
    } else {
        for (const auto& urlEnd : {"otlk_", "prob_"}) {
            urls.push_back(GlobalVariables::nwsSPCwebsitePrefix + "/products/outlook/day" + To::string(day) + urlEnd + time + ".png");
        }
    }
    return urls;
}

namespace {
    string cleanCell(string s) {
        static const std::regex tags{"<[^>]*>"};
        s = std::regex_replace(s, tags, "");
        for (const auto& [from, to] : std::vector<std::pair<string, string>>{{"&nbsp;", " "}, {"&amp;", "&"}}) {
            for (size_t at = s.find(from); at != string::npos; at = s.find(from, at + to.size())) {
                s.replace(at, from.size(), to);
            }
        }
        const auto first = s.find_first_not_of(" \t\r\n");
        const auto last = s.find_last_not_of(" \t\r\n");
        return first == string::npos ? string{} : s.substr(first, last - first + 1);
    }
}

vector<string> UtilitySpcSwo::riskTableFiles(const string& html) {
    vector<string> files;
    const std::regex pattern{"tbl[A-Za-z]* = \"\\./([A-Za-z0-9_]+_SItable\\.html)\""};
    for (auto it = std::sregex_iterator(html.begin(), html.end(), pattern); it != std::sregex_iterator(); ++it) {
        files.push_back((*it)[1].str());
    }
    return files;
}

SpcRiskTable UtilitySpcSwo::parseRiskTable(const string& html) {
    SpcRiskTable table;
    const std::regex rowPattern{"<tr[^>]*>(.*?)</tr>", std::regex::icase};
    const std::regex cellPattern{"<td[^>]*>(.*?)</td>", std::regex::icase};
    bool header = true;
    string flat;
    for (const char ch : html) {
        flat += (ch == '\n' || ch == '\r') ? ' ' : ch;
    }
    for (auto row = std::sregex_iterator(flat.begin(), flat.end(), rowPattern); row != std::sregex_iterator(); ++row) {
        const auto inner = (*row)[1].str();
        vector<string> cells;
        for (auto cell = std::sregex_iterator(inner.begin(), inner.end(), cellPattern); cell != std::sregex_iterator(); ++cell) {
            cells.push_back(cleanCell((*cell)[1].str()));
        }
        if (cells.empty()) {
            continue;
        }
        if (header) {
            table.columns = cells;
            table.title = cells[0];
            header = false;
        } else {
            table.rows.push_back(cells);
        }
    }
    return table;
}

string UtilitySpcSwo::highestRisk(const SpcRiskTable& table) {
    static const vector<string> order{"TSTM", "MARGINAL", "SLIGHT", "ENHANCED", "MODERATE", "HIGH"};
    string best;
    int bestRank = -1;
    for (const auto& row : table.rows) {
        if (row.size() < 2) {
            continue;   // "No Risk Areas Forecast"
        }
        for (size_t i = 0; i < order.size(); i++) {
            if (row[0] == order[i] && static_cast<int>(i) > bestRank) {
                bestRank = static_cast<int>(i);
                best = row[0];
            }
        }
    }
    return best;
}

string UtilitySpcSwo::riskTableHtml(const SpcRiskTable& table) {
    string html = "<table cellspacing='0' cellpadding='4' border='1'>";
    html += "<tr>";
    for (const auto& column : table.columns) {
        html += "<th>" + column + "</th>";
    }
    html += "</tr>";
    for (const auto& row : table.rows) {
        html += "<tr>";
        if (row.size() == 1) {
            html += "<td colspan='" + std::to_string(std::max<size_t>(table.columns.size(), 1)) + "'>" + row[0] + "</td>";
        } else {
            for (size_t i = 0; i < row.size(); i++) {
                html += string{i == 0 ? "<td><b>" : (i == 3 ? "<td>" : "<td align='right'>")} + row[i] + (i == 0 ? "</b></td>" : "</td>");
            }
        }
        html += "</tr>";
    }
    return html + "</table>";
}
