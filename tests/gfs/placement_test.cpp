// Test of where the text of a chart is put (GfsChart::render with Options::placedOut, which hands over every box reserved for text): on a chart with pressure highs and lows, height lines and numbers
// at points, no box is over another (a number on a line, a high or low, a number at a point), and each is inside the frame by the margin of its kind. A number's box is 8 pixels a character and 18 high,
// the letter of a high or low and its value 28 by 32.
#include <QGuiApplication>
#include <QSettings>
#include "util/MyApplication.h"
#include <cmath>
#include <cstdio>
#include <functional>
#include "gfs/GfsChart.h"

using GfsChart::Product;

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

static double mercatorY(double lat) {
    const double pi = 3.14159265358979;
    return std::log(std::tan(pi / 4.0 + std::clamp(lat, -82.0, 82.0) * pi / 180.0 / 2.0));
}

int main(int argc, char ** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app{argc, argv};
    MyApplication::preferences = new QSettings{"/tmp/wxqt_placement_test_prefs.ini", QSettings::IniFormat};
    MyApplication::preferences->clear();
    const double pi = 3.14159265358979;
    GfsChart::Grids grids;
    // a pressure with lows and highs of different sizes, and a height field whose lines run across them
    grids["p"] = made([pi] (double lon, double lat) { return 101400.0 + 1400.0 * std::sin(lon * pi / 22.0 + 0.7) * std::cos(lat * pi / 12.0) + 600.0 * std::sin(lat * pi / 9.0); });
    grids["z"] = made([pi] (double lon, double lat) { return 5600.0 + 4.0 * (lat > 40 ? -(lat - 40) * 20.0 : (40 - lat) * 5.0) + 90.0 * std::sin(lon * pi / 25.0) * std::cos(lat * pi / 20.0); });
    grids["t"] = made([pi] (double lon, double lat) { return 15.0 + 12.0 * std::sin(lon * pi / 31.0) * std::cos(lat * pi / 27.0); });
    grids["w"] = made([pi] (double lon, double lat) { return 40.0 + 30.0 * std::sin(lon * pi / 17.0) * std::sin(lat * pi / 13.0); });
    int boxesChecked = 0, pairsChecked = 0;
    for (const int width : {700, 1000, 480}) {
        for (const int variant : {0, 1, 2}) {   // lines and highs and lows; with numbers at points; and with twice as many lines under the points
            const bool points = variant > 0;
            Product product;
            product.source = "TEST";
            product.id = "placement";
            product.label = "placement";
            product.fill = [] (const GfsChart::Grids& g) { return g.at("z"); };
            product.ramp.stops = {{5000, QColor{200, 220, 255}}, {5800, QColor{255, 240, 200}}};
            GfsChart::ContourSet pressure, heights;
            pressure.key = "p";
            pressure.scale = 0.01;
            pressure.interval = variant == 2 ? 1 : 4;
            pressure.highsAndLows = true;
            heights.key = "z";
            heights.scale = 0.1;
            heights.interval = variant == 2 ? 2 : 6;
            product.contours = {pressure, heights};
            if (points) {   // numbers at points over the same chart: they keep clear of the numbers on the lines and of the highs and lows
                product.pointValues = {{"t", QColor{20, 20, 20}, "Temperature"}, {"w", QColor{30, 30, 160}, "Percent"}};
                product.quantity = GfsChart::Quantity::Temperature;
            }
            std::vector<GfsChart::Options::PlacedBox> placed;
            GfsChart::Probe probe;
            GfsChart::Options options;
            options.width = width;
            options.placedOut = &placed;
            options.probe = &probe;
            options.magColors = false;
            GfsData::Run run;
            run.date = "20261108";
            run.cycle = "12";
            const GfsChart::Sector * sector = GfsChart::sector("CONUS");
            CHECK(sector != nullptr);
            const auto image = GfsChart::render(product, *sector, grids, run, 12, options);
            CHECK(!image.isNull());
            const QRectF area{probe.areaLeft, probe.areaTop, probe.areaWidth, probe.areaHeight};   // (the picture is at scale 1)
            int numbers = 0, extremes = 0, pointBoxes = 0;
            for (const auto& a : placed) {
                numbers += a.kind == "number";
                extremes += a.kind == "extreme";
                pointBoxes += a.kind == "point";
                boxesChecked++;
                // inside the frame by the margin of its kind
                CHECK((a.kind == "number" ? area.adjusted(24, 14, -24, -14) : a.kind == "extreme" ? area.adjusted(20, 20, -20, -20) : area.adjusted(4, 4, -4, -4)).contains(a.box.center()));
            }
            std::printf("  width %d%s: %d numbers on lines, %d highs and lows, %d point boxes\n", width, variant == 2 ? " dense, with points" : points ? " with points" : "", numbers, extremes, pointBoxes);
            CHECK(numbers > 5 && extremes >= 2 && (!points || pointBoxes > 5));
            // no two boxes of the text on the chart over one another (two highs or lows are kept apart by their own radius)
            for (size_t a = 0; a < placed.size(); a++) {
                for (size_t b = a + 1; b < placed.size(); b++) {
                    if (placed[a].kind == "extreme" && placed[b].kind == "extreme") {
                        continue;
                    }
                    pairsChecked++;
                    if (placed[a].box.intersects(placed[b].box)) {
                        std::printf("  %s at (%.0f, %.0f) is over %s at (%.0f, %.0f)\n", placed[a].kind.c_str(), placed[a].box.center().x(), placed[a].box.center().y(), placed[b].kind.c_str(), placed[b].box.center().x(), placed[b].box.center().y());
                    }
                    CHECK(!placed[a].box.intersects(placed[b].box));
                }
            }
        }
    }
    // numbers at points put on the very places of the numbers on the lines (the cities are where the numbers are): the points move off them or are left out, the numbers on the lines stay
    {
        Product product;
        product.source = "TEST";
        product.id = "placement";
        product.label = "placement";
        product.fill = [] (const GfsChart::Grids& g) { return g.at("z"); };
        product.ramp.stops = {{5000, QColor{200, 220, 255}}, {5800, QColor{255, 240, 200}}};
        GfsChart::ContourSet heights;
        heights.key = "z";
        heights.scale = 0.1;
        heights.interval = 6;
        product.contours = {heights};
        const GfsChart::Sector * sector = GfsChart::sector("CONUS");
        GfsData::Run run;
        run.date = "20261108";
        run.cycle = "12";
        std::vector<GfsChart::Options::PlacedBox> first;
        GfsChart::Probe probe;
        GfsChart::Options options;
        options.width = 1000;
        options.placedOut = &first;
        options.probe = &probe;
        options.magColors = false;
        GfsChart::render(product, *sector, grids, run, 12, options);
        const double yTop = mercatorY(sector->north), yBottom = mercatorY(sector->south);
        std::vector<std::pair<float, float>> places;
        for (const auto& a : first) {
            if (a.kind == "number") {
                const double lon = sector->west + (a.box.center().x() - probe.areaLeft) / probe.areaWidth * (sector->east - sector->west);
                const double y = yTop - (a.box.center().y() - probe.areaTop) / probe.areaHeight * (yTop - yBottom);
                places.emplace_back(static_cast<float>(lon), static_cast<float>((2.0 * std::atan(std::exp(y)) - pi / 2.0) * 180.0 / pi));
            }
        }
        CHECK(places.size() > 10);
        product.pointValues = {{"t", QColor{20, 20, 20}, "Temperature"}, {"w", QColor{30, 30, 160}, "Percent"}};
        product.quantity = GfsChart::Quantity::Temperature;
        std::vector<GfsChart::Options::PlacedBox> second;
        options.placedOut = &second;
        options.places = places;
        GfsChart::render(product, *sector, grids, run, 12, options);
        int numbers = 0, pointBoxes = 0;
        for (const auto& a : second) {
            numbers += a.kind == "number";
            pointBoxes += a.kind == "point";
            for (const auto& b : second) {
                if (a.kind == "number" && b.kind == "point") {
                    CHECK(!a.box.intersects(b.box));
                    pairsChecked++;
                }
            }
        }
        std::printf("  on the numbers' places: %zu places, %d numbers on lines, %d point boxes\n", places.size(), numbers, pointBoxes);
        CHECK(numbers > 10 && pointBoxes > 5);
    }
    // the name of a marker (the ring on an area round a saved location) put so that its name would be on a number on a line: the name goes to a side that is free, and its box is clear of every number
    {
        Product product;
        product.source = "TEST";
        product.id = "placement";
        product.label = "placement";
        product.fill = [] (const GfsChart::Grids& g) { return g.at("z"); };
        product.ramp.stops = {{5000, QColor{200, 220, 255}}, {5800, QColor{255, 240, 200}}};
        GfsChart::ContourSet heights;
        heights.key = "z";
        heights.scale = 0.1;
        heights.interval = 6;
        product.contours = {heights};
        const std::string id = GfsChart::localId(40.0, -95.0, 1500.0, "A place with a long name");
        const auto local = GfsChart::localSector(id, -142.0, 18.0, -48.0, 62.0);
        GfsData::Run run;
        run.date = "20261108";
        run.cycle = "12";
        std::vector<GfsChart::Options::PlacedBox> first;
        GfsChart::Probe probe;
        GfsChart::Options options;
        options.width = 900;
        options.placedOut = &first;
        options.probe = &probe;
        options.magColors = false;
        GfsChart::render(product, local, grids, run, 12, options);
        const double yTop = mercatorY(local.north), yBottom = mercatorY(local.south);
        int moved = 0, tried = 0;
        for (const auto& a : first) {
            if (a.kind != "number" || tried >= 8) {
                continue;
            }
            tried++;
            // the ring 45 pixels to the left of the number: the name, to the right of the ring, would be on the number
            const double lon = local.west + (a.box.center().x() - 45.0 - probe.areaLeft) / probe.areaWidth * (local.east - local.west);
            const double y = yTop - (a.box.center().y() - probe.areaTop) / probe.areaHeight * (yTop - yBottom);
            std::vector<GfsChart::Options::PlacedBox> second;
            auto o = options;
            o.placedOut = &second;
            o.hasMarker = true;
            o.markerLon = lon;
            o.markerLat = (2.0 * std::atan(std::exp(y)) - pi / 2.0) * 180.0 / pi;
            o.markerName = "A place with a long name";
            GfsChart::render(product, local, grids, run, 12, o);
            bool haveMarker = false;
            for (const auto& m : second) {
                if (m.kind != "marker") {
                    continue;
                }
                haveMarker = true;
                for (const auto& n : second) {
                    if (n.kind == "number") {
                        pairsChecked++;
                        if (m.box.intersects(n.box)) {
                            std::printf("  the marker name at (%.0f, %.0f) is over a number at (%.0f, %.0f)\n", m.box.center().x(), m.box.center().y(), n.box.center().x(), n.box.center().y());
                        }
                        CHECK(!m.box.intersects(n.box));
                    }
                }
                moved += m.box.center().x() < (probe.areaLeft + (o.markerLon - local.west) / (local.east - local.west) * probe.areaWidth) || m.box.center().y() > a.box.center().y() ? 1 : 0;   // (moved off the first place: left of the ring, or below it)
            }
            CHECK(haveMarker);
        }
        std::printf("  the marker put on %d numbers on lines, its name moved off the first place %d times\n", tried, moved);
        CHECK(tried > 3);
    }
    std::printf("  %d boxes, %d pairs checked\n", boxesChecked, pairsChecked);
    std::printf(failures == 0 ? "OK\n" : "FAILED: %d\n", failures);
    return failures == 0 ? 0 : 1;
}
