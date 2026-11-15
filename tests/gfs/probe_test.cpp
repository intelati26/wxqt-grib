// Test of the hover read-out that render() fills in (Options::probe): the fields listed for a chart in the order they are read (the fill, the fills ticked on, the lines, the numbers at points, the
// values only in the read-out), the place of the map in the picture, the winds, the range of the fill, and what a point of the picture reads, in the user's units.
#include <QGuiApplication>
#include <QSettings>
#include "util/MyApplication.h"
#include <cmath>
#include <cstdio>
#include <functional>
#include "gfs/GfsChart.h"

using GfsChart::Product;
using GfsChart::Ramp;

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL line %d: %s\n", __LINE__, #c); failures++; } } while (0)

static GfsGrid::Grid made(const std::function<double(double lon, double lat)>& f) {
    GfsGrid::Grid g;
    g.step = 0.5;
    g.lon0 = -142.0;
    g.lat0 = 62.0;
    g.columns = 189;
    g.rows = 89;
    g.values.resize(static_cast<size_t>(g.columns) * static_cast<size_t>(g.rows));
    for (int r = 0; r < g.rows; r++) {
        for (int c = 0; c < g.columns; c++) {
            g.values[static_cast<size_t>(r) * static_cast<size_t>(g.columns) + static_cast<size_t>(c)] = static_cast<float>(f(g.lon0 + c * g.step, g.lat0 - r * g.step));
        }
    }
    return g;
}

int main(int argc, char ** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app{argc, argv};
    MyApplication::preferences = new QSettings{"/tmp/wxqt_probe_test_prefs.ini", QSettings::IniFormat};
    MyApplication::preferences->clear();
    const double pi = 3.14159265358979;
    GfsChart::Grids grids;
    grids["t"] = made([] (double, double) { return 20.0; });   // a plain 20 C everywhere: what a point reads is known
    grids["h"] = made([pi] (double lon, double lat) { return 5600.0 + 90.0 * std::sin(lon * pi / 25.0) * std::cos(lat * pi / 20.0); });
    grids["p"] = made([pi] (double lon, double lat) { return 101400.0 + 1400.0 * std::sin(lon * pi / 30.0) * std::cos(lat * pi / 14.0); });
    grids["u"] = made([] (double, double) { return 10.0; });
    grids["v"] = made([] (double, double) { return 0.0; });
    grids["o_fill|Dewpoint (C)"] = made([] (double, double) { return 12.0; });
    grids["w"] = made([] (double, double) { return 25.0; });
    grids["n"] = made([] (double, double) { return 1.0; });
    Product product;
    product.source = "TEST";
    product.id = "probe";
    product.label = "probe";
    product.fill = [] (const GfsChart::Grids& g) { return g.at("t"); };
    product.ramp.stops = {{-10, QColor{70, 130, 220}}, {40, QColor{190, 40, 40}}};
    product.fillTitle = "Test fill";
    product.quantity = GfsChart::Quantity::Temperature;
    product.overlays.push_back({"o_fill|Dewpoint (C)", Ramp{{{0, QColor{150, 110, 60, 150}}, {25, QColor{80, 60, 160, 150}}}}});
    product.overlays.push_back({"snow", Ramp{{{0.1, QColor{170, 200, 255}}, {10, QColor{40, 60, 190}}}}});   // (a type fill: not in the read-out)
    grids["snow"] = made([] (double, double) { return 1.0; });
    GfsChart::ContourSet heights, pressure;
    heights.key = "h";
    heights.scale = 0.1;
    heights.interval = 6;
    heights.title = "Height (dam)";
    pressure.key = "p";
    pressure.scale = 0.01;
    pressure.interval = 4;   // (no title: the key names it)
    product.contours = {heights, pressure};
    product.barbU = "u";
    product.barbV = "v";
    product.pointValues = {{"w", QColor{30, 30, 160}, "Median"}};
    product.hoverValues = {{"n", "Limited by", {"Dewpoint", "Temperature"}}};
    const GfsChart::Sector * sector = GfsChart::sector("CONUS");
    CHECK(sector != nullptr);
    GfsData::Run run;
    run.date = "20261108";
    run.cycle = "12";
    for (const bool fahrenheit : {false, true}) {
        GfsChart::Probe probe;
        GfsChart::Options options;
        options.width = 700;
        options.fahrenheit = fahrenheit;
        options.magColors = false;
        options.probe = &probe;
        const auto image = GfsChart::render(product, *sector, grids, run, 12, options);
        CHECK(!image.isNull());
        CHECK(probe.valid());
        // where the map is in the picture, and which part of the earth
        CHECK(probe.west == sector->west && probe.east == sector->east && probe.south == sector->south && probe.north == sector->north);
        CHECK(probe.imageWidth == image.width() && probe.imageHeight == image.height());
        CHECK(probe.areaLeft == 8.0 && probe.areaTop == 54.0 && probe.areaWidth == 684.0 && probe.areaHeight > 200.0);
        CHECK(probe.us == fahrenheit && probe.validUtc.isValid() && probe.validUtc.toString("yyyyMMddHH") == "2026110900");
        // the fields in the order they are read: the fill, the ticked fill, the two lines, the numbers at points, the values only in the read-out
        std::vector<std::string> labels;
        for (const auto& f : probe.fields) {
            labels.push_back(f.label);
        }
        std::printf("  %s: %zu fields:", fahrenheit ? "F" : "C", labels.size());
        for (const auto& l : labels) {
            std::printf(" [%s]", l.c_str());
        }
        std::printf("\n");
        CHECK(labels.size() == 6);
        if (labels.size() == 6) {
            CHECK(labels[0].rfind("Test fill (", 0) == 0);
            CHECK(labels[1].rfind("Dewpoint (", 0) == 0);
            CHECK(labels[2] == "Height (dam)" && labels[3] == "p" && labels[4] == "Median" && labels[5] == "Limited by");
            CHECK(probe.fields[0].convert && probe.fields[0].quantity == GfsChart::Quantity::Temperature);
            CHECK(probe.fields[1].convert && probe.fields[1].quantity == GfsChart::Quantity::Temperature);   // a temperature fill ticked on is in the user's units too
            CHECK(probe.fields[2].scale == 0.1 && probe.fields[3].scale == 0.01);
            CHECK(probe.fields[5].names.size() == 2 && probe.fields[5].names[0] == "Dewpoint");
        }
        // the wind, and the range of the fill (a plain 20 C: the largest and the smallest are that)
        CHECK(!probe.windU.values.empty() && !probe.windV.values.empty());
        CHECK(probe.hasRange && std::abs(probe.fillMax - 20.0) < 1e-3 && std::abs(probe.fillMin - 20.0) < 1e-3);
        // a point of the picture reads the fill in the user's units, and nothing outside the map
        // (a point of the picture is given as a fraction of its width and its height)
        const double x = (probe.areaLeft + probe.areaWidth / 2.0) / probe.imageWidth, y = (probe.areaTop + probe.areaHeight / 2.0) / probe.imageHeight;
        const QString read = probe.read(x, y);
        std::printf("  reads: %s\n", read.toStdString().c_str());
        CHECK(!read.isEmpty());
        CHECK(read.contains(fahrenheit ? "68" : "20"));   // 20 C is 68 F
        CHECK(read.contains("Height") && read.contains("Median"));
        CHECK(read.contains(fahrenheit ? "Median: 77" : "Median: 25"));   // (the numbers at points are of the chart's own quantity: 25 C is 77 F)
        CHECK(probe.read(0.001, 0.001).isEmpty() && probe.read((probe.areaLeft + probe.areaWidth + 5.0) / probe.imageWidth, y).isEmpty());
        // the point of the picture and its longitude and latitude agree both ways
        double lon = 0, lat = 0, fx = 0, fy = 0;
        CHECK(probe.locate(x, y, lon, lat) && lon > sector->west && lon < sector->east && lat > sector->south && lat < sector->north);
        CHECK(probe.place(lon, lat, fx, fy) && std::abs(fx - x) < 0.001 && std::abs(fy - y) < 0.001);
        CHECK(!probe.locate(0.001, 0.001, lon, lat));
    }
    std::printf(failures == 0 ? "OK\n" : "FAILED: %d\n", failures);
    return failures == 0 ? 0 : 1;
}
