// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "gfs/GfsModels.h"
#include <algorithm>
#include <cstdio>
#include "gfs/GfsModelFile.h"

namespace {
    std::string userFolder;

    // a built-in model written as a definition file; a mistake in one is a mistake in the program, which the registry test shows
    GfsModels::Def fromFile(const std::string& id) {
        for (const auto& [name, text] : GfsModelFile::builtIn()) {
            if (name == id) {
                auto parsed = GfsModelFile::parse(text, "built in");
                if (!parsed.ok) {
                    std::fprintf(stderr, "model definition %s: %s\n", id.c_str(), parsed.error.c_str());
                    GfsModels::Def broken;
                    broken.id = id;
                    broken.label = "(definition error: " + parsed.error + ")";
                    return broken;
                }
                return parsed.def;
            }
        }
        GfsModels::Def missing;
        missing.id = id;
        missing.label = "(no definition)";
        return missing;
    }
}

std::string GfsModels::pickerGroup(const std::string& id) {
    if (const auto * def = find(id); def != nullptr && !def->group.empty()) {
        return def->group;
    }
    static const std::map<std::string, std::string> groups{
        {"GFS", "Global"}, {"AIGFS", "Global"}, {"IFS", "Global"}, {"AIFS", "Global"}, {"GEOS-FP", "Global"}, {"CFS", "Seasonal"}, {"CFS-ANL", "Seasonal"},
        {"NAM", "Regional"}, {"RAP", "Regional"}, {"NBM", "Regional"}, {"FIREWX", "Regional"},
        {"HRRR", "Convection allowing"}, {"RRFS", "Convection allowing"}, {"NAM-HIRES", "Convection allowing"}, {"HRW-ARW", "Convection allowing"},
        {"HRW-ARW2", "Convection allowing"}, {"HRW-FV3", "Convection allowing"},
        {"GEFS", "Ensembles"}, {"GEPS", "Ensembles"}, {"REFS", "Ensembles"}, {"HREF", "Ensembles"}, {"NAEFS", "Ensembles"},
        {"GEFS-MEAN-SPRD", "Ensembles"}, {"GEFS-SPAG", "Ensembles"},
        {"SPCPOST", "SPC guidance and mesoanalysis"}, {"SPCGEFS", "SPC guidance and mesoanalysis"}, {"SPCREFS", "SPC guidance and mesoanalysis"},
        {"SFCOA", "SPC guidance and mesoanalysis"}, {"RRFSD", "SPC guidance and mesoanalysis"},
        {"GFS-WAVE", "Waves and ocean"}, {"GEFS-WAVE", "Waves and ocean"}, {"WW3", "Waves and ocean"}, {"WW3-ENP", "Waves and ocean"}, {"WW3-WNA", "Waves and ocean"},
        {"ESTOFS", "Waves and ocean"}, {"POLAR", "Waves and ocean"}, {"HAFSA", "Tropical cyclones"}, {"HAFSB", "Tropical cyclones"}};
    const auto found = groups.find(id);
    return found == groups.end() ? "Other" : found->second;
}

void GfsModels::setUserFolder(const std::string& folder) {
    userFolder = folder;
}

std::vector<std::string> GfsModels::userProblems() {
    std::vector<std::string> out;
    for (const auto& parsed : GfsModelFile::loadFolder(userFolder)) {
        if (!parsed.ok) {
            out.push_back(parsed.origin + ": " + parsed.error);
        }
    }
    return out;
}

const std::vector<GfsModels::Def>& GfsModels::all() {
    static const std::vector<Def> models = [] {
        std::vector<Def> m;
        m.push_back(fromFile("GFS"));
        m.push_back(fromFile("NBM"));
        m.push_back(fromFile("AIGFS"));
        m.push_back(fromFile("IFS"));
        m.push_back(fromFile("AIFS"));
        m.push_back(fromFile("GEFS"));
        m.push_back(fromFile("GEPS"));
        m.push_back(fromFile("GEOS-FP"));
        m.push_back(fromFile("CFS"));
        m.push_back(fromFile("CFS-ANL"));
        m.push_back(fromFile("RRFS"));
        m.push_back(fromFile("HRRR"));
        m.push_back(fromFile("HREF"));
        m.push_back(fromFile("REFS"));
        m.push_back(fromFile("SPCPOST"));
        m.push_back(fromFile("SPCGEFS"));
        m.push_back(fromFile("SFCOA"));
        m.push_back(fromFile("RRFSD"));
        m.push_back(fromFile("SPCREFS"));
        m.push_back(fromFile("GFS-WAVE"));
        m.push_back(fromFile("GEFS-WAVE"));
        m.push_back(fromFile("HAFSA"));
        m.push_back(fromFile("HAFSB"));
        for (auto& d : m) {   // what each is and why it matters
            for (const auto& [name, text] : GfsModelFile::aboutBuiltIn()) {
                if (name == d.id && d.why.empty()) {
                    GfsModelFile::applyAbout(d, text);
                }
            }
        }
        // the models of the user's folder, after the built-in ones (a file that does not parse, or repeats an id, is left out: userProblems says why)
        for (auto& parsed : GfsModelFile::loadFolder(userFolder)) {
            if (parsed.ok && std::none_of(m.begin(), m.end(), [&parsed] (const Def& d) { return d.id == parsed.def.id; })) {
                parsed.def.clone.enabled = parsed.def.clone.enabled;
                m.push_back(parsed.def);
            }
        }
        return m;
    }();
    return models;
}

const GfsModels::Def * GfsModels::find(const std::string& id) {
    for (const auto& d : all()) {
        if (d.id == id) {
            return &d;
        }
    }
    return nullptr;
}

bool GfsModels::draws(const std::string& id) {
    return find(id) != nullptr;
}

std::vector<std::string> GfsModels::overlayModels() {
    std::vector<std::string> out;
    for (const auto& d : all()) {
        if (d.overlays) {
            out.push_back(d.id);
        }
    }
    return out;
}
