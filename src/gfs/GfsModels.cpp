// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "gfs/GfsModels.h"
#include <QDate>
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

namespace {
    // the file that holds a want in NCEI's monthly CFS analyses (a file for each variable and level) by its stem: "z500", "t850", "wnd850", "tmp2m"
    std::string cfsStem(const GfsData::Want& w) {
        const auto& v = w.variable;
        const auto& l = w.level;
        if (v == "TMP" && l == "2 m above ground") return "tmp2m";
        if ((v == "UGRD" || v == "VGRD") && l == "10 m above ground") return "wnd10m";
        if (v == "PRMSL") return "prmsl";
        if (v == "PWAT") return "pwat";
        if (l.size() > 3 && l.compare(l.size() - 3, 3, " mb") == 0) {
            const auto mb = l.substr(0, l.size() - 3);
            if (v == "HGT") return "z" + mb;
            if (v == "TMP") return "t" + mb;
            if (v == "UGRD" || v == "VGRD") return "wnd" + mb;
            if (v == "SPFH") return "q" + mb;
            if (v == "VVEL") return "vvel" + mb;
        }
        return "pgrblanl";
    }

    // The CFS analysis through the years: the open data bucket since 22 April 2023 (files of every field of an analysis, with an index), NCEI's monthly files before (the operational analysis
    // from April 2011, the CFSR before that) which have a file for each variable and no index of their own (CfsArchiveIndex makes one). The charts are the ones whose fields the monthly files
    // have at the levels they have (1000, 850, 700, 500 and 200 mb heights; 850, 700, 500 and 250 mb temperatures and winds; humidity at 850, 700 and 500).
    GfsModels::Def cfsAnalysis() {
        auto def = fromFile("CFS-ANL");
        const auto base = def.source;
        def.source = [base] (const std::string& storm) {
            auto s = base(storm);
            s.matchRunTime = true;
            s.fileOf = [] (const GfsData::Want& w) { return cfsStem(w); };
            s.fileUrl = [] (const GfsData::Run& run, int, const std::string& stem) {
                if (run.date >= "20230422") {
                    const std::string name = stem == "tmp2m" || stem == "wnd10m" ? "sfluxgrbf00" : "pgrblanl";
                    return "https://noaa-cfs-pds.s3.amazonaws.com/cdas." + run.date + "/cdas1.t" + run.cycle + "z." + name + ".grib2";
                }
                const auto month = run.date.substr(0, 6);
                const std::string ncei = "https://www.ncei.noaa.gov/thredds/fileServer/";
                const std::string name = stem + ".l.gdas." + month + (run.date >= "20110401" ? ".grib2" : ".grb2");
                return run.date >= "20110401" ? ncei + "model-cfs-allfile-operational-analysis/time-series/" + run.date.substr(0, 4) + "/" + month + "/" + name
                                              : ncei + "model-cfs-allfile-reanalysis/time-series/" + month + "/" + name;
            };
            return s;
        };
        return def;
    }
}

namespace {
    // The RUC / RAP analysis through the years: the RUC's files before May 2012 (ruc2anl_130) and the RAP's after (rap_130), in NCEI's historical folder
    GfsModels::Def rapAnalysis() {
        auto def = fromFile("RAP-ANL");
        const auto base = def.source;
        def.source = [base] (const std::string& storm) {
            auto s = base(storm);
            s.fileUrl = [] (const GfsData::Run& run, int, const std::string&) {
                if (run.date >= "20200516") {   // the folder that goes on to the present (its indexes are named ".grb2.inv")
                    return "https://www.ncei.noaa.gov/oa/prod-model/rapid-refresh/access/rap-130-13km/analysis/" + run.date.substr(0, 6) + "/" + run.date + "/rap_130_" + run.date + "_" + run.cycle + "00_000.grb2";
                }
                const std::string stem = run.date >= "20120501" ? "rap_130_" : "ruc2anl_130_";
                return "https://www.ncei.noaa.gov/oa/prod-model/rapid-refresh/access/historical/analysis/" + run.date.substr(0, 6) + "/" + run.date + "/" + stem + run.date + "_" + run.cycle + "00_000.grb2";
            };
            s.indexOf = [] (const std::string& url) {
                return url.find("/rap-130-13km/") != std::string::npos ? url + ".inv" : url.substr(0, url.size() - 5) + ".inv";
            };
            return s;
        };
        return def;
    }
}

namespace {
    // the 1 degree GFS analysis: GRIB1 files until 7 April 2017, GRIB2 after
    GfsModels::Def gfs1Analysis() {
        auto def = fromFile("GFS1-ANL");
        const auto base = def.source;
        def.source = [base] (const std::string& storm) {
            auto s = base(storm);
            const auto first = s.fileUrl;
            s.fileUrl = [first] (const GfsData::Run& run, int hour, const std::string& file) {
                auto url = first(run, hour, file);
                return run.date >= "20170407" ? url + "2" : url;
            };
            return s;
        };
        return def;
    }
}

namespace {
    // the RTMA and URMA files: a different ending in each era of the buckets
    GfsModels::Def surfaceAnalysis(const std::string& id) {
        auto def = fromFile(id);
        const auto base = def.source;
        const bool rtma = id == "RTMA";
        def.source = [base, rtma] (const std::string& storm) {
            auto s = base(storm);
            s.fileUrl = [rtma] (const GfsData::Run& run, int, const std::string&) {
                const std::string low = rtma ? "rtma" : "urma";
                // RTMA: ".grb2" to 28 Jan 2014, "_ext" to 1 May 2017, then "_wexp"; URMA: "_ext" to 30 Apr 2017, then "_wexp"
                const std::string ending = rtma && run.date <= "20140128" ? "grb2" : run.date < (rtma ? "20170502" : "20170501") ? "grb2_ext" : "grb2_wexp";
                return "https://noaa-" + low + "-pds.s3.amazonaws.com/" + low + "2p5." + run.date + "/" + low + "2p5.t" + run.cycle + "z.2dvaranl_ndfd." + ending;
            };
            return s;
        };
        return def;
    }
}

namespace {
    // ERA5 at NCAR: a file for each variable, a month of hours (the surface) or a day of hours and 37 levels (the upper air), the field's hour and level a band of it
    struct EraName {
        const char * file;       // "128_167_2t.ll025sc"
        const char * variable;   // the NetCDF name
    };

    bool eraNameOf(const GfsData::Want& w, EraName& out, bool& upper) {
        const auto& v = w.variable;
        const auto& l = w.level;
        upper = l.size() > 3 && l.compare(l.size() - 3, 3, " mb") == 0;
        if (upper) {
            if (v == "HGT") out = {"128_129_z.ll025sc", "Z"};
            else if (v == "TMP") out = {"128_130_t.ll025sc", "T"};
            else if (v == "UGRD") out = {"128_131_u.ll025uv", "U"};
            else if (v == "VGRD") out = {"128_132_v.ll025uv", "V"};
            else if (v == "SPFH") out = {"128_133_q.ll025sc", "Q"};
            else if (v == "VVEL") out = {"128_135_w.ll025sc", "W"};
            else if (v == "RH") out = {"128_157_r.ll025sc", "R"};
            else return false;
            return true;
        }
        if (v == "TMP" && l == "2 m above ground") out = {"128_167_2t.ll025sc", "VAR_2T"};
        else if (v == "DPT" && l == "2 m above ground") out = {"128_168_2d.ll025sc", "VAR_2D"};
        else if (v == "UGRD" && l == "10 m above ground") out = {"128_165_10u.ll025sc", "VAR_10U"};
        else if (v == "VGRD" && l == "10 m above ground") out = {"128_166_10v.ll025sc", "VAR_10V"};
        else if (v == "PRMSL") out = {"128_151_msl.ll025sc", "MSL"};
        else if (v == "PRES" && l == "surface") out = {"128_134_sp.ll025sc", "SP"};
        else if (v == "PWAT") out = {"128_137_tcwv.ll025sc", "TCWV"};
        else if (v == "CAPE") out = {"128_059_cape.ll025sc", "CAPE"};
        else if (v == "TCDC") out = {"128_164_tcc.ll025sc", "TCC"};
        else return false;
        return true;
    }

    GfsModels::Def era5() {
        auto def = fromFile("ERA5");
        const auto base = def.source;
        def.source = [base] (const std::string& storm) {
            auto s = base(storm);
            s.fileOf = [] (const GfsData::Want& w) {
                EraName n{};
                bool upper = false;
                return eraNameOf(w, n, upper) ? std::string{upper ? "pl:" : "sfc:"} + n.file : std::string{"none"};
            };
            s.variableOf = [] (const GfsData::Want& w) {
                EraName n{};
                bool upper = false;
                return eraNameOf(w, n, upper) ? std::string{n.variable} : w.variable;
            };
            s.fileUrl = [] (const GfsData::Run& run, int, const std::string& file) {
                const bool upper = file.compare(0, 3, "pl:") == 0;
                const std::string stem = file.substr(file.find(':') + 1);
                const std::string month = run.date.substr(0, 6);
                const QDate day = QDate::fromString(QString::fromStdString(run.date), "yyyyMMdd");
                if (upper) {
                    return "https://nsf-ncar-era5.s3.amazonaws.com/e5.oper.an.pl/" + month + "/e5.oper.an.pl." + stem + "." + run.date + "00_" + run.date + "23.nc";
                }
                const auto last = QDate{day.year(), day.month(), day.daysInMonth()}.toString("yyyyMMdd").toStdString();
                return "https://nsf-ncar-era5.s3.amazonaws.com/e5.oper.an.sfc/" + month + "/e5.oper.an.sfc." + stem + "." + month + "0100_" + last + "23.nc";
            };
            s.ncBand = [] (const GfsData::Run& run, int, const GfsData::Want& w) {
                const int hour = std::atoi(run.cycle.c_str());
                EraName n{};
                bool upper = false;
                if (!eraNameOf(w, n, upper)) {
                    return 0;
                }
                if (!upper) {
                    return (std::atoi(run.date.substr(6, 2).c_str()) - 1) * 24 + hour + 1;
                }
                static const int levels[] = {1, 2, 3, 5, 7, 10, 20, 30, 50, 70, 100, 125, 150, 175, 200, 225, 250, 300, 350, 400, 450, 500, 550, 600, 650, 700, 750, 775, 800, 825, 850, 875, 900, 925, 950, 975, 1000};
                const int mb = std::atoi(w.level.c_str());
                for (int i = 0; i < 37; i++) {
                    if (levels[i] == mb) {
                        return hour * 37 + i + 1;
                    }
                }
                return 0;
            };
            s.valueScale = [] (const GfsData::Want& w) {
                return w.variable == "HGT" ? 1.0 / 9.80665 : w.variable == "TCDC" ? 100.0 : 1.0;   // geopotential to meters, a fraction to percent
            };
            s.valueOffset = [] (const GfsData::Want& w) {
                return w.variable == "TMP" || w.variable == "DPT" ? -273.15 : 0.0;
            };
            return s;
        };
        return def;
    }
}

std::string GfsModels::pickerGroup(const std::string& id) {
    if (const auto * def = find(id); def != nullptr && !def->group.empty()) {
        return def->group;
    }
    static const std::map<std::string, std::string> groups{
        {"GFS", "Global"}, {"AIGFS", "Global"}, {"IFS", "Global"}, {"AIFS", "Global"}, {"GEOS-FP", "Global"}, {"CFS", "Seasonal"}, {"CFS-ANL", "Seasonal"},
        {"NAM", "Regional"}, {"RAP", "Regional"}, {"NBM", "Regional"}, {"FIREWX", "Regional"},
        {"HRRR", "Convection allowing"}, {"RRFS", "Convection allowing"}, {"RRFS-AK", "Convection allowing"}, {"HRRR-AK", "Convection allowing"}, {"ICON-EU", "Regional"}, {"ICON-D2", "Convection allowing"}, {"HRDPS", "Convection allowing"}, {"RRFS-HI", "Convection allowing"}, {"RRFS-PR", "Convection allowing"}, {"RRFS-NA", "Regional"}, {"NAM-HIRES", "Convection allowing"}, {"HRW-ARW", "Convection allowing"},
        {"HRW-ARW2", "Convection allowing"}, {"HRW-FV3", "Convection allowing"},
        {"GEFS", "Ensembles"}, {"GEPS", "Ensembles"}, {"IFS-ENS", "Ensembles"}, {"ENS-ALL", "Ensembles"}, {"REFS", "Ensembles"}, {"HREF", "Ensembles"}, {"NAEFS", "Ensembles"},
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
        m.push_back(fromFile("IFS-ENS"));
        m.push_back(fromFile("ENS-ALL"));
        m.push_back(fromFile("GEFS"));
        m.push_back(fromFile("GEPS"));
        m.push_back(fromFile("GEOS-FP"));
        m.push_back(fromFile("CFS"));
        m.push_back(fromFile("HRDPS"));
        m.push_back(fromFile("ICON-D2"));
        m.push_back(cfsAnalysis());
        m.push_back(rapAnalysis());
        m.push_back(fromFile("GFS-ANL"));
        m.push_back(era5());
        m.push_back(fromFile("NAM-ANL"));
        m.push_back(gfs1Analysis());
        m.push_back(fromFile("NARR"));
        m.push_back(surfaceAnalysis("RTMA"));
        m.push_back(surfaceAnalysis("URMA"));
        m.push_back(fromFile("RRFS"));
        m.push_back(fromFile("RRFS-AK"));
        m.push_back(fromFile("RRFS-HI"));
        m.push_back(fromFile("RRFS-PR"));
        m.push_back(fromFile("RRFS-NA"));
        m.push_back(fromFile("HRRR"));
        m.push_back(fromFile("HRRR-AK"));
        m.push_back(fromFile("ICON-EU"));
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

bool GfsModels::isStormModel(const std::string& id) {
    const auto * def = find(id);
    return def != nullptr && def->storm;
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
