// Golden-image test of the chart renderer (GfsChart::render): charts built by hand from made-up grids (no network, no catalogue), drawn with Options::noLabels so that no text is in them, and
// compared with the pictures saved in tests/gfs/golden. The renderer is a very large function that is to be taken apart: a change that moves one pixel of the colors, the lines or the barbs
// fails here. GOLDEN_UPDATE=1 writes the pictures again (look at them before keeping them); a missing golden is a failure.
#include <QGuiApplication>
#include <QImage>
#include <QSettings>
#include "util/MyApplication.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include "gfs/GfsChart.h"

using GfsChart::Product;
using GfsChart::Ramp;

namespace {

int failures = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL line %d: %s\n", __LINE__, #c); failures++; } } while (0)

// a regional grid over 18 to 62 N and 142 to 48 W, half a degree: row 0 is the northernmost
GfsGrid::Grid made(const std::function<double(double lon, double lat)>& f) {
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

GfsChart::Grids fields() {
    GfsChart::Grids g;
    const double pi = 3.14159265358979;
    g["t"] = made([pi] (double lon, double lat) { return 32.0 - 0.85 * (lat - 18.0) + 6.0 * std::sin(lon * pi / 40.0) * std::cos(lat * pi / 30.0); });             // temperature, C
    g["h"] = made([pi] (double lon, double lat) { return 5600.0 + 4.0 * (lat > 40 ? -(lat - 40) * 20.0 : (40 - lat) * 5.0) + 90.0 * std::sin(lon * pi / 25.0) * std::cos(lat * pi / 20.0); });   // height, m
    g["p"] = made([pi] (double lon, double lat) { return 1014.0 + 14.0 * std::sin(lon * pi / 30.0 + 0.7) * std::cos(lat * pi / 14.0) + 6.0 * std::sin(lat * pi / 9.0); });   // pressure, mb
    g["u"] = made([pi] (double lon, double lat) { return 8.0 + 22.0 * std::exp(-std::pow((lat - 40.0) / 9.0, 2.0)) + 4.0 * std::sin(lon * pi / 18.0); });              // m/s
    g["v"] = made([pi] (double lon, double lat) { return 12.0 * std::sin(lon * pi / 22.0) * std::exp(-std::pow((lat - 38.0) / 14.0, 2.0)); });
    g["u2"] = made([pi] (double lon, double lat) { return -6.0 + 10.0 * std::cos(lat * pi / 25.0); });
    g["v2"] = made([pi] (double lon, double lat) { return 9.0 * std::sin(lon * pi / 30.0) * std::cos(lat * pi / 40.0); });
    g["r"] = made([pi] (double lon, double lat) { return std::max(0.0, 12.0 * std::sin(lon * pi / 17.0) * std::sin(lat * pi / 11.0) - 1.5); });                       // an amount
    g["snow"] = made([pi] (double lon, double lat) { return lat > 43.0 ? std::max(0.0, 10.0 * std::sin(lon * pi / 21.0) * std::sin(lat * pi / 8.0)) : -1.0; });          // snow where it is cold
    g["mix"] = made([pi] (double lon, double lat) { return lat > 37.0 && lat <= 43.0 ? std::max(0.0, 9.0 * std::cos(lon * pi / 19.0) * std::sin(lat * pi / 6.0)) : -1.0; });
    g["w"] = made([pi] (double lon, double lat) { return 40.0 + 45.0 * std::sin(lon * pi / 33.0) * std::cos(lat * pi / 28.0); });                                     // a percentage
    g["o_fill|Dewpoint"] = made([pi] (double lon, double lat) { return 22.0 - 0.7 * std::abs(lat - 32.0) + 4.0 * std::sin(lon * pi / 28.0); });
    return g;
}

Ramp rampOf(std::initializer_list<std::pair<double, QColor>> stops, bool banded = false) {
    Ramp r;
    r.stops = stops;
    r.banded = banded;
    return r;
}

Ramp temperatureRamp() {
    return rampOf({{-10, QColor{90, 70, 170}}, {0, QColor{70, 130, 220}}, {10, QColor{110, 200, 190}}, {20, QColor{240, 230, 130}}, {30, QColor{240, 150, 60}}, {40, QColor{190, 40, 40}}});
}

Ramp translucent(Ramp r, int alpha, double clearBelow) {
    for (auto& stop : r.stops) {
        stop.second.setAlpha(stop.first < clearBelow ? 0 : alpha);
    }
    return r;
}

GfsChart::ContourSet lines(const std::string& key, double scale, double interval, const QColor& color, double base = 0.0) {
    GfsChart::ContourSet set;
    set.key = key;
    set.scale = scale;
    set.interval = interval;
    set.base = base;
    set.color = color;
    return set;
}

Product basic(const std::string& id, const Ramp& ramp, const std::string& fillKey) {
    Product p;
    p.source = "TEST";
    p.id = id;
    p.label = id;
    p.fill = [fillKey] (const GfsChart::Grids& g) { return g.at(fillKey); };
    p.ramp = ramp;
    p.fillTitle = "Test fill";
    return p;
}

struct Case {
    std::string name;
    Product product;
    std::string sector{"CONUS"};
    GfsChart::Options options;
};

std::vector<Case> cases() {
    std::vector<Case> out;
    GfsChart::Options base;
    base.noLabels = true;
    base.width = 700;
    base.fahrenheit = false;
    base.magColors = false;
    {   // a smooth fill and nothing else
        out.push_back({"fill", basic("fill", temperatureRamp(), "t"), "CONUS", base});
    }
    {   // a fill, height lines (a heavy one every fifth), the highs and lows of the pressure, the chart's own barbs
        auto p = basic("lines_barbs", temperatureRamp(), "t");
        auto h = lines("h", 0.1, 6.0, QColor{30, 30, 30});
        auto pr = lines("p", 1.0, 4.0, QColor{150, 30, 30});
        pr.highsAndLows = true;
        p.contours = {h, pr};
        p.barbU = "u";
        p.barbV = "v";
        out.push_back({"lines_barbs", p, "CONUS", base});
    }
    {   // flat bands, lines that change color at a value and are dashed (a thickness line), uniform weight
        auto p = basic("bands_split", rampOf({{0, QColor{200, 230, 255}}, {20, QColor{120, 180, 240}}, {40, QColor{60, 120, 200}}, {60, QColor{250, 220, 90}}, {80, QColor{230, 90, 60}}}, true), "w");
        auto th = lines("h", 0.1, 3.0, QColor{190, 50, 40});
        th.colorBelow = QColor{40, 90, 190};
        th.split = 558.0;
        th.dashed = true;
        th.uniform = true;
        th.width = 1.3;
        p.contours = {th};
        out.push_back({"bands_split", p, "CONUS", base});
    }
    {   // lines only (the members' contours): no fill, no color bar, nothing labelled
        Product p;
        p.source = "TEST";
        p.id = "lines_only";
        p.label = "lines_only";
        p.linesOnly = true;
        auto a = lines("h", 0.1, 6.0, QColor{200, 40, 40});
        a.labelled = false;
        auto b = lines("p", 1.0, 4.0, QColor{40, 90, 200});
        b.labelled = false;
        b.dashed = true;
        p.contours = {a, b};
        out.push_back({"lines_only", p, "CONUS", base});
    }
    {   // the winds of more levels stacked on the chart's own, each in a color
        auto p = basic("barb_sets", rampOf({{0, QColor{255, 255, 255}}, {20, QColor{170, 210, 240}}, {40, QColor{90, 150, 220}}, {60, QColor{130, 100, 200}}, {80, QColor{210, 80, 150}}}), "w");
        p.barbU = "u";
        p.barbV = "v";
        p.barbSets = {{"u2", "v2", QColor{200, 40, 40}, "850mb wind", "red"}, {"v", "u", QColor{30, 140, 60}, "700mb wind", "green"}};
        out.push_back({"barb_sets", p, "CONUS", base});
    }
    {   // a fill ticked over the chart's own: translucent, clear under a value
        auto p = basic("overlay_fill", temperatureRamp(), "t");
        p.overlays.push_back({"o_fill|Dewpoint", translucent(rampOf({{0, QColor{150, 110, 60}}, {10, QColor{110, 190, 120}}, {20, QColor{60, 150, 200}}, {25, QColor{80, 60, 160}}}), 150, 12.0)});
        p.contours = {lines("p", 1.0, 4.0, QColor{30, 30, 30})};
        out.push_back({"overlay_fill", p, "CONUS", base});
    }
    {   // rain with the snow and the mixed precipitation painted over it where they have a value
        auto p = basic("precip_type", rampOf({{0.1, QColor{140, 230, 140}}, {2, QColor{60, 190, 80}}, {6, QColor{20, 130, 50}}, {12, QColor{10, 80, 40}}}), "r");
        p.overlays.push_back({"snow", rampOf({{0.1, QColor{170, 200, 255}}, {3, QColor{90, 130, 230}}, {10, QColor{40, 60, 190}}})});
        p.overlays.push_back({"mix", rampOf({{0.1, QColor{255, 190, 230}}, {5, QColor{220, 90, 180}}, {9, QColor{160, 30, 120}}})});
        p.contours = {lines("p", 1.0, 4.0, QColor{30, 30, 30}), lines("h", 0.1, 6.0, QColor{120, 60, 30})};
        out.push_back({"precip_type", p, "CONUS", base});
    }
    {   // streamlines of a wind over a fill
        auto p = basic("streamlines", rampOf({{0, QColor{255, 255, 255}}, {15, QColor{200, 190, 120}}, {30, QColor{210, 120, 70}}, {45, QColor{170, 50, 50}}}), "w");
        p.streamU = "u";
        p.streamV = "v";
        out.push_back({"streamlines", p, "CONUS", base});
    }
    {   // a local area with the frame marks and a marker
        auto p = basic("local_ticks", temperatureRamp(), "t");
        p.contours = {lines("h", 0.1, 3.0, QColor{30, 30, 30})};
        p.barbU = "u";
        p.barbV = "v";
        auto o = base;
        o.ticks = true;
        o.hasMarker = true;
        o.markerLat = 35.5;
        o.markerLon = -97.5;
        o.markerName = "Place";
        out.push_back({"local_ticks", p, GfsChart::localId(35.5, -97.5, 900.0, "Place"), o});
    }
    {   // a picture twice as large in each direction, at the same design
        auto p = basic("scaled", temperatureRamp(), "t");
        p.contours = {lines("h", 0.1, 6.0, QColor{30, 30, 30})};
        p.barbU = "u";
        p.barbV = "v";
        auto o = base;
        o.width = 480;
        o.scale = 2.0;
        out.push_back({"scaled", p, "CONUS", o});
    }
    return out;
}

// how much two pictures differ: the largest difference of any color channel, and the number of pixels that differ by more than a few levels
void compare(const QImage& a, const QImage& b, int& largest, long long& differing) {
    largest = 0;
    differing = 0;
    for (int y = 0; y < a.height(); y++) {
        const QRgb * pa = reinterpret_cast<const QRgb *>(a.constScanLine(y));
        const QRgb * pb = reinterpret_cast<const QRgb *>(b.constScanLine(y));
        for (int x = 0; x < a.width(); x++) {
            const int d = std::max({std::abs(qRed(pa[x]) - qRed(pb[x])), std::abs(qGreen(pa[x]) - qGreen(pb[x])), std::abs(qBlue(pa[x]) - qBlue(pb[x])), std::abs(qAlpha(pa[x]) - qAlpha(pb[x]))});
            largest = std::max(largest, d);
            differing += d > 6 ? 1 : 0;
        }
    }
}

}  // namespace

int main(int argc, char ** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app{argc, argv};
    MyApplication::preferences = new QSettings{"/tmp/wxqt_golden_prefs.ini", QSettings::IniFormat};
    MyApplication::preferences->clear();
    const bool update = std::getenv("GOLDEN_UPDATE") != nullptr;
    const std::string folder = std::getenv("GOLDEN_DIR") != nullptr ? std::getenv("GOLDEN_DIR") : "tests/gfs/golden";
    const auto grids = fields();
    GfsData::Run run;
    run.date = "20261108";
    run.cycle = "12";
    int shown = 0;
    for (auto& c : cases()) {
        const GfsChart::Sector * sector = GfsChart::sector(c.sector);
        GfsChart::Sector local;
        if (sector == nullptr) {   // a local area (the id says where)
            double lat, lon, km;
            std::string name;
            CHECK(GfsChart::parseLocalId(c.sector, lat, lon, km, name));
            local = GfsChart::localSector(c.sector, -142.0, 18.0, -48.0, 62.0);
            sector = &local;
        }
        const QImage image = GfsChart::render(c.product, *sector, grids, run, 12, c.options).convertToFormat(QImage::Format_ARGB32);
        CHECK(!image.isNull() && image.width() > 100);
        const auto path = QString::fromStdString(folder + "/" + c.name + ".png");
        if (update) {
            CHECK(image.save(path));
            std::printf("  written %s (%dx%d)\n", path.toStdString().c_str(), image.width(), image.height());
            continue;
        }
        QImage golden;
        if (!golden.load(path)) {
            std::printf("FAIL: no golden picture %s (GOLDEN_UPDATE=1 makes it)\n", path.toStdString().c_str());
            failures++;
            continue;
        }
        golden = golden.convertToFormat(QImage::Format_ARGB32);
        if (golden.size() != image.size()) {
            std::printf("FAIL: %s is %dx%d, the golden %dx%d\n", c.name.c_str(), image.width(), image.height(), golden.width(), golden.height());
            failures++;
            continue;
        }
        int largest = 0;
        long long differing = 0;
        compare(image, golden, largest, differing);
        const bool same = differing <= static_cast<long long>(image.width()) * image.height() / 5000;   // up to 0.02 per cent of the pixels may differ (anti-aliasing)
        std::printf("  %-14s %dx%d  largest difference %3d, pixels that differ %lld%s\n", c.name.c_str(), image.width(), image.height(), largest, differing, same ? "" : "   <-- DIFFERENT");
        if (!same) {
            failures++;
            image.save(QString::fromStdString("/tmp/golden_now_" + c.name + ".png"));
            shown++;
        }
    }
    if (shown > 0) {
        std::printf("the pictures that differ are in /tmp/golden_now_<name>.png\n");
    }
    std::printf(failures == 0 ? "OK\n" : "FAILED: %d\n", failures);
    return failures == 0 ? 0 : 1;
}
