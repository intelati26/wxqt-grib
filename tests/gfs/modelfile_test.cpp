// The models described by definition files (src/gfs/GfsModelFile.*) must be the models the program had before they were: this prints what each ported model's source and registry entry
// do (urls, file names, hours, clone rules) and compares it with a golden text made from the code they were written in.
//   modelfile_test --write golden.txt    makes the golden;   modelfile_test golden.txt    compares
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include "gfs/GfsData.h"
#include "gfs/GfsModelFile.h"
#include "gfs/GfsModels.h"

static std::string describe(const std::string& id) {
    std::ostringstream out;
    const auto * def = GfsModels::find(id);
    if (def == nullptr) {
        return "== " + id + "\nMISSING\n";
    }
    const auto source = def->source("");
    out << "== " << id << "\n";
    out << "label=" << def->label << " source.id=" << source.id << " source.label=" << source.label << "\n";
    out << "sectors=" << static_cast<int>(def->sectors) << " hourlyRuns=" << def->hourlyRuns << " cycleStep=" << def->cycleStep << " overlays=" << def->overlays << " recipeList=" << def->recipeList
        << " onMissing=" << static_cast<int>(def->onMissing) << " preloadHours=" << def->preloadHours << " storm=" << def->storm << "\n";
    out << "hours=";
    for (const auto& h : def->hours) {
        out << h.from << "-" << h.to << "/" << h.step << ",";
    }
    out << "\n";
    if (def->runLength) {
        out << "runLength=";
        for (int c = 0; c < 24; c++) {
            out << def->runLength(c) << ",";
        }
        out << "\n";
    }
    const auto& c = def->clone;
    out << "clone enabled=" << c.enabled << " specificHumidity=" << c.specificHumidity << " pieces=" << c.pieces << " prefix=" << c.labelPrefix << "\nids=";
    for (const auto& s : c.ids) out << s << ",";
    out << "\nvariables=";
    for (const auto& s : c.variables) out << s << ",";
    out << "\nlevels=";
    for (const auto& s : c.levels) out << s << ",";
    out << "\nfields=";
    for (const auto& s : c.fields) out << s << ",";
    out << "\nskip=";
    for (const auto& s : c.skip) out << s << ",";
    out << "\nplain=";
    for (const auto& s : c.plain) out << s << ",";
    out << "\nrename=";
    for (const auto& [a, b] : c.rename) out << a << ">" << b << ",";
    out << "\n";
    out << "source cycleHours=" << source.cycleHours << " lagHours=" << source.lagHours << " probeHour=" << source.probeHour << " probeFile=" << source.probeFile << " cyclesToTry=" << source.cyclesToTry
        << " maxParallel=" << source.maxParallel << " wholeFile=" << source.wholeFile << " netcdf=" << source.netcdf << " custom=" << source.custom << " indexSuffix=" << source.indexSuffix
        << " indexReplaces=" << source.indexReplaces << " defaultDetail=" << source.defaultDetail << " extraMissing=" << source.extraMissing << " warp=" << source.warp.enabled << ":" << source.warp.step << ","
        << source.warp.west << "," << source.warp.south << "," << source.warp.east << "," << source.warp.north << " hasParse=" << static_cast<bool>(source.parseIndex) << " hasSubset=" << static_cast<bool>(source.subsetUrl) << "\n";
    if (source.subsetUrl) {
        out << "subset=" << source.subsetUrl({"20261010", "12"}, 36, {"HGT", "TMP"}, {"500 mb", "2 m above ground"}, GfsData::Box{-130.0, 20.0, -60.0, 55.0}) << "\n";
    }
    if (def->storm) {
        const auto stormy = def->source("09l");
        out << "storm source.id=" << stormy.id << " label=" << stormy.label << "\n";
        for (const char * file : {"", "swath", "trak", "ww3", "storm.atm", "storm.sat"}) {
            out << "storm file=" << file << " url=" << stormy.fileUrl({"20261010", "12"}, 36, file) << "\n";
        }
    }
    const std::vector<GfsData::Run> runs{{"20261010", "00"}, {"20261011", "18"}};
    const std::vector<int> hours{1, 120};
    const std::vector<GfsData::Want> wants{
        {"a", "HGT", "500 mb", "", "", ""}, {"a", "TMP", "2 m above ground", "", "", ""}, {"a", "UGRD", "10 m above ground", "", "", ""}, {"a", "PRMSL", "mean sea level", "", "", ""},
        {"a", "PWAT", "entire atmosphere (considered as a single layer)", "", "", ""}, {"a", "CAPE", "surface", "", "", ""}, {"a", "APCP", "surface", "0-6 hour acc fcst", "", ""},
        {"a", "RH", "1000 mb", "", "", ""}, {"a", "SPFH", "2 m above ground", "", "", ""}, {"a", "WIND", "10 m above ground", "", "", ""},
        {"a", "TMP", "2 m above ground", "", "", "spr"}, {"a", "TMP", "2 m above ground", "", "", "p07"}, {"a", "TMP", "2 m above ground", "", "", "c00"},
        {"a", "PROB", "refsct/refsct_1hr", "", "", ""}, {"a", "PROB", "grefs/grefs_{ee}.4hr;grefs/grefs_{pe}.4hr", "", "", "b2"}, {"a", "PROB", "gefs_mlp/gefsmlp_tor_std", "", "", ""},
        {"a", "sb_cape", "", "", "", ""}, {"a", "HGT", "500 mb", "", "", "mean"}, {"a", "HGT", "500 mb", "", "", "sprd"},
        // the edge cases of the models that were code: GEFS's three sets of files and its members, the REFS's three places, HAFS's track, waves and swath, and the REFS products by name
        {"a", "CAPE", "255-0 mb above ground", "", "", ""}, {"a", "CIN", "surface", "", "", "p30"}, {"a", "GUST", "surface", "", "", "spr"}, {"a", "MSLET", "mean sea level", "", "", "c00"},
        {"a", "HLCY", "3000-0 m above ground", "", "", ""}, {"a", "VIS", "surface", "", "", ""}, {"a", "TMP", "850 mb", "", "", "m002"}, {"a", "TMP", "2 m above ground", "", "", "rrfs"},
        {"a", "TMP", "500 mb", "", "", "rrfs"}, {"a", "REFC", "entire atmosphere", "", "", "ens:avrg"}, {"a", "APCP", "surface", "", "", "ens:eas"}, {"a", "HGT", "500 mb", "", "", "swath"},
        {"a", "varBT", "", "", "", ""}, {"a", "TMP", "2 m above ground", "", "", "trak"}, {"a", "cref", "", "", "", "m001"}, {"a", "cref", "", "", "", ".zgroup"}};
    for (const auto& run : runs) {
        for (const int hour : hours) {
            for (const auto& w : wants) {
                const auto file = source.fileOf ? source.fileOf(w) : std::string{};
                out << run.id() << " f" << hour << " " << w.variable << "|" << w.level << "|" << w.stat << " file=" << file << " url=" << source.fileUrl(run, hour, file);
                if (source.candidates) {
                    out << " candidates=";
                    for (const auto& u : source.candidates(run, hour, file)) {
                        out << u << " ";
                    }
                }
                if (source.valueScale) {
                    out << " scale=" << source.valueScale(w);
                }
                if (source.memberBand && !w.stat.empty()) {
                    out << " band=" << source.memberBand(w.stat);
                }
                out << "\n";
            }
        }
    }
    return out.str();
}

// what a mistake in a file says
static int mistakes() {
    int bad = 0;
    const auto expect = [&bad] (const std::string& text, const std::string& part) {
        const auto parsed = GfsModelFile::parse(text, "test");
        if (parsed.ok || parsed.error.find(part) == std::string::npos) {
            std::printf("FAIL: expected an error with \"%s\" for:\n%s\n got: %s\n", part.c_str(), text.c_str(), parsed.ok ? "(parsed)" : parsed.error.c_str());
            bad++;
        }
    };
    expect("label = x\nurl = http://x/{date}\nhours = 0-6/3\n", "no id");
    expect("id = A\nlabel = x\nhours = 0-6/3\n", "no url");
    expect("id = A\nlabel = x\nurl = http://x\n", "no hours");
    expect("id = A\nlabel = x\nurl = http://x\nhours = 6-0/3\n", "hours is from-to/step");
    expect("id = A\nlabel = x\nurl = http://x\nhours = 0-6/3\ncolour = red\n", "unknown key colour");
    expect("id = A\nlabel = x\nurl = http://x\nhours = 0-6/3\nfile = when depth=3 => a\n", "unknown condition field depth");
    expect("id = A\nlabel = x\nurl = http://x\nhours = 0-6/3\nkind = grid\n", "kind is grib, whole");
    const auto good = GfsModelFile::parse("id = A\nlabel = x\nkind = whole\nurl = http://x/{date}/{cycle}/{file}_{hour3}\nfile = when level=\"2 m above ground\" => {variable}_2m\nfile = {variable}\nhours = 0-6/3\n", "test");
    if (!good.ok) {
        std::printf("FAIL: a good file did not parse: %s\n", good.error.c_str());
        bad++;
    } else {
        const auto source = good.def.source("");
        const GfsData::Want w{"a", "TMP", "2 m above ground", "", "", ""};
        if (source.fileOf(w) != "TMP_2m" || source.fileUrl({"20261010", "12"}, 6, source.fileOf(w)) != "http://x/20261010/12/TMP_2m_006") {
            std::printf("FAIL: a good file made the wrong url: %s\n", source.fileUrl({"20261010", "12"}, 6, source.fileOf(w)).c_str());
            bad++;
        }
    }
    return bad;
}

int main(int argc, char ** argv) {
    // the models that are written as definition files
    const std::vector<std::string> ids{"GFS", "NBM", "GEFS", "REFS", "HAFSA", "HAFSB", "SPCREFS", "GFS-WAVE", "GEFS-WAVE", "AIGFS", "IFS", "AIFS", "RRFS", "GEPS", "SPCPOST", "SPCGEFS", "SFCOA", "RRFSD", "HREF"};
    std::string all;
    for (const auto& id : ids) {
        all += describe(id);
    }
    if (argc == 3 && std::string{argv[1]} == "--write") {
        std::ofstream{argv[2]} << all;
        std::printf("wrote %s (%zu bytes)\n", argv[2], all.size());
        return 0;
    }
    if (argc != 2) {
        std::printf("usage: modelfile_test [--write] golden.txt\n");
        return 2;
    }
    if (mistakes() != 0) {
        return 1;
    }
    std::ifstream in{argv[1]};
    const std::string golden{std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
    if (golden == all) {
        std::printf("definition files match the golden (%zu bytes)\n", all.size());
        return 0;
    }
    // the first line that differs
    std::istringstream a{golden}, b{all};
    std::string x, y;
    int line = 0;
    while (std::getline(a, x) && std::getline(b, y)) {
        line++;
        if (x != y) {
            std::printf("first difference at line %d\n golden: %s\n now:    %s\n", line, x.c_str(), y.c_str());
            break;
        }
    }
    return 1;
}
