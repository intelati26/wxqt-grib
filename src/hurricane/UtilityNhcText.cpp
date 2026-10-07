// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "hurricane/UtilityNhcText.h"
#include <algorithm>
#include <sstream>

namespace {
    string trim(const string& s) {
        const auto a = s.find_first_not_of(" \t\r\n");
        if (a == string::npos) {
            return "";
        }
        return s.substr(a, s.find_last_not_of(" \t\r\n") - a + 1);
    }

    string decode(string s) {
        static const std::pair<const char *, const char *> entities[] = {{"&lt;", "<"}, {"&gt;", ">"}, {"&quot;", "\""}, {"&#39;", "'"}, {"&nbsp;", " "}, {"&amp;", "&"}};
        for (const auto& [from, to] : entities) {
            size_t at = 0;
            const string f = from;
            while ((at = s.find(f, at)) != string::npos) {
                s.replace(at, f.size(), to);
                at += string{to}.size();
            }
        }
        return s;
    }
}

string UtilityNhcText::bulletin(const string& html) {
    const auto open = html.find("<pre");
    if (open == string::npos) {
        return {};
    }
    const auto start = html.find('>', open);
    const auto end = html.find("</pre>", start);
    if (start == string::npos || end == string::npos) {
        return {};
    }
    string text = html.substr(start + 1, end - start - 1);
    // any tags inside (links, bold) are dropped
    string plain;
    bool inTag = false;
    for (const char c : text) {
        if (c == '<') {
            inTag = true;
        } else if (c == '>' && inTag) {
            inTag = false;
        } else if (!inTag) {
            plain.push_back(c);
        }
    }
    return trim(decode(plain));
}

vector<std::pair<string, string>> UtilityNhcText::sections(const string& text) {
    vector<std::pair<string, string>> out;
    std::istringstream stream{text};
    vector<string> lines;
    string line;
    while (std::getline(stream, line)) {
        lines.push_back(line);
    }
    string heading = "Header";
    string body;
    for (size_t i = 0; i < lines.size(); i++) {
        const auto t = trim(lines[i]);
        const bool dashes = i + 1 < lines.size() && !trim(lines[i + 1]).empty() && trim(lines[i + 1]).find_first_not_of('-') == string::npos && trim(lines[i + 1]).size() >= 5;
        if (!t.empty() && dashes) {
            if (!trim(body).empty() || heading != "Header") {
                out.emplace_back(heading, trim(body));
            }
            heading = t;
            body.clear();
            i++;   // the dashes
            continue;
        }
        body += lines[i] + "\n";
    }
    if (!trim(body).empty()) {
        out.emplace_back(heading, trim(body));
    }
    return out;
}

string UtilityNhcText::headline(const string& text) {
    // "...DEPRESSION BECOMES TROPICAL STORM ISAIAS...": lines of the bulletin that start and end with three dots, before the first section heading
    std::istringstream stream{text};
    string line;
    string out;
    while (std::getline(stream, line)) {
        const auto t = trim(line);
        if (t.size() > 6 && t.compare(0, 3, "...") == 0 && t.compare(t.size() - 3, 3, "...") == 0) {
            out += (out.empty() ? "" : "; ") + t.substr(3, t.size() - 6);
        } else if (t.find("SUMMARY OF") != string::npos) {
            break;
        }
    }
    return out;
}
