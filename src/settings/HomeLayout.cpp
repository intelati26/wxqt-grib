// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "settings/HomeLayout.h"
#include <algorithm>
#include "objects/WString.h"
#include "util/To.h"
#include "util/Utility.h"

namespace {
    const string templatePref{"HOME_LAYOUT_TEMPLATE"};
    const string assignmentPref{"HOME_LAYOUT_ASSIGNMENT"};
    constexpr int defaultTemplate = 5;   // thumbnails on top, forecast and text side by side below

    int current = defaultTemplate;
    // section tokens in stacking order, each with its zone
    vector<std::pair<string, int>> assignment;

    vector<std::pair<string, int>> defaults() {
        return {{HomeLayout::sectionSevere, 0}, {HomeLayout::sectionImages, 0},
                {HomeLayout::sectionForecast, 1}, {HomeLayout::sectionText, 2}};
    }

    int zoneCount() {
        return static_cast<int>(HomeLayout::templates()[current].zones.size());
    }

    void clampZones() {
        for (auto& entry : assignment) {
            entry.second = std::clamp(entry.second, 0, zoneCount() - 1);
        }
    }

    void save() {
        string text;
        for (const auto& entry : assignment) {
            text += (text.empty() ? "" : ",") + entry.first + ":" + To::string(entry.second);
        }
        Utility::writePrefInt(templatePref, current);
        Utility::writePref(assignmentPref, text);
    }
}

const vector<HomeLayout::Template>& HomeLayout::templates() {
    static const vector<Template> all{
        {"One zone", {1}, 1, {{0, 0, 1, 1}}},
        {"Two equal columns", {1, 1}, 1, {{0, 0, 1, 1}, {0, 1, 1, 1}}},
        {"Wide and narrow", {2, 1}, 1, {{0, 0, 1, 1}, {0, 1, 1, 1}}},
        {"Narrow and wide", {1, 2}, 1, {{0, 0, 1, 1}, {0, 1, 1, 1}}},
        {"Left, right split in two", {1, 1}, 2, {{0, 0, 2, 1}, {0, 1, 1, 1}, {1, 1, 1, 1}}},
        {"Top, two below", {1, 1}, 2, {{0, 0, 1, 2}, {1, 0, 1, 1}, {1, 1, 1, 1}}},
        {"Four zones", {1, 1}, 2, {{0, 0, 1, 1}, {0, 1, 1, 1}, {1, 0, 1, 1}, {1, 1, 1, 1}}},
        {"Three equal columns", {1, 1, 1}, 1, {{0, 0, 1, 1}, {0, 1, 1, 1}, {0, 2, 1, 1}}},
        {"Narrow, wide, narrow", {1, 2, 1}, 1, {{0, 0, 1, 1}, {0, 1, 1, 1}, {0, 2, 1, 1}}},
    };
    return all;
}

const vector<string>& HomeLayout::sections() {
    static const vector<string> all{sectionSevere, sectionImages, sectionForecast, sectionText};
    return all;
}

string HomeLayout::sectionLabel(const string& token) {
    if (token == sectionSevere) {
        return "Severe dashboard";
    }
    if (token == sectionImages) {
        return "Thumbnails";
    }
    if (token == sectionForecast) {
        return "Forecast";
    }
    if (token == sectionText) {
        return "Text";
    }
    return token;
}

void HomeLayout::load() {
    current = std::clamp(Utility::readPrefInt(templatePref, defaultTemplate), 0, static_cast<int>(templates().size()) - 1);
    assignment.clear();
    const auto saved = Utility::readPref(assignmentPref, "");
    if (!saved.empty()) {
        for (const auto& item : WString::split(saved, ",")) {
            const auto parts = WString::split(item, ":");
            const auto known = parts.size() == 2 && std::find(sections().begin(), sections().end(), parts[0]) != sections().end();
            const auto seen = known && std::any_of(assignment.begin(), assignment.end(), [&parts] (const auto& e) { return e.first == parts[0]; });
            if (known && !seen) {
                assignment.emplace_back(parts[0], To::Int(parts[1]));
            }
        }
    }
    // a section missing from the saved text (e.g. added later) goes where the built-in layout puts it
    for (const auto& entry : defaults()) {
        const auto present = std::any_of(assignment.begin(), assignment.end(), [&entry] (const auto& e) { return e.first == entry.first; });
        if (!present) {
            assignment.push_back(entry);
        }
    }
    clampZones();
}

int HomeLayout::templateIndex() {
    return current;
}

void HomeLayout::setTemplate(int index) {
    current = std::clamp(index, 0, static_cast<int>(templates().size()) - 1);
    clampZones();
    save();
}

int HomeLayout::zoneOf(const string& section) {
    for (const auto& entry : assignment) {
        if (entry.first == section) {
            return entry.second;
        }
    }
    return 0;
}

vector<string> HomeLayout::sectionsIn(int zone) {
    vector<string> out;
    for (const auto& entry : assignment) {
        if (entry.second == zone) {
            out.push_back(entry.first);
        }
    }
    return out;
}

void HomeLayout::move(const string& section, int zone, int position) {
    zone = std::clamp(zone, 0, zoneCount() - 1);
    const auto found = std::find_if(assignment.begin(), assignment.end(), [&section] (const auto& e) { return e.first == section; });
    if (found == assignment.end()) {
        return;
    }
    assignment.erase(found);
    // find the slot in the flat list that is the `position`-th entry of that zone (or after its last)
    auto insertAt = assignment.end();
    int seen = 0;
    int lastOfZone = -1;
    for (int i = 0; i < static_cast<int>(assignment.size()); i += 1) {
        if (assignment[i].second == zone) {
            if (seen == position) {
                insertAt = assignment.begin() + i;
                break;
            }
            seen += 1;
            lastOfZone = i;
        }
    }
    if (insertAt == assignment.end() && lastOfZone >= 0) {
        insertAt = assignment.begin() + lastOfZone + 1;
    }
    assignment.insert(insertAt, {section, zone});
    save();
}

string HomeLayout::signature() {
    string text = To::string(current) + "|";
    for (const auto& entry : assignment) {
        text += entry.first + ":" + To::string(entry.second) + ",";
    }
    return text;
}
