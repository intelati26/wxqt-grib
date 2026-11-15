// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

// The catalogue check (section 20 of the plan): what a model's charts ask for against what its index lists, with an index made up here: found, gone, never found, not cataloged, and the kept status.
#include <cstdio>
#include <QCoreApplication>
#include <QDir>
#include <QJsonArray>
#include <QSettings>
#include <QStandardPaths>
#include "gfs/CatalogCheck.h"
#include "gfs/GfsData.h"
#include "gfs/GfsModels.h"
#include "util/MyApplication.h"

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL line %d: %s\n", __LINE__, #c); failures++; } } while (0)

namespace {
    // an index of NOAA's kind: the fields listed
    QByteArray index(const std::vector<std::string>& lines) {
        std::string text;
        long long at = 0;
        int n = 1;
        for (const auto& l : lines) {
            text += std::to_string(n++) + ":" + std::to_string(at) + ":d=2026111500:" + l + "\n";
            at += 1000;
        }
        return QByteArray::fromStdString(text);
    }
    GfsData::Need need(const char * variable, const char * level) {
        return {0, GfsData::Want{"k", variable, level, "", "", ""}};
    }
}

int main(int argc, char ** argv) {
    QCoreApplication app{argc, argv};
    QStandardPaths::setTestModeEnabled(true);
    QDir{QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)}.removeRecursively();
    MyApplication::preferences = new QSettings{"/tmp/wxqt_catalogcheck_test_prefs.ini", QSettings::IniFormat};
    const auto * def = GfsModels::find("GFS");
    CHECK(def != nullptr);
    if (def == nullptr) {
        return 1;
    }
    GfsData::Config config;
    QByteArray listing = index({"HGT:500 mb:anl:", "UGRD:500 mb:anl:", "VGRD:500 mb:anl:", "RH:700 mb:anl:"});
    config.bytes = [&listing] (const std::string& url, long long, long long) { return url.find(".idx") != std::string::npos ? listing : QByteArray{"GRIB"}; };
    const GfsData data{config, def->source("")};
    const GfsData::Run run{"20261115", "00"};
    const std::vector<GfsData::Need> asked{need("HGT", "500 mb"), need("TMP", "500 mb"), need("UGRD", "500 mb")};
    const auto inventory = data.inventory(run, asked);
    CHECK(inventory.checked);
    CHECK(inventory.found.size() == 2 && inventory.gone.size() == 1 && inventory.gone[0].variable == "TMP" && inventory.gone[0].level == "500 mb");
    // listed and not asked: VGRD 500 and RH 700 (the plain records), not the ones that are asked
    CHECK(inventory.notCataloged.size() == 2);
    // the first check has nothing to compare with: a missing field was never found
    auto first = CatalogCheck::make("GFS", run.id(), inventory, {}, false);
    CHECK(first.firstCheck && first.gone.empty() && first.neverFound.size() == 1 && first.found.size() == 2 && first.notCataloged.size() == 2);
    CatalogCheck::keep(first);
    bool had = false;
    auto kept = CatalogCheck::keptFound("GFS", had);
    CHECK(had && kept.size() == 2);
    // the next check: TMP is listed (it was asked and found), then it is gone from a later index
    listing = index({"HGT:500 mb:anl:", "UGRD:500 mb:anl:", "TMP:500 mb:anl:"});
    auto second = CatalogCheck::make("GFS", run.id(), data.inventory(run, asked), kept, had);
    CHECK(second.gone.empty() && second.neverFound.empty() && second.found.size() == 3);
    CatalogCheck::keep(second);
    kept = CatalogCheck::keptFound("GFS", had);
    listing = index({"HGT:500 mb:anl:", "UGRD:500 mb:anl:", "T:500 mb:anl:"});   // TMP renamed T
    auto third = CatalogCheck::make("GFS", run.id(), data.inventory(run, asked), kept, had);
    CHECK(third.gone.size() == 1 && third.gone[0].variable == "TMP");              // it was there last time: gone
    CHECK(third.neverFound.empty() && third.notCataloged.size() == 1 && third.notCataloged[0].variable == "T");   // and its new name is listed and not cataloged
    CHECK(third.summary().find("1 gone") != std::string::npos);
    CatalogCheck::keep(third);
    kept = CatalogCheck::keptFound("GFS", had);
    auto fourth = CatalogCheck::make("GFS", run.id(), data.inventory(run, asked), kept, had);
    CHECK(fourth.gone.size() == 1);   // (still gone, not forgotten, until it is listed again)
    // a model whose index cannot be read is not checked, and keeps what the last check found
    config.bytes = [] (const std::string&, long long, long long) { return QByteArray{}; };
    const GfsData silent{config, def->source("")};
    const auto none = silent.inventory(run, asked);
    CHECK(!none.checked && !none.note.empty());
    auto skipped = CatalogCheck::make("GFS", run.id(), none, kept, had);
    CatalogCheck::keep(skipped);
    CHECK(CatalogCheck::keptFound("GFS", had).size() == kept.size());
    CHECK(skipped.summary().rfind("not checked", 0) == 0);
    // the report as JSON: the lists and the counts
    const auto json = CatalogCheck::toJson(third);
    CHECK(json["gone"].toArray().size() == 1 && json["not_cataloged_count"].toInt() == 1 && json["checked"].toBool());
    std::printf(failures == 0 ? "all catalogue check tests passed\n" : "%d failures\n", failures);
    return failures == 0 ? 0 : 1;
}
