// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

// Override files (section 20 of the plan, A): `override = GFS` in a .model file of the person's folder changes only the keys it lists. The merge of the text, what is not allowed, and the registry: a
// built-in model read with the person's lines applied, tagged with the file; a file that is not allowed leaves the model as it is and is reported.
#include <cstdio>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QSettings>
#include "util/MyApplication.h"
#include "gfs/GfsModelFile.h"
#include "gfs/GfsModels.h"

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL line %d: %s\n", __LINE__, #c); failures++; } } while (0)

namespace {
    bool has(const std::string& text, const std::string& line) {
        return text.find(line + "\n") != std::string::npos;
    }
    void write(const QString& path, const QByteArray& text) {
        QFile f{path};
        (void) f.open(QIODevice::WriteOnly | QIODevice::Truncate);
        f.write(text);
    }
}

int main(int argc, char ** argv) {
    QGuiApplication app{argc, argv};
    MyApplication::preferences = new QSettings{"/tmp/wxqt_override_test_prefs.ini", QSettings::IniFormat};
    std::string merged, why;

    // ---- the merge of the text
    const std::string base =
        "id = TEST\nlabel = A test model\n# a comment\nlag_hours = 4\ncycle_hours = 6\nurl = {@a}/one/{date}\nurl = when file=x => {@b}/two/{date}\nurl = when file=y => {@c}/three/{date}\nfile = main\nclone = true\nhours = 0-48/3\n";
    CHECK(GfsModelFile::overrideTarget("# x\noverride = GFS\nlag_hours = 5\n") == "GFS" && GfsModelFile::overrideTarget(base).empty());
    CHECK(GfsModelFile::mergeOverride(base, "override = TEST\nlag_hours = 9\n", merged, why) && has(merged, "lag_hours = 9") && !has(merged, "lag_hours = 4") && has(merged, "cycle_hours = 6") && has(merged, "# a comment"));
    // prepend and append around the built-in urls
    CHECK(GfsModelFile::mergeOverride(base, "override = TEST\nurl_prepend = {@mirror}/one/{date}\nurl_append = {@last}/one/{date}\n", merged, why));
    CHECK(merged.find("url = {@mirror}/one/{date}\nurl = {@a}/one/{date}") != std::string::npos && merged.find("{@c}/three/{date}\nurl = {@last}/one/{date}") != std::string::npos);
    // urls as a set: the built-in ones are gone, the file's take their place
    CHECK(GfsModelFile::mergeOverride(base, "override = TEST\nurl = {@new}/x/{date}\nurl = {@new2}/y/{date}\n", merged, why) && !has(merged, "url = {@a}/one/{date}") && merged.find("url = {@new}/x/{date}\nurl = {@new2}/y/{date}\nfile = main") != std::string::npos);
    // a when rule with a condition the model has replaces that rule only
    CHECK(GfsModelFile::mergeOverride(base, "override = TEST\nurl = when file=x => {@other}/two/{date}\n", merged, why) && has(merged, "url = {@a}/one/{date}") && has(merged, "url = when file=x => {@other}/two/{date}") && has(merged, "url = when file=y => {@c}/three/{date}") && !has(merged, "url = when file=x => {@b}/two/{date}"));
    // a key the model does not have is added
    CHECK(GfsModelFile::mergeOverride(base, "override = TEST\nprobe_hour = 3\n", merged, why) && has(merged, "probe_hour = 3"));
    // what is not allowed
    for (const char * bad : {"override = TEST\nid = OTHER\n", "override = TEST\nkind = tar\n", "override = TEST\nlabel = Mine\n", "override = TEST\nclone_levels = 1 mb\n", "override = TEST\ninclude = x\n"}) {
        CHECK(!GfsModelFile::mergeOverride(base, bad, merged, why) && why.find("cannot be overridden") != std::string::npos);
    }

    // ---- the registry: an override of a built-in model, one that is not allowed, one for a model that is not there
    const QString folder = "/tmp/wxqt_override_test_folder";
    QDir{folder}.removeRecursively();
    QDir{}.mkpath(folder);
    const auto original = GfsModelFile::builtInText("GFS");
    CHECK(!original.empty() && original.find("lag_hours") != std::string::npos);
    write(folder + "/gfs-mine.model", "# my change\noverride = GFS\nlag_hours = 11\nurl_prepend = {@mirror}/gfs.{date}/{cycle}/atmos/gfs.t{cycle}z.pgrb2.0p25.f{hour3}\n");
    write(folder + "/bad.model", "override = GEFS\nid = GEFS2\n");
    write(folder + "/nothere.model", "override = NOT-A-MODEL\nlag_hours = 1\n");
    write(folder + "/broken.model", "override = NBM\nlag_hours = not a number\n");
    GfsModels::setUserFolder(folder.toStdString());
    const auto * gfs = GfsModels::find("GFS");
    CHECK(gfs != nullptr && gfs->overriddenBy == "gfs-mine.model" && gfs->source("").lagHours == 11);
    const auto * gefs = GfsModels::find("GEFS");
    CHECK(gefs != nullptr && gefs->overriddenBy.empty());   // (the not allowed override leaves it as it is)
    const auto * nbm = GfsModels::find("NBM");
    CHECK(nbm != nullptr && nbm->overriddenBy.empty());     // (a value that does not parse: the model as it is)
    const auto problems = GfsModels::userProblems();
    const auto has2 = [&problems] (const std::string& part) { return std::any_of(problems.begin(), problems.end(), [&part] (const std::string& p) { return p.find(part) != std::string::npos; }); };
    CHECK(has2("bad.model") && has2("cannot be overridden") && has2("nothere.model") && has2("not a model that comes with the program") && has2("broken.model"));
    CHECK(!has2("gfs-mine.model"));   // (the good one has no problem)
    CHECK(GfsModels::find("OVERRIDE") == nullptr && GfsModels::find("GFS")->id == "GFS");
    QDir{folder}.removeRecursively();
    std::printf(failures == 0 ? "all override file tests passed\n" : "%d failures\n", failures);
    return failures == 0 ? 0 : 1;
}
