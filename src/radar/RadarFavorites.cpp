// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "radar/RadarFavorites.h"
#include <algorithm>
#include "objects/WString.h"
#include "radar/RadarSites.h"
#include "util/Utility.h"

std::function<void()> RadarFavorites::onChanged;

namespace {
    const string pref{"RADAR_FAVORITES"};

    void save(const vector<string>& sites) {
        Utility::writePref(pref, WString::join(sites, ","));
        if (RadarFavorites::onChanged) {
            RadarFavorites::onChanged();
        }
    }
}

vector<string> RadarFavorites::list() {
    vector<string> out;
    const auto saved = Utility::readPref(pref, "");
    if (saved.empty()) {
        return out;
    }
    for (const auto& site : WString::split(saved, ",")) {
        if (!site.empty() && std::find(out.begin(), out.end(), site) == out.end()) {
            out.push_back(site);
        }
    }
    return out;
}

bool RadarFavorites::contains(const string& site) {
    const auto sites = list();
    return std::find(sites.begin(), sites.end(), site) != sites.end();
}

void RadarFavorites::add(const string& site) {
    auto sites = list();
    if (std::find(sites.begin(), sites.end(), site) == sites.end()) {
        sites.push_back(site);
        save(sites);
    }
}

void RadarFavorites::remove(const string& site) {
    auto sites = list();
    sites.erase(std::remove(sites.begin(), sites.end(), site), sites.end());
    save(sites);
}

void RadarFavorites::toggle(const string& site) {
    contains(site) ? remove(site) : add(site);
}

string RadarFavorites::label(const string& site) {
    for (const auto& entry : RadarSites::radars()) {
        if (entry.rfind(site + ":", 0) == 0) {
            return entry;
        }
    }
    return site;
}
