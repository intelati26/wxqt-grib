// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

// The servers by name (section 20 of the plan): the file's checks, the layers, the {@name} places, a model's urls with the table, and the built-in text being the repository's file.
#include <cstdio>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QSettings>
#include <QStandardPaths>
#include "gfs/GfsModels.h"
#include "gfs/Servers.h"
#include "util/MyApplication.h"
#include "util/Utility.h"

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL line %d: %s\n", __LINE__, #c); failures++; } } while (0)

namespace {
    QByteArray doc(const char * body) {
        return QByteArray{"{\"version\": 1, \"servers\": {"} + body + "}}";
    }
}

int main(int argc, char ** argv) {
    QCoreApplication app{argc, argv};
    QDir{"/tmp/wxqt_servers_test_data"}.removeRecursively();
    QStandardPaths::setTestModeEnabled(true);   // (the folders of the program's data are the test ones)
    QDir{QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)}.removeRecursively();
    MyApplication::preferences = new QSettings{"/tmp/wxqt_servers_test_prefs.ini", QSettings::IniFormat};
    MyApplication::preferences->clear();

    // the built-in text is the file of the repository
    {
        QFile file{QString::fromStdString(std::string{__FILE__}.substr(0, std::string{__FILE__}.rfind("tests/")) + "servers/servers.json")};
        CHECK(file.open(QIODevice::ReadOnly));
        CHECK(file.readAll().toStdString() == Servers::builtInJson());
    }
    // a valid file, the checks that refuse a bad one
    Servers::Table t;
    std::string why;
    CHECK(Servers::parse(doc("\"a-b\": {\"bases\": [\"https://noaa-x.s3.amazonaws.com/\", \"https://nomads.ncep.noaa.gov/pub\"], \"note\": \"n\"}"), "project", false, t, why));
    CHECK(t.count("a-b") == 1 && t["a-b"].bases.size() == 2 && t["a-b"].bases[0] == "https://noaa-x.s3.amazonaws.com" && t["a-b"].origin == "project");   // (no trailing slash)
    Servers::Table u;
    CHECK(!Servers::parse(doc("\"a\": {\"bases\": [\"http://nomads.ncep.noaa.gov\"]}"), "project", false, u, why) && why.find("https") != std::string::npos && u.empty());
    CHECK(!Servers::parse(doc("\"a\": {\"bases\": [\"https://evil.example.com/x\"]}"), "project", false, u, why) && why.find("not one the program reads from") != std::string::npos);
    CHECK(Servers::parse(doc("\"a\": {\"bases\": [\"https://evil.example.com/x\"]}"), "yours", true, u, why));   // (any host, for the person's own or when they allow it)
    CHECK(!Servers::parse(doc("\"A_b\": {\"bases\": [\"https://nomads.ncep.noaa.gov\"]}"), "project", true, u, why) && why.find("plain name") != std::string::npos);
    CHECK(!Servers::parse(doc("\"a\": {\"bases\": []}"), "project", true, u, why));
    CHECK(!Servers::parse(doc("\"a\": {\"bases\": [\"https://nomads.ncep.noaa.gov/a b\"]}"), "project", true, u, why));
    CHECK(!Servers::parse(doc("\"a\": {\"bases\": [\"https://nomads.ncep.noaa.gov/?x=1\"]}"), "project", true, u, why));
    CHECK(!Servers::parse(QByteArray{"{\"version\": 2, \"servers\": {}}"}, "project", true, u, why) && !Servers::parse("not json", "project", true, u, why) && !Servers::parse(QByteArray{}, "project", true, u, why));
    CHECK(!Servers::parse(QByteArray(70000, ' '), "project", true, u, why));
    Servers::Table partial;   // one bad entry: nothing of the file is taken
    CHECK(!Servers::parse(doc("\"good\": {\"bases\": [\"https://nomads.ncep.noaa.gov\"]}, \"bad\": {\"bases\": [\"ftp://x\"]}"), "project", true, partial, why) && partial.empty());
    CHECK(Servers::allowedHost("noaa-rrfs-ops-pds.s3.amazonaws.com") && Servers::allowedHost("nomads.ncep.noaa.gov") && !Servers::allowedHost("amazonaws.com.evil.org") && !Servers::allowedHost("evilnoaa.gov"));

    // the layers: built in, the project's file over it, the person's over that, Settings over all
    Servers::reload();
    auto table = Servers::table();
    CHECK(table.count("rrfs-ops") == 1 && table["rrfs-ops"].origin == "built in" && table["rrfs-ops"].bases.size() == 2);
    QDir{}.mkpath(QString::fromStdString(Servers::projectFile()).section('/', 0, -2));
    {
        QFile f{QString::fromStdString(Servers::projectFile())};
        CHECK(f.open(QIODevice::WriteOnly));
        f.write(doc("\"rrfs-ops\": {\"bases\": [\"https://noaa-rrfs-ops-pds.s3.us-east-1.amazonaws.com\"]}, \"new-one\": {\"bases\": [\"https://nomads.ncep.noaa.gov/pub\"]}"));
    }
    Servers::reload();
    table = Servers::table();
    CHECK(table["rrfs-ops"].origin == "project" && table["rrfs-ops"].bases.size() == 1 && table["rrfs-ops"].bases[0] == "https://noaa-rrfs-ops-pds.s3.us-east-1.amazonaws.com");
    CHECK(table["new-one"].origin == "project" && table["gfs-aws"].origin == "built in");   // (what the file does not name stays)
    {
        QDir{}.mkpath(QString::fromStdString(Servers::userFile()).section('/', 0, -2));
        QFile f{QString::fromStdString(Servers::userFile())};
        CHECK(f.open(QIODevice::WriteOnly));
        f.write(doc("\"rrfs-ops\": {\"bases\": [\"https://mirror.example.org/rrfs\", \"https://noaa-rrfs-ops-pds.s3.amazonaws.com\"]}"));
    }
    Servers::reload();
    table = Servers::table();
    CHECK(table["rrfs-ops"].origin == "yours" && table["rrfs-ops"].bases.size() == 2 && table["rrfs-ops"].bases[0] == "https://mirror.example.org/rrfs");
    Utility::writePref("SERVER_rrfs-ops", "https://typed.example.net/x/");
    Servers::reload();
    table = Servers::table();
    CHECK(table["rrfs-ops"].origin == "settings" && table["rrfs-ops"].bases.size() == 3 && table["rrfs-ops"].bases[0] == "https://typed.example.net/x");
    // a project file that is not valid is not used at all
    {
        QFile f{QString::fromStdString(Servers::projectFile())};
        CHECK(f.open(QIODevice::WriteOnly));
        f.write(doc("\"gfs-aws\": {\"bases\": [\"https://evil.example.com\"]}"));
    }
    Utility::writePref("SERVER_rrfs-ops", "");
    QFile::remove(QString::fromStdString(Servers::userFile()));
    Servers::reload();
    table = Servers::table();
    CHECK(table["gfs-aws"].origin == "built in" && table["gfs-aws"].bases[0].find("evil") == std::string::npos && table["rrfs-ops"].origin == "built in");

    // {@name} places
    CHECK(Servers::nameIn("{@rrfs-ops}/rrfs.{date}") == "rrfs-ops" && Servers::nameIn("https://x/{@a}").empty() && Servers::nameIn("{@Bad_Name}/x").empty() && Servers::nameIn("{date}").empty());
    const auto places = Servers::places("{@rrfs-ops}/rrfs.{date}/f.grib2");
    CHECK(places.size() == 2 && places[0] == "https://noaa-rrfs-ops-pds.s3.amazonaws.com/rrfs.{date}/f.grib2" && places[1] == "https://noaa-rrfs-ops-pds.s3.us-east-1.amazonaws.com/rrfs.{date}/f.grib2");
    CHECK(Servers::places("https://plain.example.com/x").size() == 1 && Servers::places("{@no-such}/x").empty());

    // a model's urls through the table: the first place is what the program read before, the others are the fallbacks
    const auto * rrfs = GfsModels::find("RRFS");
    CHECK(rrfs != nullptr);
    if (rrfs != nullptr) {
        const auto source = rrfs->source("");
        GfsData::Run run{"20261110", "12"};
        const auto first = source.fileUrl(run, 3, "rrfs/prslev");
        CHECK(first.rfind("https://noaa-rrfs-ops-pds.s3.amazonaws.com/", 0) == 0);
        CHECK(source.candidates && source.candidates(run, 3, "rrfs/prslev").size() == 2);
        Utility::writePref("SERVER_rrfs-ops", "https://mirror.example.org/rrfs");   // a flip with no release: the settings (or the project's file) name another base
        Servers::reload();
        CHECK(source.fileUrl(run, 3, "rrfs/prslev").rfind("https://mirror.example.org/rrfs/", 0) == 0);
        CHECK(source.candidates(run, 3, "rrfs/prslev").size() == 3);
    }
    // a model file may name a server and a path, not a host of its own after {@
    std::printf(failures == 0 ? "all server table tests passed\n" : "%d failures\n", failures);
    return failures == 0 ? 0 : 1;
}
