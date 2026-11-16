// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

// Charts as data (section 21 of the plan): the example file of docs/examples, the checks that refuse a bad file, and a chart made from a file drawn from made-up grids.
#include <cmath>
#include <cstdio>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QImage>
#include <QSettings>
#include "gfs/GfsChart.h"
#include "gfs/GfsModels.h"
#include "gfs/UserCharts.h"
#include <QThread>
#include "util/MyApplication.h"

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL line %d: %s\n", __LINE__, #c); failures++; } } while (0)

namespace {
    QByteArray doc(const char * chart) {
        return QByteArray{"{\"version\": 1, \"charts\": ["} + chart + "]}";
    }
    GfsGrid::Grid made(double value) {
        GfsGrid::Grid g;
        g.columns = 189;
        g.rows = 89;
        g.lon0 = -140.0;
        g.lat0 = 60.0;
        g.step = 0.5;
        g.values.assign(static_cast<size_t>(g.columns) * static_cast<size_t>(g.rows), 0.0f);
        for (int r = 0; r < g.rows; r++) {
            for (int c = 0; c < g.columns; c++) {
                g.values[static_cast<size_t>(r) * static_cast<size_t>(g.columns) + static_cast<size_t>(c)] = static_cast<float>(value + 12.0 * std::sin(c / 20.0) + 8.0 * std::cos(r / 15.0));
            }
        }
        return g;
    }
}

int main(int argc, char ** argv) {
    QGuiApplication app{argc, argv};
    MyApplication::preferences = new QSettings{"/tmp/wxqt_userchart_test_prefs.ini", QSettings::IniFormat};
    std::vector<GfsChart::Product> out;
    std::string why;

    // the example file of the repository
    QFile example{QString::fromStdString(std::string{__FILE__}.substr(0, std::string{__FILE__}.rfind("tests/")) + "docs/examples/newsurface1.chart.json")};
    CHECK(example.open(QIODevice::ReadOnly));
    CHECK(UserCharts::parse(example.readAll(), out, why));
    CHECK(out.size() == 1);
    if (out.size() == 1) {
        const auto& p = out[0];
        CHECK(p.id == "newsurface1" && p.source == "GFS" && p.pickerGroup == "Your charts" && p.label.rfind("NewSurface1", 0) == 0);
        CHECK(p.wants.size() == 4 && p.wants[0].variable == "TMP" && p.wants[0].level == "2 m above ground" && p.wants[1].variable == "HGT" && p.wants[1].level == "500 mb");
        CHECK(p.contours.size() == 1 && p.contours[0].interval == 6.0 && p.contours[0].scale == 0.1 && p.barbU == "bu" && p.barbV == "bv");
        CHECK(p.quantity == GfsChart::Quantity::Temperature && !p.ramp.stops.empty() && p.legendStep == 5.0);
        // drawn from made-up grids
        GfsChart::Grids grids;
        grids["f0"] = made(10.0);
        grids["c0"] = made(5600.0);
        grids["bu"] = made(8.0);
        grids["bv"] = made(-3.0);
        const auto sector = GfsChart::sector("CONUS");
        CHECK(sector != nullptr);
        if (sector != nullptr) {
            GfsChart::Options options;
            options.noLabels = true;
            options.width = 600;
            options.fahrenheit = false;
            const auto image = GfsChart::render(p, *sector, grids, GfsData::Run{"20261115", "12"}, 12, options);
            CHECK(!image.isNull() && image.width() >= 600);
            int colored = 0;
            for (int y = 0; y < image.height(); y += 4) {
                for (int x = 0; x < image.width(); x += 4) {
                    const QColor c = image.pixelColor(x, y);
                    colored += (c.red() != c.green() || c.green() != c.blue()) ? 1 : 0;
                }
            }
            CHECK(colored > 1000);   // the fill is there
        }
    }
    // other fills: the speed of two fields, a difference, a scale and an offset; lines only
    out.clear();
    CHECK(UserCharts::parse(doc("{\"id\": \"s\", \"model\": \"GFS\", \"fill\": {\"speed\": [\"UGRD@850 mb\", \"VGRD@850 mb\"], \"ramp\": \"wind\", \"scale\": 1.94384}}"), out, why) && out.size() == 1);
    out.clear();
    CHECK(UserCharts::parse(doc("{\"id\": \"d\", \"model\": \"GFS\", \"fill\": {\"diff\": [\"TMP@850 mb\", \"TMP@500 mb\"], \"ramp\": \"temperature\", \"offset\": 2}}"), out, why) && out[0].wants.size() == 2);
    out.clear();
    CHECK(UserCharts::parse(doc("{\"id\": \"l\", \"model\": \"GFS\", \"contours\": [{\"field\": \"PRMSL@mean sea level\", \"scale\": 0.01, \"interval\": 4, \"highs_lows\": true}]}"), out, why) && out[0].linesOnly && out[0].contours[0].highsAndLows);
    // what is refused, as a whole file
    out.clear();
    CHECK(!UserCharts::parse(doc("{\"id\": \"a\", \"model\": \"NOT-A-MODEL\", \"fill\": {\"field\": \"TMP@2 m\"}}"), out, why) && why.find("not one of the program's") != std::string::npos && out.empty());
    CHECK(!UserCharts::parse(doc("{\"id\": \"a b\", \"model\": \"GFS\", \"fill\": {\"field\": \"TMP@2 m\"}}"), out, why) && why.find("letters") != std::string::npos);
    CHECK(!UserCharts::parse(doc("{\"id\": \"a\", \"model\": \"GFS\", \"fill\": {\"field\": \"TMP\"}}"), out, why) && why.find("VARIABLE@level") != std::string::npos);
    CHECK(!UserCharts::parse(doc("{\"id\": \"a\", \"model\": \"GFS\", \"fill\": {\"field\": \"TMP@2 m\", \"ramp\": \"no-such\"}}"), out, why) && why.find("ramp") != std::string::npos);
    CHECK(!UserCharts::parse(doc("{\"id\": \"a\", \"model\": \"GFS\"}"), out, why));
    CHECK(!UserCharts::parse(doc("{\"id\": \"a\", \"model\": \"GFS\", \"contours\": [{\"field\": \"HGT@500 mb\", \"interval\": 0}]}"), out, why) && why.find("interval") != std::string::npos);
    CHECK(!UserCharts::parse(doc("{\"id\": \"a\", \"model\": \"GFS\", \"fill\": {\"field\": \"TMP@2 m\\\"x\"}}"), out, why) && why.find("not allowed") != std::string::npos);
    CHECK(!UserCharts::parse(doc("{\"id\": \"a\", \"model\": \"GFS\", \"fill\": {\"field\": \"TMP@2 m\"}}, {\"id\": \"a\", \"model\": \"GFS\", \"fill\": {\"field\": \"TMP@2 m\"}}"), out, why) && why.find("twice") != std::string::npos);
    CHECK(!UserCharts::parse(QByteArray{"not json"}, out, why) && !UserCharts::parse(QByteArray{"{\"version\": 2, \"charts\": []}"}, out, why) && !UserCharts::parse(QByteArray(200000, ' '), out, why));
    // one bad chart: none of the file is taken
    out.clear();
    CHECK(!UserCharts::parse(doc("{\"id\": \"good\", \"model\": \"GFS\", \"fill\": {\"field\": \"TMP@2 m\"}}, {\"id\": \"bad\", \"model\": \"GFS\"}"), out, why) && out.empty());
    CHECK(!UserCharts::rampNames().empty());

    // the registry reads the folder again when its files change, without a restart; a pointer from the generation before stays good
    {
        const QString folder = "/tmp/wxqt_userchart_reload_test";
        QDir{folder}.removeRecursively();
        QDir{}.mkpath(folder);
        GfsModels::setUserFolder(folder.toStdString());
        CHECK(GfsChart::product("reload1", "GFS") == nullptr);
        const auto first = GfsChart::products().size();
        const auto write = [&] (const QString& label) {
            QFile f{folder + "/mine.chart.json"};
            CHECK(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
            f.write(QByteArray{"{\"version\": 1, \"charts\": [{\"id\": \"reload1\", \"model\": \"GFS\", \"label\": \""} + label.toUtf8() + "\", \"fill\": {\"field\": \"TMP@2 m above ground\"}}]}");
        };
        CHECK(!GfsChart::refreshUserCharts());   // (nothing changed since it was read: not read again)
        write("first label");
        CHECK(GfsChart::refreshUserCharts() && GfsChart::products().size() == first + 1);
        const auto * old = GfsChart::product("reload1", "GFS");
        CHECK(old != nullptr && old->label == "first label");
        CHECK(!GfsChart::refreshUserCharts());   // (read, and not changed since)
        QThread::msleep(20);
        write("second label, longer");
        CHECK(GfsChart::refreshUserCharts() && GfsChart::product("reload1", "GFS")->label == "second label, longer");
        CHECK(old->label == "first label");   // (the old generation is kept: a screen holding the pointer is not left with nothing)
        QFile::remove(folder + "/mine.chart.json");
        CHECK(GfsChart::refreshUserCharts() && GfsChart::product("reload1", "GFS") == nullptr && GfsChart::products().size() == first);
        // a file that is not valid: its charts are left out and the problem is kept
        QFile bad{folder + "/bad.chart.json"};
        CHECK(bad.open(QIODevice::WriteOnly));
        bad.write("not json");
        bad.close();
        CHECK(GfsChart::refreshUserCharts() && GfsChart::products().size() == first && !UserCharts::problems().empty());
        QDir{folder}.removeRecursively();
    }
    std::printf(failures == 0 ? "all chart file tests passed\n" : "%d failures\n", failures);
    return failures == 0 ? 0 : 1;
}
