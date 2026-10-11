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
        {   // the GFS: the charts everything else is made from
            Def d;
            d.id = "GFS";
            d.label = "NOAA/NCEP GFS 0.25 degree";
            d.source = [] (const std::string&) { return GfsData::gfs(); };
            d.hours = {{0, 243, 3}, {252, 396, 12}};
            d.overlays = true;
            m.push_back(d);
        }
        {   // the National Blend of Models: hourly runs, its own charts, only the contiguous United States
            Def d;
            d.id = "NBM";
            d.label = "NOAA/NWS National Blend of Models v4, 2.5 km";
            d.source = [] (const std::string&) { return GfsData::nbm(); };
            d.sectors = Sectors::Conus;
            d.hours = {{1, 36, 1}, {39, 192, 3}, {198, 264, 6}};   // hourly for a day and a half, then every 3 hours, then every 6
            d.hourlyRuns = true;
            d.onMissing = Missing::MainRun;   // a run that is not at 00, 06, 12 or 18Z has fewer fields: the newest of the main runs has the rest
            m.push_back(d);
        }
        m.push_back(fromFile("AIGFS"));
        m.push_back(fromFile("IFS"));
        m.push_back(fromFile("AIFS"));
        {   // the ensemble mean and spread
            Def d;
            d.id = "GEFS";
            d.label = "NOAA/NCEP GEFS 30 member ensemble, 0.5 degree";
            d.source = [] (const std::string&) { return GfsData::gefs(); };
            d.hours = {{0, 384, 6}};   // 6 hourly: the precipitation pieces are 6 hours
            d.overlays = true;
            d.recipeList = true;
            d.clone.enabled = true;
            d.clone.variables = {"HGT", "TMP", "RH", "UGRD", "VGRD", "VVEL", "PRMSL", "PWAT", "CAPE", "CIN", "APCP", "CRAIN", "CSNOW", "CFRZR", "CICEP", "TCDC", "DPT", "GUST", "SNOD", "WEASD", "TMAX", "TMIN", "HLCY"};
            d.clone.levels = {"10 mb", "50 mb", "100 mb", "200 mb", "250 mb", "300 mb", "400 mb", "500 mb", "700 mb", "850 mb", "925 mb", "1000 mb"};
            d.clone.pieces = true;
            d.clone.labelPrefix = "Mean ";
            d.clone.plain = {"precip_p"};
            d.clone.skip = {"precip_p06", "precip_p24", "500_vort_ht", "850_vort_ht"};   // these are drawn as mean and spread together
            m.push_back(d);
        }
        m.push_back(fromFile("GEPS"));
        m.push_back(fromFile("RRFS"));
        m.push_back(fromFile("HREF"));
        {   // the Rapid Refresh Forecast System ensemble: five members at 3 km, and its ready-made probability products (it replaces HREF and SREF)
            Def d;
            d.id = "REFS";
            d.label = "NOAA/NCEP REFS, 5 member 3 km ensemble";
            d.source = [] (const std::string&) { return GfsData::refs(); };
            d.sectors = Sectors::Conus;
            d.hours = {{0, 60, 1}};
            d.recipeList = true;
            m.push_back(d);
        }
        m.push_back(fromFile("SPCPOST"));
        m.push_back(fromFile("SPCGEFS"));
        m.push_back(fromFile("SFCOA"));
        m.push_back(fromFile("RRFSD"));
        {   // SPC's REFS ensemble products from its open Zarr store: means, maxima, probabilities and paintballs of the 10 members, on the 3 km grid
            Def d;
            d.id = "SPCREFS";
            d.label = "SPC REFS ensemble products (open Zarr data)";
            d.source = [] (const std::string&) { return GfsData::spcRefs(); };
            d.sectors = Sectors::Conus;
            d.hours = {{1, 60, 1}};
            d.cycleStep = 6;
            d.recipeList = true;
            m.push_back(d);
        }
        m.push_back(fromFile("GFS-WAVE"));
        m.push_back(fromFile("GEFS-WAVE"));
        for (const char * version : {"HAFSA", "HAFSB"}) {   // the hurricane model, for one storm at a time
            Def d;
            d.id = version;
            d.label = std::string{"NOAA/NCEP HAFS-"} + (std::string{version} == "HAFSB" ? "B" : "A") + ", 2 km storm-following grid";
            const std::string id = version;
            d.source = [id] (const std::string& storm) { return GfsData::hafs(id, storm); };
            d.sectors = Sectors::Storm;
            d.hours = {{0, 126, 3}};
            d.storm = true;
            d.onMissing = Missing::PreviousCycle;   // the waves come out after the rest
            m.push_back(d);
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
