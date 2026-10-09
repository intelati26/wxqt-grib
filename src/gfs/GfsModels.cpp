// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "gfs/GfsModels.h"

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
        {   // the AI model: the same charts as the GFS, from its two files; humidity from specific humidity and precipitation from 6 hour pieces
            Def d;
            d.id = "AIGFS";
            d.label = "NOAA/NCEP AIGFS 0.25 degree (an AI model; experimental)";
            d.source = [] (const std::string&) { return GfsData::aigfs(); };
            d.hours = {{0, 384, 6}};
            d.overlays = true;
            d.recipeList = true;
            d.clone.enabled = true;
            d.clone.ids = {"precip_p06", "precip_p12", "precip_p24", "precip_p36", "precip_p48", "precip_p60", "precip_ptot", "1000_500_thick", "1000_850_thick", "850_700_thick", "850_temp_mslp_precip",
                           "10m_wnd_precip", "10m_wnd_2m_temp", "200_wnd_ht", "250_wnd_ht", "300_wnd_ht", "500_rh_ht", "500_wnd_ht", "500_vort_ht", "700_rh_ht", "850_rh_ht", "850_temp_ht", "850_vort_ht",
                           "850vor_500ht_200wd", "925_temp_ht"};
            d.clone.specificHumidity = true;
            m.push_back(d);
        }
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
            m.push_back(d);
        }
        {   // the 3 km Rapid Refresh Forecast System: precipitation has running totals, and sea level pressure is MSLET
            Def d;
            d.id = "RRFS";
            d.label = "NOAA/NCEP RRFS 3 km";
            d.source = [] (const std::string&) { return GfsData::rrfs(); };
            d.sectors = Sectors::Conus;
            d.hours = {{0, 84, 1}};   // only the runs at 00, 06, 12 and 18Z go past 18 hours
            d.hourlyRuns = true;
            d.overlays = true;
            d.recipeList = true;
            d.clone.enabled = true;
            d.clone.variables = {"HGT", "TMP", "RH", "UGRD", "VGRD", "ABSV", "DPT", "PRMSL", "PWAT", "CAPE", "CIN", "APCP", "CRAIN", "CSNOW", "CFRZR", "CICEP", "TCDC", "GUST", "SNOD", "WEASD", "REFC", "HLCY", "VIS"};
            d.clone.levels = {"200 mb", "250 mb", "300 mb", "400 mb", "500 mb", "700 mb", "850 mb", "925 mb", "1000 mb"};
            d.clone.skip = {"anom"};
            d.clone.pieces = false;
            d.clone.rename = {{"PRMSL", "MSLET"}};
            m.push_back(d);
        }
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
