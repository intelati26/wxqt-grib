// Test of where the text of a chart is put (GfsChart::render with Options::vectorOut, which hands over the numbers on the lines and the highs and lows as places, not as pixels): on a chart with
// pressure highs and lows and height lines, no number on a line is over a high or a low, no two numbers are over one another, and every one is inside the frame. The rules are in the picture's pixels:
// a number's box is 8 pixels a character and 18 high, the letter of a high or low and its value 28 by 32.
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
    int checkedNumbers = 0, checkedPairs = 0;
    for (const int width : {700, 1000, 480}) {
        Product product;
        product.source = "TEST";
        product.id = "placement";
        product.label = "placement";
        product.fill = [] (const GfsChart::Grids& g) { return g.at("z"); };
        product.ramp.stops = {{5000, QColor{200, 220, 255}}, {5800, QColor{255, 240, 200}}};
        GfsChart::ContourSet pressure, heights;
        pressure.key = "p";
        pressure.scale = 0.01;
        pressure.interval = 4;
        pressure.highsAndLows = true;
        heights.key = "z";
        heights.scale = 0.1;
        heights.interval = 6;
        product.contours = {pressure, heights};
        GfsChart::VectorLines out;
        GfsChart::Probe probe;
        GfsChart::Options options;
        options.width = width;
        options.vectorOut = &out;
        options.probe = &probe;
        options.magColors = false;
        GfsData::Run run;
        run.date = "20261108";
        run.cycle = "12";
        const GfsChart::Sector * sector = GfsChart::sector("CONUS");
        CHECK(sector != nullptr);
        const auto image = GfsChart::render(product, *sector, grids, run, 12, options);
        CHECK(!image.isNull());
        const double left = probe.areaLeft, top = probe.areaTop, w = probe.areaWidth, h = probe.areaHeight;   // (the picture is at scale 1)
        const double yTop = mercatorY(sector->north), yBottom = mercatorY(sector->south);
        const auto pixel = [&] (const GfsChart::VectorLines::Label& l) {
            return QPointF{left + (l.lon - sector->west) / (sector->east - sector->west) * w, top + (yTop - mercatorY(l.lat)) / (yTop - yBottom) * h};
        };
        std::vector<QRectF> extremes, numbers;
        for (const auto& l : out.labels) {
            const auto at = pixel(l);
            if (l.text == "H" || l.text == "L") {
                extremes.push_back(QRectF{at.x() - 14.0, at.y() - 10.0, 28.0, 32.0});   // (the value is 13 pixels under the letter: the box has both)
            } else if (l.text.size() <= 3 && !l.text.isEmpty() && (l.text.at(0).isDigit() || l.text.at(0) == '-')) {
                numbers.push_back(QRectF{at.x() - l.text.size() * 4.0 - 4.0, at.y() - 9.0, l.text.size() * 8.0 + 8.0, 18.0});
            }
        }
        std::printf("  width %d: %zu highs and lows, %zu labels\n", width, extremes.size() / 2 + extremes.size() % 2, numbers.size());
        CHECK(!extremes.empty() && !numbers.empty());
        // the values under the highs and lows are numbers too: they sit inside the boxes of their own letters, not another's. A number on a line must be clear of every letter's box
        for (const auto& n : numbers) {
            checkedNumbers++;
            bool ownValue = false;
            for (const auto& e : extremes) {
                if (e.intersects(n)) {
                    // a value printed under its own letter has its center inside that letter's box, 13 pixels under the letter: those are the extremes' own labels, not numbers on lines
                    ownValue = ownValue || (std::abs(n.center().x() - e.center().x()) < 1.0 && std::abs(n.center().y() - (e.top() + 10.0 + 13.0)) < 1.0);
                    if (!ownValue) {
                        std::printf("  a number at (%.0f, %.0f) is over a high or low at (%.0f, %.0f)\n", n.center().x(), n.center().y(), e.center().x(), e.center().y());
                    }
                    CHECK(ownValue);
                }
            }
        }
        for (size_t a = 0; a < numbers.size(); a++) {
            CHECK((QRectF{left, top, w, h}.contains(numbers[a].center())));
            for (size_t b = a + 1; b < numbers.size(); b++) {
                checkedPairs++;
                CHECK(!numbers[a].intersects(numbers[b]));
            }
        }
    }
    std::printf("  %d numbers and %d pairs checked\n", checkedNumbers, checkedPairs);
    std::printf(failures == 0 ? "OK\n" : "FAILED: %d\n", failures);
    return failures == 0 ? 0 : 1;
}
