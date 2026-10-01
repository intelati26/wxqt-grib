// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "ui/ToolbarGroups.h"
#include <algorithm>
#include <map>
#include "objects/WString.h"
#include "util/To.h"
#include "util/Utility.h"

namespace {
    const string modePref{"TOOLBAR_MODE"};
    const string groupsPref{"TOOLBAR_GROUPS"};
    const string otherGroup{"Other"};

    int currentMode = 0;
    vector<ToolbarGroups::Group> current;
    vector<string> knownIds;

    // built-in grouping: group names in menu order, then the entries (RouteItem ids) of each
    const vector<std::pair<string, vector<string>>>& builtIn() {
        static const vector<std::pair<string, vector<string>>> all{
            {"Radar and satellite", {"baseline_flash_on_black_48dp.png", "wxogldualpane.png", "wxoglquadpane.png",
                                     "radarmosaicnws.png", "mcd_tile.png", "baseline_cloud_black_48dp.png", "goesfulldisk.png",
                                     "lightning.png"}},
            {"Forecast and observations", {"baseline_date_range_black_48dp.png", "baseline_info_black_48dp.png",
                                           "nwsobs.png", "nwsobssites.png", "spcsoundings.png", "rtma.png"}},
            {"Severe weather", {"baseline_warning_black_48dp.png", "uswarn.png", "report_today.png",
                                "report_yesterday.png", "spc_sum.png", "day1.png", "day2.png", "day3.png", "day48.png",
                                "tstorm.png#2", "ntor.png", "fire_outlook.png", "meso.png", "spccompmap.png", "tor.png"}},
            {"National, tropical and marine", {"fmap.png", "srfd.png", "wpc_rainfall.png", "nhc.png", "opc.png"}},
            {"Models", {"grib.png", "refs.png", "nsslwrf.png", "tstorm.png", "ncep.png", "spchrrr.png", "spcsref.png",
                        "hrrrviewer.png", "nsslwrf.png#2", "wpcgefs.png"}},
            {"Tools", {"baseline_settings_black_48dp.png"}},
        };
        return all;
    }

    string builtInGroupOf(const string& id) {
        for (const auto& group : builtIn()) {
            if (std::find(group.second.begin(), group.second.end(), id) != group.second.end()) {
                return group.first;
            }
        }
        return otherGroup;
    }

    ToolbarGroups::Group * find(const string& name) {
        for (auto& group : current) {
            if (group.name == name) {
                return &group;
            }
        }
        return nullptr;
    }

    string clean(string name) {
        for (const char bad : {';', '=', ','}) {
            name.erase(std::remove(name.begin(), name.end(), bad), name.end());
        }
        while (!name.empty() && name.front() == ' ') {
            name.erase(name.begin());
        }
        while (!name.empty() && name.back() == ' ') {
            name.pop_back();
        }
        return name;
    }

    void save() {
        string text;
        for (const auto& group : current) {
            text += (text.empty() ? "" : ";") + group.name + "=" + WString::join(group.ids, ",");
        }
        Utility::writePref(groupsPref, text);
    }

    // every id in exactly one group, in the order of knownIds within each group; empty "Other" dropped
    void normalise() {
        for (auto& group : current) {
            group.ids.erase(std::remove_if(group.ids.begin(), group.ids.end(), [] (const string& id) {
                return std::find(knownIds.begin(), knownIds.end(), id) == knownIds.end();
            }), group.ids.end());
        }
        for (const auto& id : knownIds) {
            const auto placed = std::any_of(current.begin(), current.end(), [&id] (const auto& g) {
                return std::find(g.ids.begin(), g.ids.end(), id) != g.ids.end();
            });
            if (placed) {
                continue;
            }
            const auto name = builtInGroupOf(id);
            auto * group = find(name);
            if (group == nullptr) {
                current.push_back({name, {}});
                group = &current.back();
            }
            group->ids.push_back(id);
        }
        current.erase(std::remove_if(current.begin(), current.end(), [] (const auto& g) {
            return g.ids.empty() && g.name == otherGroup;
        }), current.end());
        if (current.empty()) {
            current.push_back({otherGroup, knownIds});
        }
    }

    void buildBuiltIn() {
        current.clear();
        for (const auto& group : builtIn()) {
            current.push_back({group.first, {}});
        }
        normalise();
    }
}

int ToolbarGroups::mode() {
    return currentMode;
}

void ToolbarGroups::setMode(int mode) {
    currentMode = std::clamp(mode, 0, 2);
    Utility::writePrefInt(modePref, currentMode);
}

vector<string> ToolbarGroups::modeLabels() {
    return {"Icons only", "Icons and names, grouped", "Menu bar"};
}

void ToolbarGroups::load(const vector<string>& allIds) {
    knownIds = allIds;
    currentMode = std::clamp(Utility::readPrefInt(modePref, 0), 0, 2);
    current.clear();
    const auto saved = Utility::readPref(groupsPref, "");
    if (saved.empty()) {
        buildBuiltIn();
        return;
    }
    for (const auto& part : WString::split(saved, ";")) {
        const auto at = part.find('=');
        if (at == string::npos || at == 0) {
            continue;
        }
        Group group{part.substr(0, at), {}};
        const auto rest = part.substr(at + 1);
        if (!rest.empty()) {
            group.ids = WString::split(rest, ",");
        }
        current.push_back(group);
    }
    normalise();
}

const vector<ToolbarGroups::Group>& ToolbarGroups::groups() {
    return current;
}

int ToolbarGroups::groupOf(const string& id) {
    for (int i = 0; i < static_cast<int>(current.size()); i += 1) {
        if (std::find(current[i].ids.begin(), current[i].ids.end(), id) != current[i].ids.end()) {
            return i;
        }
    }
    return 0;
}

void ToolbarGroups::moveItem(const string& id, int group) {
    if (group < 0 || group >= static_cast<int>(current.size())) {
        return;
    }
    for (auto& g : current) {
        g.ids.erase(std::remove(g.ids.begin(), g.ids.end(), id), g.ids.end());
    }
    current[group].ids.push_back(id);
    // keep the toolbar's own order inside the group
    auto& ids = current[group].ids;
    std::stable_sort(ids.begin(), ids.end(), [] (const string& a, const string& b) {
        return std::find(knownIds.begin(), knownIds.end(), a) < std::find(knownIds.begin(), knownIds.end(), b);
    });
    save();
}

void ToolbarGroups::renameGroup(int group, const string& name) {
    const auto cleaned = clean(name);
    if (group < 0 || group >= static_cast<int>(current.size()) || cleaned.empty()) {
        return;
    }
    const auto taken = std::any_of(current.begin(), current.end(), [&cleaned] (const auto& g) { return g.name == cleaned; });
    if (taken && current[group].name != cleaned) {
        return;   // names are unique
    }
    current[group].name = cleaned;
    save();
}

void ToolbarGroups::addGroup(const string& name) {
    auto cleaned = clean(name);
    if (cleaned.empty()) {
        cleaned = "New group";
    }
    string unique = cleaned;
    for (int n = 2; find(unique) != nullptr; n += 1) {
        unique = cleaned + " " + To::string(n);
    }
    current.push_back({unique, {}});
    save();
}

void ToolbarGroups::deleteGroup(int group) {
    if (current.size() < 2 || group < 0 || group >= static_cast<int>(current.size())) {
        return;
    }
    const auto target = group > 0 ? group - 1 : 1;
    for (const auto& id : current[group].ids) {
        current[target].ids.push_back(id);
    }
    current.erase(current.begin() + group);
    normalise();
    save();
}

void ToolbarGroups::moveGroup(int from, int to) {
    const auto count = static_cast<int>(current.size());
    if (from < 0 || from >= count || to < 0 || to >= count || from == to) {
        return;
    }
    std::swap(current[from], current[to]);
    save();
}

void ToolbarGroups::reset() {
    buildBuiltIn();
    save();
}
