// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "gfs/GfsChart.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <QDateTime>
#include <QFontMetricsF>
#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>
#include <QStringList>
#include <set>
#include <tuple>
#include "gfs/GfsClimate.h"
#include "ui/WindBarb.h"

namespace GfsChart {
namespace {
    constexpr double pi = 3.14159265358979323846;
    constexpr double msToKnots = 1.943844;

    double mercatorY(double lat) {
        const double clamped = std::clamp(lat, -82.0, 82.0) * pi / 180.0;
        return std::log(std::tan(pi / 4.0 + clamped / 2.0));
    }
    double mercatorLat(double y) {
        return (2.0 * std::atan(std::exp(y)) - pi / 2.0) * 180.0 / pi;
    }

    // The view: longitude is linear across the width, latitude is Mercator down the height.
    struct View {
        const Sector& sector;
        QRectF area;
        double yTop, yBottom;
        View(const Sector& s, const QRectF& a) : sector{s}, area{a}, yTop{mercatorY(s.north)}, yBottom{mercatorY(s.south)} {}
        QPointF toPixel(double lon, double lat) const {
            double dx = lon - sector.west;
            while (dx < -180.0) {   // only a point a half turn out is moved: a hair beyond the edge stays beyond it
                dx += 360.0;
            }
            while (dx > 540.0) {
                dx -= 360.0;
            }
            return {area.left() + dx / (sector.east - sector.west) * area.width(), area.top() + (yTop - mercatorY(lat)) / (yTop - yBottom) * area.height()};
        }
        double lonAt(double x) const { return sector.west + (x - area.left()) / area.width() * (sector.east - sector.west); }
        double latAt(double y) const { return mercatorLat(yTop - (y - area.top()) / area.height() * (yTop - yBottom)); }
        // the height the area needs so that a degree is as long both ways at the center of the view
        static double heightFor(const Sector& s, double width) {
            return width * (mercatorY(s.north) - mercatorY(s.south)) / ((s.east - s.west) * pi / 180.0);
        }
    };

    Ramp temperature() {
        return {{{-45, QColor{"#4b2d7f"}}, {-30, QColor{"#5b55a8"}}, {-20, QColor{"#3f74b8"}}, {-10, QColor{"#4aa0d4"}}, {0, QColor{"#9ad6e6"}}, {5, QColor{"#bfe4cf"}},
                 {10, QColor{"#dcefb4"}}, {15, QColor{"#f3f0a4"}}, {20, QColor{"#f8d97e"}}, {25, QColor{"#f5ac5c"}}, {30, QColor{"#e57741"}}, {35, QColor{"#c5432e"}}, {40, QColor{"#8e1f2c"}}, {47, QColor{"#5c1024"}}}};
    }
    Ramp windSpeed() {
        return {{{0, QColor{"#f7fbff"}}, {15, QColor{"#e0ecf6"}}, {30, QColor{"#c4dcee"}}, {45, QColor{"#98c5e0"}}, {60, QColor{"#6aa9d2"}}, {80, QColor{"#4687bf"}},
                 {100, QColor{"#7a72b8"}}, {120, QColor{"#b36aa6"}}, {140, QColor{"#d6618a"}}, {170, QColor{"#e8665d"}}, {200, QColor{"#f29a4d"}}}};
    }
    Ramp vorticityRamp() {   // units of 1e-5 per second; the cyclonic side only
        return {{{0, QColor{255, 255, 255, 0}}, {6, QColor{"#fff7d1"}}, {10, QColor{"#fde8a0"}}, {16, QColor{"#fcc967"}}, {24, QColor{"#f59d47"}}, {34, QColor{"#df6a34"}}, {48, QColor{"#a8402a"}}, {70, QColor{"#6b1f2a"}}}};
    }

    Ramp precipitation() {   // millimeters
        return {{{0.0, QColor{217, 239, 208, 0}}, {0.25, QColor{"#d9efd0"}}, {1.0, QColor{"#b6e3b0"}}, {2.5, QColor{"#86d08f"}}, {6.0, QColor{"#4fb98a"}}, {12.0, QColor{"#2fa3a6"}},
                 {25.0, QColor{"#2b7fc0"}}, {38.0, QColor{"#3a5bb8"}}, {50.0, QColor{"#5b43a8"}}, {75.0, QColor{"#8a3aa6"}}, {100.0, QColor{"#b5368f"}}, {150.0, QColor{"#d9435f"}},
                 {200.0, QColor{"#ee7a47"}}, {300.0, QColor{"#f6b24e"}}}};
    }
    Ramp reflectivity() {   // dBZ
        return {{{0, QColor{207, 232, 243, 0}}, {5, QColor{"#cfe8f3"}}, {15, QColor{"#8ccbe0"}}, {20, QColor{"#5bb0c8"}}, {25, QColor{"#55a868"}}, {30, QColor{"#86c04a"}},
                 {35, QColor{"#e3d44a"}}, {40, QColor{"#f0a63a"}}, {45, QColor{"#e0702e"}}, {50, QColor{"#cc3d2c"}}, {55, QColor{"#a8206b"}}, {60, QColor{"#7a2a9a"}}, {70, QColor{"#f2e6f7"}}}};
    }
    Ramp humidity() {   // percent
        return {{{0, QColor{"#7a4a24"}}, {20, QColor{"#b58a52"}}, {40, QColor{"#e3cfa0"}}, {55, QColor{"#f4f0df"}}, {70, QColor{"#c3e3b4"}}, {85, QColor{"#6cc496"}}, {95, QColor{"#2d9aa6"}}, {100, QColor{"#1e5fa8"}}}};
    }
    Ramp precipitableWater() {   // millimeters
        return {{{0, QColor{"#f7f4e8"}}, {13, QColor{"#e3ecc0"}}, {25, QColor{"#b5dd9a"}}, {38, QColor{"#7cc9a0"}}, {50, QColor{"#46aeb8"}}, {63, QColor{"#3a86c4"}}, {75, QColor{"#5a5fb8"}}, {90, QColor{"#8b4aa6"}}}};
    }
    Ramp snowChange() {   // centimeters; transparent where nothing changes
        return {{{-30, QColor{"#8c5a2b"}}, {-10, QColor{"#c99a62"}}, {-1, QColor{"#ecd9b8"}}, {-0.3, QColor{236, 217, 184, 0}}, {0.3, QColor{200, 225, 245, 0}}, {1, QColor{"#c8e1f5"}},
                 {10, QColor{"#6aa9d2"}}, {30, QColor{"#5a5fb8"}}, {60, QColor{"#8b4aa6"}}}};
    }

    Ramp shearRamp() {   // knots: calm to violent, the low shear tropical cyclones like in the pale greens
        return {{{0, QColor{"#f4f9f1"}}, {10, QColor{"#cfe6c8"}}, {20, QColor{"#f0e8a0"}}, {30, QColor{"#f3b86a"}}, {40, QColor{"#e0703e"}}, {60, QColor{"#a22b3a"}}, {80, QColor{"#5c1024"}}}};
    }
    Ramp divergenceRamp() {   // 1e-5 per second: convergence in blues, divergence in warm colors, little of either left clear
        return {{{-30, QColor{"#2b4a9a"}}, {-15, QColor{"#4f86c6"}}, {-6, QColor{"#a9cde8"}}, {-2, QColor{169, 205, 232, 0}}, {2, QColor{246, 216, 154, 0}}, {6, QColor{"#f6d89a"}}, {15, QColor{"#ee9a4a"}}, {30, QColor{"#c0392b"}}}};
    }
    Ramp thetaERamp() {   // kelvin
        return {{{260, QColor{"#4b2d7f"}}, {280, QColor{"#3f74b8"}}, {295, QColor{"#9ad6e6"}}, {305, QColor{"#dcefb4"}}, {315, QColor{"#f3f0a4"}}, {325, QColor{"#f8d97e"}}, {335, QColor{"#f5ac5c"}}, {345, QColor{"#e57741"}}, {355, QColor{"#c5432e"}}, {370, QColor{"#5c1024"}}}};
    }

    Ramp heightAnomaly() {   // decameters: below normal in blues, above in warm colors, within 1.5 clear
        return {{{-30, QColor{"#2b4a9a"}}, {-20, QColor{"#3f74b8"}}, {-10, QColor{"#86b4dc"}}, {-4, QColor{"#d3e5f2"}}, {-1.5, QColor{211, 229, 242, 0}}, {1.5, QColor{246, 224, 184, 0}},
                 {4, QColor{"#f6e0b8"}}, {10, QColor{"#f0b070"}}, {20, QColor{"#d9633a"}}, {30, QColor{"#a02a2a"}}}};
    }
    Ramp pressureAnomaly() {   // millibars
        return {{{-30, QColor{"#2b4a9a"}}, {-16, QColor{"#3f74b8"}}, {-8, QColor{"#86b4dc"}}, {-3, QColor{"#d3e5f2"}}, {-1, QColor{211, 229, 242, 0}}, {1, QColor{246, 224, 184, 0}},
                 {3, QColor{"#f6e0b8"}}, {8, QColor{"#f0b070"}}, {16, QColor{"#d9633a"}}, {30, QColor{"#a02a2a"}}}};
    }

    Ramp dewpoint() {   // degrees C: dry in the browns, humid in greens and blues
        return {{{-30, QColor{"#8c5a2b"}}, {-15, QColor{"#c99a62"}}, {-5, QColor{"#e8d9b0"}}, {0, QColor{"#f1ecd2"}}, {8, QColor{"#d4ead0"}}, {14, QColor{"#a6dbb0"}}, {18, QColor{"#6cc496"}},
                 {22, QColor{"#2fa3a6"}}, {25, QColor{"#2b7fc0"}}, {28, QColor{"#5b43a8"}}, {31, QColor{"#8a3aa6"}}}};
    }
    Ramp lowWind() {   // knots, for the near-surface and 850 mb winds
        return {{{0, QColor{"#f7fbff"}}, {10, QColor{"#e0ecf6"}}, {20, QColor{"#c4dcee"}}, {30, QColor{"#98c5e0"}}, {40, QColor{"#6aa9d2"}}, {50, QColor{"#4687bf"}}, {65, QColor{"#7a72b8"}}, {80, QColor{"#d6618a"}}}};
    }
    Ramp capeRamp() {   // J/kg
        return {{{0, QColor{227, 240, 198, 0}}, {100, QColor{"#e3f0c6"}}, {500, QColor{"#b8dc8a"}}, {1000, QColor{"#f1e27a"}}, {1500, QColor{"#f4bf5a"}}, {2000, QColor{"#ee9144"}}, {3000, QColor{"#dc5a3c"}},
                 {4000, QColor{"#b8303c"}}, {5000, QColor{"#7a2a8a"}}}};
    }
    Ramp snowPrecipitation() {   // millimeters of water
        return {{{0.0, QColor{214, 230, 247, 0}}, {0.25, QColor{"#d6e6f7"}}, {1.0, QColor{"#a9c9ec"}}, {2.5, QColor{"#7ba7e0"}}, {6.0, QColor{"#4f7fd0"}}, {12.0, QColor{"#3a55b8"}}, {25.0, QColor{"#3a2f9a"}}, {50.0, QColor{"#5b2a8a"}}}};
    }
    Ramp mixedPrecipitation() {   // freezing rain and ice pellets
        return {{{0.0, QColor{243, 208, 230, 0}}, {0.25, QColor{"#f3d0e6"}}, {1.0, QColor{"#ea9fcb"}}, {2.5, QColor{"#d96aa8"}}, {6.0, QColor{"#c03f88"}}, {12.0, QColor{"#8f2a73"}}, {25.0, QColor{"#5c1a5a"}}}};
    }

    Ramp cloudCover() {   // percent: clear sky pale, overcast a dark grey-blue
        return {{{0, QColor{"#eef6fb"}}, {20, QColor{"#cfe3f1"}}, {40, QColor{"#a9c3d8"}}, {60, QColor{"#8da1b4"}}, {80, QColor{"#6e7d8c"}}, {100, QColor{"#4c5560"}}}};
    }
    Ramp spreadRamp(double top) {   // the spread of an ensemble, from none (white) up through yellow and orange to a deep purple at `top`
        return {{{0.0, {255, 255, 255}}, {top * 0.1, {255, 247, 188}}, {top * 0.3, {254, 196, 79}}, {top * 0.5, {244, 109, 67}}, {top * 0.75, {200, 30, 80}}, {top, {90, 20, 120}}}};
    }
    Ramp tropicalWind() {   // knots: the Saffir-Simpson steps, tropical depression and storm in the cool colors, hurricanes warm to purple
        return {{{0, QColor{235, 245, 255}}, {20, QColor{150, 200, 240}}, {34, QColor{0, 210, 160}}, {50, QColor{255, 230, 0}}, {64, QColor{255, 170, 0}}, {83, QColor{255, 100, 0}}, {96, QColor{230, 30, 30}},
                 {113, QColor{180, 0, 90}}, {137, QColor{130, 0, 170}}, {160, QColor{255, 255, 255}}}};
    }
    Ramp satelliteIr() {   // brightness temperature in kelvin: warm surfaces dark, cool cloud light grey, then the cold tops in color to the coldest in white
        return {{{183, QColor{255, 255, 255}}, {193, QColor{60, 0, 90}}, {203, QColor{200, 0, 200}}, {213, QColor{255, 0, 0}}, {223, QColor{255, 150, 0}}, {233, QColor{255, 255, 0}}, {243, QColor{0, 230, 230}},
                 {253, QColor{175, 175, 175}}, {273, QColor{95, 95, 95}}, {303, QColor{0, 0, 0}}}};
    }
    Ramp waterVapor() {   // brightness temperature in kelvin: the moist, cold air in white through blue, dry air warm in orange to black
        return {{{185, QColor{255, 255, 255}}, {200, QColor{120, 255, 200}}, {210, QColor{0, 200, 255}}, {220, QColor{30, 100, 255}}, {230, QColor{30, 30, 120}}, {240, QColor{110, 110, 110}},
                 {250, QColor{190, 170, 120}}, {260, QColor{255, 200, 60}}, {270, QColor{255, 120, 0}}, {285, QColor{0, 0, 0}}}};
    }
    Ramp seaSurfaceTemperature() {   // degrees C: fine steps through the range that matters to a storm (26 C is where the warm water starts)
        return {{{10, QColor{40, 60, 160}}, {20, QColor{30, 150, 220}}, {24, QColor{0, 200, 200}}, {26, QColor{0, 200, 120}}, {28, QColor{250, 230, 0}}, {29.5, QColor{255, 140, 0}}, {31, QColor{220, 30, 30}},
                 {33, QColor{130, 0, 60}}}};
    }
    Ramp waveHeight() {   // meters
        return {{{0, QColor{235, 245, 255}}, {1, QColor{150, 200, 240}}, {2, QColor{0, 170, 220}}, {3, QColor{0, 200, 120}}, {4, QColor{250, 230, 0}}, {6, QColor{255, 140, 0}}, {8, QColor{220, 30, 30}},
                 {10, QColor{180, 0, 90}}, {12, QColor{130, 0, 170}}, {15, QColor{255, 255, 255}}}};
    }
    Ramp probability() {   // percent: nothing under 5, then pale green to deep magenta
        return {{{0, QColor{229, 242, 217, 0}}, {5, QColor{229, 242, 217, 0}}, {10, QColor{"#e5f2d9"}}, {20, QColor{"#c4e3a4"}}, {30, QColor{"#93d17f"}}, {40, QColor{"#5cbf8a"}}, {50, QColor{"#31a8a8"}},
                 {60, QColor{"#2f86c4"}}, {70, QColor{"#3a5fb8"}}, {80, QColor{"#5b43a8"}}, {90, QColor{"#8a3aa6"}}, {100, QColor{"#b5368f"}}}};
    }
    Ramp snowfall() {   // centimeters of new snow, the steps at whole and simple inches (0.1, 1, 2, 4, 8, 12, 20, 30)
        return {{{0.0, QColor{214, 230, 247, 0}}, {0.254, QColor{"#d6e6f7"}}, {2.54, QColor{"#a9c9ec"}}, {5.08, QColor{"#7ba7e0"}}, {10.16, QColor{"#4f7fd0"}}, {20.32, QColor{"#3a55b8"}}, {30.48, QColor{"#3a2f9a"}},
                 {50.8, QColor{"#5b2a8a"}}, {76.2, QColor{"#8b4aa6"}}}};
    }

    // equivalent potential temperature (Bolton 1980) in K from temperature in C, relative humidity in percent and the pressure in hPa
    double thetaE(double tC, double rh, double p) {
        const double t = tC + 273.15;
        const double es = 6.112 * std::exp(17.67 * tC / (tC + 243.5));
        const double e = std::max(0.01, rh / 100.0 * es);
        const double td = 243.5 * std::log(e / 6.112) / (17.67 - std::log(e / 6.112)) + 273.15;
        const double tl = 1.0 / (1.0 / (td - 56.0) + std::log(t / td) / 800.0) + 56.0;
        const double r = 0.62197 * e / (p - e);   // kg/kg
        return t * std::pow(1000.0 / (p - e), 0.2854 * (1.0 - 0.28 * r)) * std::exp((3.376 / tl - 0.00254) * r * 1000.0 * (1.0 + 0.81 * r));
    }

    GfsGrid::Grid pick(const Grids& g, const std::string& key) {
        const auto found = g.find(key);
        return found == g.end() ? GfsGrid::Grid{} : found->second;
    }

    GfsData::Want want(const char * key, const char * variable, const char * level) {
        return {key, variable, level, ""};
    }
}

QRgb Ramp::at(double value) const {
    if (stops.empty() || std::isnan(value)) {
        return qRgba(0, 0, 0, 0);
    }
    if (value <= stops.front().first) {
        return stops.front().second.rgba();
    }
    for (size_t i = 1; i < stops.size(); i++) {
        if (value <= stops[i].first) {
            const double t = (value - stops[i - 1].first) / (stops[i].first - stops[i - 1].first);
            const auto a = stops[i - 1].second, b = stops[i].second;
            return qRgba(static_cast<int>(a.red() + (b.red() - a.red()) * t), static_cast<int>(a.green() + (b.green() - a.green()) * t),
                         static_cast<int>(a.blue() + (b.blue() - a.blue()) * t), static_cast<int>(a.alpha() + (b.alpha() - a.alpha()) * t));
        }
    }
    return stops.back().second.rgba();
}

const std::vector<Sector>& sectors() {
    static const std::vector<Sector> list{
        {"CONUS", -127, 22, -65, 51},
        {"NAMER", -170, 5, -50, 72},
        {"SAMER", -95, -58, -30, 17},
        {"AFRICA", -25, -38, 62, 40},
        {"NORTH-PAC", 110, 0, 250, 62},
        {"EAST-PAC", -160, 0, -75, 50},
        {"WEST-ATL", -100, 5, -40, 50},
        {"ATLANTIC", -100, -5, 20, 65},
        {"POLAR", -180, 55, 180, 82},
        {"ALASKA", -175, 50, -125, 72},
        {"EUROPE", -25, 33, 50, 72},
        {"ASIA", 55, 0, 150, 60},
        {"SOUTH-PAC", 140, -60, 290, 5},
        {"ARTIC", -180, 60, 180, 82},
        {"INDIA", 50, -10, 110, 40},
        {"US-SAMOA", -190, -30, -140, 5},
        // the world
        {"GLOBAL", -180, -75, 180, 75},
        {"TROPICS", -180, -38, 180, 38},
        {"NORTHERN-HEMI", -180, 15, 180, 82},
        {"SOUTHERN-HEMI", -180, -82, 180, -15},
        {"AUSTRALIA", 105, -50, 180, -5},
        {"MIDDLE-EAST", 25, 8, 72, 45},
        {"INDIAN-OCEAN", 30, -45, 125, 28},
        {"E-ASIA", 95, 15, 150, 56},
        {"SE-ASIA", 88, -12, 150, 30},
        {"NORTH-ATL", -85, 10, 20, 68},
        {"CARIBBEAN", -92, 6, -56, 29},
        {"GULF-MEXICO", -100, 17, -78, 32},
        {"CENT-AMER", -95, 5, -70, 22},
        {"HAWAII", -165, 15, -150, 26},
        // the United States by region
        {"NORTHEAST", -83, 36, -66, 48},
        {"MID-ATLANTIC", -86, 34, -72, 42},
        {"SOUTHEAST", -93, 24, -74, 38},
        {"GREAT-LAKES", -96, 40, -74, 50},
        {"OHIO-VALLEY", -92, 34, -78, 43},
        {"S-PLAINS", -107, 25, -90, 40},
        {"N-PLAINS", -113, 39, -94, 50},
        {"ROCKIES", -118, 35, -101, 50},
        {"SOUTHWEST", -122, 30, -102, 42},
        {"PACIFIC-NW", -128, 41, -113, 52},
        {"CALIFORNIA", -126, 31, -112, 43},
        {"GULF-COAST", -100, 24, -80, 35}};
    return list;
}

Sector gridSector(const std::string& id, const GfsGrid::Grid& g) {
    int left = g.columns, right = -1, top = g.rows, bottom = -1;
    for (int y = 0; y < g.rows; y++) {
        for (int x = 0; x < g.columns; x++) {
            if (!std::isnan(g.values[static_cast<size_t>(y) * static_cast<size_t>(g.columns) + static_cast<size_t>(x)])) {
                left = std::min(left, x);
                right = std::max(right, x);
                top = std::min(top, y);
                bottom = std::max(bottom, y);
            }
        }
    }
    if (right < 0) {   // nothing valid: the whole grid
        left = 0;
        right = g.columns - 1;
        top = 0;
        bottom = g.rows - 1;
    }
    const double margin = g.step / 2.0;   // half a cell: the data and nothing more (the tilted footprint leaves wedges in the corners)
    return {id, g.lon0 + left * g.step - margin, g.lat0 - bottom * g.step - margin, g.lon0 + right * g.step + margin, g.lat0 - top * g.step + margin};
}

const Sector * sector(const std::string& id) {
    for (const auto& s : sectors()) {
        if (s.id == id) {
            return &s;
        }
    }
    return nullptr;
}

const std::vector<Product>& products() {
    static const std::vector<Product> list = [] {
        std::vector<Product> p;
        const auto speedOf = [] (const char * u, const char * v) {
            return [u, v] (const Grids& g) { return GfsGrid::scaled(GfsGrid::speed(pick(g, u), pick(g, v)), msToKnots); };
        };
        const auto heights = [] (double interval, bool dark = true) {
            ContourSet c;
            c.key = "z";
            c.scale = 0.1;
            c.interval = interval;
            c.title = "Height (dam)";
            c.color = dark ? QColor{30, 30, 30} : QColor{60, 60, 60};
            return c;
        };
        const auto pressure = [] {
            ContourSet c;
            c.key = "p";
            c.scale = 0.01;
            c.interval = 4;
            c.title = "Sea level pressure (mb)";
            c.highsAndLows = true;
            return c;
        };
        const auto vorticityOf = [] (const Grids& g) { return GfsGrid::scaled(GfsGrid::vorticity(GfsGrid::smoothed(pick(g, "u"), 1), GfsGrid::smoothed(pick(g, "v"), 1)), 1e5); };

        // the upper-air charts: wind speed filled, heights contoured, barbs
        const auto upper = [&] (const char * id, const char * label, const char * level, double interval) {
            Product x;
            x.id = id;
            x.label = label;
            x.wants = {want("z", "HGT", level), want("u", "UGRD", level), want("v", "VGRD", level)};
            x.fill = speedOf("u", "v");
            x.ramp = windSpeed();
            x.fillTitle = "Wind speed (kt)";
            x.legendStep = 20;
            x.contours = {heights(interval)};
            x.barbU = "u";
            x.barbV = "v";
            return x;
        };
        p.push_back(upper("200_wnd_ht", "200mb Wind and Height", "200 mb", 12));
        p.push_back(upper("250_wnd_ht", "250mb Wind and Height", "250 mb", 12));
        p.push_back(upper("300_wnd_ht", "300mb Wind and Height", "300 mb", 12));
        p.push_back(upper("500_wnd_ht", "500mb Wind and Height", "500 mb", 6));
        for (const auto& [id, label, level, interval] : {std::tuple{"500_vort_ht", "500mb Vorticity, Wind, and Height", "500 mb", 6.0}, {"850_vort_ht", "850mb Vorticity, Wind and Height", "850 mb", 3.0}}) {
            auto x = upper(id, label, level, interval);
            x.fill = vorticityOf;
            x.ramp = vorticityRamp();
            x.fillTitle = "Relative vorticity (1e-5 /s)";
            x.legendStep = 10;
            p.push_back(x);
        }
        // the lower levels: temperature filled
        for (const auto& [id, label, level, interval] : {std::tuple{"850_temp_ht", "850mb Temperature, Wind and Height", "850 mb", 3.0}, {"925_temp_ht", "925mb Temperature, Wind and Height", "925 mb", 3.0}}) {
            auto x = upper(id, label, level, interval);
            x.wants.push_back(want("t", "TMP", level));
            x.fill = [] (const Grids& g) { return pick(g, "t"); };
            x.ramp = temperature();
            x.fillTitle = "Temperature";
            x.legendStep = 5;
            x.quantity = Quantity::Temperature;
            p.push_back(x);
        }
        {   // humidity
            auto x = upper("500_rh_ht", "500mb Relative Humidity and Height", "500 mb", 6);
            x.wants = {want("z", "HGT", "500 mb"), want("rh", "RH", "500 mb")};
            x.fill = [] (const Grids& g) { return pick(g, "rh"); };
            x.ramp = humidity();
            x.fillTitle = "Relative humidity (%)";
            x.legendStep = 10;
            x.barbU.clear();
            x.barbV.clear();
            p.push_back(x);
            auto y = x;
            y.id = "850_rh_ht";
            y.label = "850mb Relative Humidity and Height";
            y.wants = {want("z", "HGT", "850 mb"), want("rh", "RH", "850 mb")};
            y.contours = {heights(3)};
            p.push_back(y);
            auto w = x;   // 700 mb: with the vertical motion (omega) as lines, the rising air only
            w.id = "700_rh_ht";
            w.label = "700mb Relative Humidity, Height and Omega";
            w.wants = {want("z", "HGT", "700 mb"), want("rh", "RH", "700 mb"), want("o", "VVEL", "700 mb")};
            w.contours = {heights(3)};
            ContourSet omega;
            omega.key = "o";
            omega.scale = 10.0;   // Pa/s -> microbar/s
            omega.interval = 2;
            omega.title = "Omega (ubar/s, rising air)";
            omega.color = QColor{20, 110, 70};
            omega.dashed = true;
            omega.onlyBelowZero = true;
            w.contours.push_back(omega);
            p.push_back(w);
        }
        {   // precipitable water
            auto x = upper("850_pw_ht", "850mb Height, Precipitable Water and Wind", "850 mb", 3);
            x.wants = {want("z", "HGT", "850 mb"), want("u", "UGRD", "850 mb"), want("v", "VGRD", "850 mb"), {"pw", "PWAT", "entire atmosphere (considered as a single layer)", ""}};
            x.fill = [] (const Grids& g) { return pick(g, "pw"); };
            x.ramp = precipitableWater();
            x.fillTitle = "Precipitable water";
            x.quantity = Quantity::Millimeters;
            x.legendStep = 0.5;
            p.push_back(x);
        }
        {   // 850 mb vorticity under the 500 mb heights and the 200 mb wind
            auto x = upper("850vor_500ht_200wd", "850mb Vorticity, 500mb Height, 200mb Wind", "850 mb", 6);
            x.wants = {want("z", "HGT", "500 mb"), want("u", "UGRD", "850 mb"), want("v", "VGRD", "850 mb"), want("u2", "UGRD", "200 mb"), want("v2", "VGRD", "200 mb")};
            x.fill = vorticityOf;
            x.ramp = vorticityRamp();
            x.fillTitle = "850mb relative vorticity (1e-5 /s)";
            x.legendStep = 10;
            x.contours = {heights(6)};
            x.barbU = "u2";
            x.barbV = "v2";
            p.push_back(x);
        }
        // the surface: temperature, 10 m wind, sea level pressure
        {
            Product x;
            x.id = "10m_wnd_2m_temp";
            x.label = "MSLP, 10m wind, 2m temperature";
            x.wants = {want("t", "TMP", "2 m above ground"), want("u", "UGRD", "10 m above ground"), want("v", "VGRD", "10 m above ground"), want("p", "PRMSL", "mean sea level")};
            x.fill = [] (const Grids& g) { return pick(g, "t"); };
            x.ramp = temperature();
            x.fillTitle = "2 m temperature";
            x.legendStep = 5;
            x.quantity = Quantity::Temperature;
            x.contours = {pressure()};
            x.barbU = "u";
            x.barbV = "v";
            p.push_back(x);
        }
        // the tropical set
        {   // deep-layer shear: the 200 mb wind less the 850 mb wind
            Product x;
            x.id = "shear_850_200";
            x.label = "850-200mb Deep-Layer Wind Shear";
            x.wants = {want("u8", "UGRD", "850 mb"), want("v8", "VGRD", "850 mb"), want("u2", "UGRD", "200 mb"), want("v2", "VGRD", "200 mb"), want("p", "PRMSL", "mean sea level")};
            x.derive = [] (Grids& g, const Context&) {
                g["su"] = GfsGrid::difference(g["u2"], g["u8"]);
                g["sv"] = GfsGrid::difference(g["v2"], g["v8"]);
            };
            x.fill = speedOf("su", "sv");
            x.ramp = shearRamp();
            x.fillTitle = "Shear magnitude (kt)";
            x.legendStep = 10;
            x.contours = {pressure()};
            x.contours[0].highsAndLows = false;
            x.barbU = "su";
            x.barbV = "sv";
            p.push_back(x);
        }
        {   // upper-level divergence under the 200 mb wind (as barbs) and heights
            auto x = upper("200_div_wnd", "200mb Divergence, Wind and Height", "200 mb", 12);
            x.fill = [] (const Grids& g) { return GfsGrid::scaled(GfsGrid::divergence(GfsGrid::smoothed(pick(g, "u"), 1), GfsGrid::smoothed(pick(g, "v"), 1)), 1e5); };
            x.ramp = divergenceRamp();
            x.fillTitle = "Divergence (1e-5 /s)";
            x.legendStep = 10;
            p.push_back(x);
        }
        {   // the same as streamlines
            auto x = upper("200_stream_div", "200mb Divergence and Streamlines", "200 mb", 12);
            x.fill = [] (const Grids& g) { return GfsGrid::scaled(GfsGrid::divergence(GfsGrid::smoothed(pick(g, "u"), 1), GfsGrid::smoothed(pick(g, "v"), 1)), 1e5); };
            x.ramp = divergenceRamp();
            x.fillTitle = "Divergence (1e-5 /s)";
            x.legendStep = 10;
            x.contours.clear();
            x.barbU.clear();
            x.barbV.clear();
            x.streamU = "u";
            x.streamV = "v";
            p.push_back(x);
        }
        {   // 850 mb vorticity with streamlines: the look of a tropical wave
            auto x = upper("850_stream_vort", "850mb Vorticity and Streamlines", "850 mb", 3);
            x.fill = vorticityOf;
            x.ramp = vorticityRamp();
            x.fillTitle = "Relative vorticity (1e-5 /s)";
            x.legendStep = 10;
            x.contours.clear();
            x.barbU.clear();
            x.barbV.clear();
            x.streamU = "u";
            x.streamV = "v";
            p.push_back(x);
        }
        {   // the mean wind through the deep layer, which is what steers a tropical cyclone
            Product x;
            x.id = "steering_850_200";
            x.label = "850-200mb Mean Wind (Steering Flow)";
            const char * levels[] = {"850 mb", "700 mb", "500 mb", "300 mb", "200 mb"};
            for (int i = 0; i < 5; i++) {
                x.wants.push_back({"u" + std::to_string(i), "UGRD", levels[i], ""});
                x.wants.push_back({"v" + std::to_string(i), "VGRD", levels[i], ""});
            }
            x.wants.push_back(want("p", "PRMSL", "mean sea level"));
            x.derive = [] (Grids& g, const Context&) {
                // pressure-weighted (trapezoid in pressure) mean of the five levels
                const double pressures[] = {850, 700, 500, 300, 200};
                double weights[5];
                for (int i = 0; i < 5; i++) {
                    const double above = i == 4 ? pressures[i] : (pressures[i] + pressures[i + 1]) / 2.0;
                    const double below = i == 0 ? pressures[i] : (pressures[i] + pressures[i - 1]) / 2.0;
                    weights[i] = below - above;
                }
                for (const char * name : {"u", "v"}) {
                    auto mean = g[std::string{name} + "0"];
                    std::fill(mean.values.begin(), mean.values.end(), 0.0f);
                    for (int i = 0; i < 5; i++) {
                        const auto& level = g[std::string{name} + std::to_string(i)];
                        for (size_t k = 0; k < mean.values.size(); k++) {
                            mean.values[k] += static_cast<float>(level.values[k] * weights[i] / 650.0);
                        }
                    }
                    g[std::string{"m"} + name] = std::move(mean);
                }
            };
            x.fill = speedOf("mu", "mv");
            x.ramp = windSpeed();
            x.fillTitle = "Mean wind speed (kt)";
            x.legendStep = 20;
            x.contours = {pressure()};
            x.contours[0].highsAndLows = false;
            x.barbU = "mu";
            x.barbV = "mv";
            p.push_back(x);
        }
        {   // 850 mb equivalent potential temperature: the moist, warm air of the tropics and the fronts' edges
            auto x = upper("850_thetae_ht", "850mb Equivalent Potential Temperature, Wind and Height", "850 mb", 3);
            x.wants.push_back(want("t", "TMP", "850 mb"));
            x.wants.push_back(want("rh", "RH", "850 mb"));
            x.fill = [] (const Grids& g) {
                auto out = pick(g, "t");
                const auto rh = pick(g, "rh");
                for (size_t i = 0; i < out.values.size(); i++) {
                    out.values[i] = static_cast<float>(thetaE(out.values[i], std::clamp<double>(rh.values[i], 1.0, 100.0), 850.0));
                }
                return out;
            };
            x.ramp = thetaERamp();
            x.fillTitle = "Equivalent potential temperature (K)";
            x.legendStep = 10;
            p.push_back(x);
        }
        // anomalies: the forecast less the 1991-2020 daily mean for the valid day (GfsClimate), where the heights of the level are real
        const auto validDay = [] (const Context& context) {
            auto t = QDateTime::fromString(QString::fromStdString(context.run.id()), "yyyyMMddHH");
            t.setTimeSpec(Qt::UTC);
            t = t.addSecs(context.hour * 3600);
            return GfsClimate::dayIndex(t.date().year(), t.date().month(), t.date().day());
        };
        for (const auto& [id, label, level, interval] : {std::tuple{"500_hgt_anom", "500mb Height and Anomaly", 500, 6.0}, {"700_hgt_anom", "700mb Height and Anomaly", 700, 3.0}}) {
            Product x;
            x.id = id;
            x.label = label;
            const std::string levelText = std::to_string(level) + " mb";
            x.wants = {want("z", "HGT", levelText.c_str()), want("u", "UGRD", levelText.c_str()), want("v", "VGRD", levelText.c_str())};
            if (level == 700) {
                x.wants.push_back(want("ps", "PRES", "surface"));   // where the surface is higher than 700 mb the level is underground
            }
            x.derive = [validDay, level] (Grids& g, const Context& context) {
                GfsGrid::Grid normal;
                std::string error;
                if (!context.climate || !context.climate->at(GfsClimate::height(level), validDay(context), normal, error)) {
                    return;
                }
                auto anom = GfsGrid::scaled(GfsGrid::anomaly(g["z"], normal), 0.1);   // meters -> decameters
                if (g.find("ps") != g.end()) {
                    for (size_t i = 0; i < anom.values.size(); i++) {
                        if (g["ps"].values[i] < 70000.0f) {
                            anom.values[i] = std::nanf("");
                        }
                    }
                }
                g["anom"] = std::move(anom);
            };
            x.fill = [] (const Grids& g) { return pick(g, "anom"); };
            x.ramp = heightAnomaly();
            x.fillTitle = "Height anomaly from the 1991-2020 mean (dam)";
            x.legendStep = 5;
            x.contours = {heights(interval)};
            x.barbU = "u";
            x.barbV = "v";
            p.push_back(x);
        }
        {
            Product x;
            x.id = "mslp_anom";
            x.label = "MSLP and Anomaly";
            x.wants = {want("p", "PRMSL", "mean sea level"), want("u", "UGRD", "10 m above ground"), want("v", "VGRD", "10 m above ground")};
            x.derive = [validDay] (Grids& g, const Context& context) {
                GfsGrid::Grid normal;
                std::string error;
                if (!context.climate || !context.climate->at(GfsClimate::seaLevelPressure(), validDay(context), normal, error)) {
                    return;
                }
                g["anom"] = GfsGrid::scaled(GfsGrid::anomaly(g["p"], normal), 0.01);   // Pa -> mb
            };
            x.fill = [] (const Grids& g) { return pick(g, "anom"); };
            x.ramp = pressureAnomaly();
            x.fillTitle = "Pressure anomaly from the 1991-2020 mean (mb)";
            x.legendStep = 5;
            x.contours = {pressure()};
            x.barbU = "u";
            x.barbV = "v";
            p.push_back(x);
        }
        // precipitation: the running total from the start of the run is in every file, so a period is the total at its end less the total at its start
        const auto lastHourOk = [] (int h) { return h <= 120 || (h <= 240 && h % 3 == 0) || h % 6 == 0; };
        const auto startOf = [lastHourOk] (int hour, int period) {
            int s = std::max(0, hour - period);
            while (s > 0 && !lastHourOk(s)) {
                s--;
            }
            return s;
        };
        const auto totalNeeds = [startOf] (int hour, int period) {
            std::vector<GfsData::Need> needs;
            needs.push_back({hour, {"a1", "APCP", "surface", "0-*"}});
            const int s = startOf(hour, period);
            if (s > 0) {
                needs.push_back({s, {"a0", "APCP", "surface", "0-*"}});
            }
            return needs;
        };
        const auto totalDerive = [] (Grids& g, const Context&) {
            auto precip = g["a1"];
            if (g.find("a0") != g.end()) {
                precip = GfsGrid::difference(precip, g["a0"]);
            }
            for (auto& v : precip.values) {
                v = std::max(0.0f, v);
            }
            g["precip"] = std::move(precip);
        };
        const auto precipFill = [] (const Grids& g) { return pick(g, "precip"); };
        const auto precipTitle = [startOf] (int period) {
            return [startOf, period] (int hour) {
                if (period <= 0) {
                    return std::string{"Precipitation, hours 0-" + std::to_string(hour)};
                }
                return "Precipitation, hours " + std::to_string(startOf(hour, period)) + "-" + std::to_string(hour);
            };
        };
        for (const auto& [id, period] : {std::pair{"precip_p01", 1}, {"precip_p03", 3}, {"precip_p06", 6}, {"precip_p12", 12}, {"precip_p24", 24}, {"precip_p36", 36}, {"precip_p48", 48}, {"precip_p60", 60}, {"precip_ptot", 0}}) {
            Product x;
            x.id = id;
            x.label = period == 0 ? "Total Accumulated Precipitation of Period" : "Total Precipitation every " + std::to_string(period) + " hour" + (period == 1 ? "" : "s");
            x.needs = [totalNeeds, period] (int hour) { return totalNeeds(hour, period > 0 ? period : hour); };
            x.derive = totalDerive;
            x.fill = precipFill;
            x.ramp = precipitation();
            x.fillTitleFor = precipTitle(period);
            x.quantity = Quantity::Millimeters;
            x.legendStep = 0.0;
            p.push_back(x);
        }
        {   // simulated radar
            Product x;
            x.id = "sim_radar_comp";
            x.label = "Simulated Composite Radar Reflectivity";
            x.wants = {want("r", "REFC", "entire atmosphere")};
            x.fill = [] (const Grids& g) { return pick(g, "r"); };
            x.ramp = reflectivity();
            x.fillTitle = "Composite reflectivity (dBZ)";
            x.legendStep = 10;
            p.push_back(x);
        }
        {   // snow depth change since the start
            Product x;
            x.id = "snodpth_chng";
            x.label = "Snow Depth Change from F00";
            x.needs = [] (int hour) {
                return std::vector<GfsData::Need>{{hour, {"s1", "SNOD", "surface", ""}}, {0, {"s0", "SNOD", "surface", "anl"}}};
            };
            x.derive = [] (Grids& g, const Context&) { g["snow"] = GfsGrid::scaled(GfsGrid::difference(g["s1"], g["s0"]), 100.0); };   // meters -> centimeters
            x.fill = [] (const Grids& g) { return pick(g, "snow"); };
            x.ramp = snowChange();
            x.fillTitle = "Snow depth change";
            x.quantity = Quantity::Centimeters;
            x.legendStep = 0.0;
            p.push_back(x);
        }
        // sea level pressure with thickness lines (the rain / snow edge) over a precipitation fill
        const auto thickness = [&] (const char * id, const char * label, const char * low, const char * high, double interval, double edge, const char * what) {
            Product x;
            x.id = id;
            x.label = label;
            x.needs = [totalNeeds, low, high] (int hour) {
                auto needs = totalNeeds(hour, hour <= 240 ? 3 : 6);
                needs.push_back({hour, {"p", "PRMSL", "mean sea level", ""}});
                needs.push_back({hour, {"zl", "HGT", low, ""}});
                needs.push_back({hour, {"zh", "HGT", high, ""}});
                return needs;
            };
            x.derive = [totalDerive] (Grids& g, const Context& context) {
                totalDerive(g, context);
                g["thick"] = GfsGrid::difference(g["zh"], g["zl"]);
            };
            x.fill = precipFill;
            x.ramp = precipitation();
            x.fillTitleFor = precipTitle(0);
            x.fillTitleFor = [startOf] (int hour) { return "Precipitation, hours " + std::to_string(startOf(hour, hour <= 240 ? 3 : 6)) + "-" + std::to_string(hour); };
            x.quantity = Quantity::Millimeters;
            x.legendStep = 0.0;
            ContourSet t;
            t.key = "thick";
            t.scale = 0.1;
            t.interval = interval;
            t.base = 0.0;
            t.title = what;
            t.color = QColor{190, 50, 40};
            t.colorBelow = QColor{40, 90, 190};
            t.split = edge;
            t.dashed = true;
            t.width = 1.3;
            x.contours = {pressure(), t};
            return x;
        };
        p.push_back(thickness("1000_500_thick", "MSLP, 1000-500mb thickness and precipitation", "1000 mb", "500 mb", 6, 540, "1000-500mb thickness (dam)"));
        p.push_back(thickness("1000_850_thick", "MSLP, 1000-850mb thickness and precipitation", "1000 mb", "850 mb", 3, 130, "1000-850mb thickness (dam)"));
        p.push_back(thickness("850_700_thick", "MSLP, 850-700mb thickness and precipitation", "850 mb", "700 mb", 3, 154, "850-700mb thickness (dam)"));
        {   // 850 mb temperature as lines under the precipitation
            auto x = thickness("850_temp_mslp_precip", "MSLP, 850mb temperature and precipitation", "850 mb", "500 mb", 6, 0, "");
            x.needs = [totalNeeds] (int hour) {
                auto needs = totalNeeds(hour, hour <= 240 ? 3 : 6);
                needs.push_back({hour, {"p", "PRMSL", "mean sea level", ""}});
                needs.push_back({hour, {"t", "TMP", "850 mb", ""}});
                return needs;
            };
            x.derive = totalDerive;
            ContourSet t;
            t.key = "t";
            t.interval = 5;
            t.title = "850mb temperature (C)";
            t.color = QColor{190, 50, 40};
            t.colorBelow = QColor{40, 90, 190};
            t.split = 0.0;
            t.dashed = true;
            t.width = 1.3;
            x.contours = {pressure(), t};
            p.push_back(x);
        }
        {   // 10 m wind over the precipitation, with sea level pressure
            auto x = thickness("10m_wnd_precip", "MSLP, 10m wind and precipitation", "850 mb", "500 mb", 6, 0, "");
            x.needs = [totalNeeds] (int hour) {
                auto needs = totalNeeds(hour, hour <= 240 ? 3 : 6);
                needs.push_back({hour, {"p", "PRMSL", "mean sea level", ""}});
                needs.push_back({hour, {"u", "UGRD", "10 m above ground", ""}});
                needs.push_back({hour, {"v", "VGRD", "10 m above ground", ""}});
                return needs;
            };
            x.derive = totalDerive;
            x.contours = {pressure()};
            x.barbU = "u";
            x.barbV = "v";
            p.push_back(x);
        }
        // the quick ones: one or two records each
        {
            Product x;
            x.id = "2m_dewpoint";
            x.label = "MSLP, 10m wind, 2m dewpoint";
            x.wants = {want("d", "DPT", "2 m above ground"), want("u", "UGRD", "10 m above ground"), want("v", "VGRD", "10 m above ground"), want("p", "PRMSL", "mean sea level")};
            x.fill = [] (const Grids& g) { return pick(g, "d"); };
            x.ramp = dewpoint();
            x.fillTitle = "2 m dewpoint";
            x.legendStep = 5;
            x.quantity = Quantity::Temperature;
            x.contours = {pressure()};
            x.barbU = "u";
            x.barbV = "v";
            p.push_back(x);
        }
        {
            Product x;
            x.id = "rh_700_300";
            x.label = "700-300mb Mean Relative Humidity, 500mb Height and Wind";
            x.wants = {want("r7", "RH", "700 mb"), want("r5", "RH", "500 mb"), want("r3", "RH", "300 mb"), want("z", "HGT", "500 mb"), want("u", "UGRD", "500 mb"), want("v", "VGRD", "500 mb")};
            x.derive = [] (Grids& g, const Context&) {
                auto mean = g["r7"];
                for (size_t i = 0; i < mean.values.size(); i++) {
                    mean.values[i] = (g["r7"].values[i] + g["r5"].values[i] + g["r3"].values[i]) / 3.0f;
                }
                g["rm"] = std::move(mean);
            };
            x.fill = [] (const Grids& g) { return pick(g, "rm"); };
            x.ramp = humidity();
            x.fillTitle = "Mean relative humidity, 700-300mb (%)";
            x.legendStep = 10;
            x.contours = {heights(6)};
            x.barbU = "u";
            x.barbV = "v";
            p.push_back(x);
        }
        {
            Product x;
            x.id = "mslp_pwat";
            x.label = "MSLP and Precipitable Water";
            x.wants = {want("pw", "PWAT", "entire atmosphere (considered as a single layer)"), want("p", "PRMSL", "mean sea level")};
            x.fill = [] (const Grids& g) { return pick(g, "pw"); };
            x.ramp = precipitableWater();
            x.fillTitle = "Precipitable water";
            x.quantity = Quantity::Millimeters;
            x.legendStep = 0.5;
            x.contours = {pressure()};
            p.push_back(x);
        }
        {
            Product x;
            x.id = "sbcape_wind";
            x.label = "Surface-Based CAPE, MSLP and 10m Wind";
            x.wants = {want("c", "CAPE", "surface"), want("u", "UGRD", "10 m above ground"), want("v", "VGRD", "10 m above ground"), want("p", "PRMSL", "mean sea level")};
            x.fill = [] (const Grids& g) { return pick(g, "c"); };
            x.ramp = capeRamp();
            x.fillTitle = "Surface-based CAPE (J/kg)";
            x.legendStep = 500;
            x.contours = {pressure()};
            x.contours[0].highsAndLows = false;
            x.barbU = "u";
            x.barbV = "v";
            p.push_back(x);
        }
        {
            Product x;
            x.id = "700_temp_mslp";
            x.label = "700mb Temperature, Wind and MSLP";
            x.wants = {want("t", "TMP", "700 mb"), want("u", "UGRD", "700 mb"), want("v", "VGRD", "700 mb"), want("p", "PRMSL", "mean sea level")};
            x.fill = [] (const Grids& g) { return pick(g, "t"); };
            x.ramp = temperature();
            x.fillTitle = "700mb temperature";
            x.legendStep = 5;
            x.quantity = Quantity::Temperature;
            x.contours = {pressure()};
            x.contours[0].highsAndLows = false;
            x.barbU = "u";
            x.barbV = "v";
            p.push_back(x);
        }
        {
            Product x;
            x.id = "mslp_10m_wind";
            x.label = "MSLP and 10m Wind";
            x.wants = {want("u", "UGRD", "10 m above ground"), want("v", "VGRD", "10 m above ground"), want("p", "PRMSL", "mean sea level")};
            x.fill = speedOf("u", "v");
            x.ramp = lowWind();
            x.fillTitle = "10 m wind speed (kt)";
            x.legendStep = 10;
            x.contours = {pressure()};
            x.barbU = "u";
            x.barbV = "v";
            p.push_back(x);
        }
        {
            auto x = upper("850_wnd_ht", "850mb Height and Wind", "850 mb", 3);
            x.ramp = lowWind();
            p.push_back(x);
        }
        {   // rain, snow and mixed precipitation in their own colors (the type flags are at the forecast hour; the amount is the 3 or 6 hour total)
            Product x;
            x.id = "precip_type";
            x.label = "MSLP and Precipitation (Rain / Snow / Mixed)";
            x.needs = [totalNeeds] (int hour) {
                auto needs = totalNeeds(hour, hour <= 240 ? 3 : 6);
                needs.push_back({hour, {"p", "PRMSL", "mean sea level", ""}});
                for (const auto& [key, variable] : {std::pair{"cr", "CRAIN"}, {"cs", "CSNOW"}, {"cf", "CFRZR"}, {"ci", "CICEP"}}) {
                    needs.push_back({hour, {key, variable, "surface", ""}});
                }
                return needs;
            };
            x.derive = [totalDerive] (Grids& g, const Context& context) {
                totalDerive(g, context);
                const auto& total = g["precip"];
                auto rain = total, snow = total, mix = total;
                for (size_t i = 0; i < total.values.size(); i++) {
                    const bool isSnow = g["cs"].values[i] >= 0.5f;
                    const bool isMix = g["cf"].values[i] >= 0.5f || g["ci"].values[i] >= 0.5f;
                    rain.values[i] = (!isSnow && !isMix) ? total.values[i] : 0.0f;   // none of the flags set (or no flags at all): rain
                    snow.values[i] = isSnow && !isMix ? total.values[i] : 0.0f;
                    mix.values[i] = isMix ? total.values[i] : 0.0f;
                }
                g["rain"] = std::move(rain);
                g["snow"] = std::move(snow);
                g["mix"] = std::move(mix);
            };
            x.fill = [] (const Grids& g) { return pick(g, "rain"); };
            x.ramp = precipitation();
            x.overlays = {{"snow", snowPrecipitation()}, {"mix", mixedPrecipitation()}};
            x.fillTitleFor = [startOf] (int hour) { return "Precipitation (green rain, blue snow, pink mixed), hours " + std::to_string(startOf(hour, hour <= 240 ? 3 : 6)) + "-" + std::to_string(hour); };
            x.quantity = Quantity::Millimeters;
            x.legendStep = 0.0;
            x.contours = {pressure()};
            p.push_back(x);
        }
        {   // what the last 48 hours did to the pressure and the 500 mb heights
            const auto trendNeeds = [] (const char * variable, const char * level, int hour) {
                const int start = std::max(0, hour - 48);
                return std::vector<GfsData::Need>{{hour, {"now", variable, level, ""}}, {start, {"then", variable, level, ""}}};
            };
            const auto trendTitle = [] (const char * what) {
                return [what] (int hour) { return std::string{what} + " change, hours " + std::to_string(std::max(0, hour - 48)) + "-" + std::to_string(hour); };
            };
            Product x;
            x.id = "mslp_trend";
            x.label = "MSLP and 48-hour Change";
            x.needs = [trendNeeds] (int hour) { return trendNeeds("PRMSL", "mean sea level", hour); };
            x.derive = [] (Grids& g, const Context&) { g["p"] = g["now"]; g["trend"] = GfsGrid::scaled(GfsGrid::difference(g["now"], g["then"]), 0.01); };
            x.fill = [] (const Grids& g) { return pick(g, "trend"); };
            x.ramp = pressureAnomaly();
            x.fillTitleFor = trendTitle("Sea level pressure");
            x.fillTitle = "Sea level pressure change (mb)";
            x.legendStep = 5;
            x.contours = {pressure()};
            p.push_back(x);
            Product y;
            y.id = "z500_trend";
            y.label = "500mb Height and 48-hour Change";
            y.needs = [trendNeeds] (int hour) { return trendNeeds("HGT", "500 mb", hour); };
            y.derive = [] (Grids& g, const Context&) { g["z"] = g["now"]; g["trend"] = GfsGrid::scaled(GfsGrid::difference(g["now"], g["then"]), 0.1); };
            y.fill = [] (const Grids& g) { return pick(g, "trend"); };
            y.ramp = heightAnomaly();
            y.fillTitleFor = trendTitle("500mb height");
            y.fillTitle = "500mb height change (dam)";
            y.legendStep = 5;
            y.contours = {heights(6)};
            p.push_back(y);
        }

        // ---- the National Blend of Models: the plain field of each (NWS's blend of many models, 2.5 km over the contiguous United States)
        const auto nbmWant = [] (const std::string& key, const char * variable, const char * level, const std::string& forecast, const std::string& detail = "") {
            return GfsData::Want{key, variable, level, forecast, detail};
        };
        const auto atHour = [] (int hour) { return std::to_string(hour) + " hour fcst"; };
        const auto window = [] (int hour, int length, const char * kind) { return std::to_string(hour - length) + "-" + std::to_string(hour) + " hour " + kind + " fcst"; };
        // wind from its speed and direction: u and v in m/s as the barbs and the speed fills want them
        const auto windComponents = [] (Grids& g, const Context&) {
            auto u = g["ws"], v = g["ws"];
            for (size_t i = 0; i < u.values.size(); i++) {
                const double speedNow = g["ws"].values[i], from = g["wd"].values[i] * pi / 180.0;
                u.values[i] = static_cast<float>(-speedNow * std::sin(from));
                v.values[i] = static_cast<float>(-speedNow * std::cos(from));
            }
            g["u"] = std::move(u);
            g["v"] = std::move(v);
        };
        const auto nbmSurface = [&] (const char * id, const char * label, const char * variable, const char * level, const Ramp& ramp, const char * title, Quantity quantity, double step) {
            Product x;
            x.source = "NBM";
            x.id = id;
            x.label = label;
            x.needs = [=] (int hour) {
                return std::vector<GfsData::Need>{{hour, nbmWant("f", variable, level, std::to_string(hour) + " hour fcst")}, {hour, nbmWant("ws", "WIND", "10 m above ground", std::to_string(hour) + " hour fcst")},
                                                  {hour, nbmWant("wd", "WDIR", "10 m above ground", std::to_string(hour) + " hour fcst")}};
            };
            x.derive = windComponents;
            x.fill = [] (const Grids& g) { return pick(g, "f"); };
            x.ramp = ramp;
            x.fillTitle = title;
            x.quantity = quantity;
            x.legendStep = step;
            x.barbU = "u";
            x.barbV = "v";
            return x;
        };
        p.push_back(nbmSurface("2m_temp_10m_wnd", "2 m Temperature and 10 m Wind", "TMP", "2 m above ground", temperature(), "2 m temperature", Quantity::Temperature, 5));
        p.push_back(nbmSurface("2m_dewp_10m_wnd", "2 m Dewpoint and 10 m Wind", "DPT", "2 m above ground", dewpoint(), "2 m dewpoint", Quantity::Temperature, 5));
        p.push_back(nbmSurface("2m_relh_10m_wnd", "2 m Relative Humidity and 10 m Wind", "RH", "2 m above ground", humidity(), "2 m relative humidity (%)", Quantity::Other, 10));
        p.push_back(nbmSurface("2m_apparent_temp", "2 m Apparent Temperature and 10 m Wind", "APTMP", "2 m above ground", temperature(), "2 m apparent temperature", Quantity::Temperature, 5));
        p.push_back(nbmSurface("total_cloud_cover", "Total Cloud Cover and 10 m Wind", "TCDC", "surface", cloudCover(), "Total cloud cover (%)", Quantity::Other, 10));
        {   // the gust is the fill; the barbs are the sustained wind
            auto x = nbmSurface("10m_wnd_gust", "10 m Wind and Gust", "GUST", "10 m above ground", lowWind(), "10 m wind gust (kt)", Quantity::Other, 10);
            x.fill = [] (const Grids& g) { return GfsGrid::scaled(pick(g, "f"), msToKnots); };
            p.push_back(x);
        }
        {   // the temperature extreme of the 12 hours that end at this hour (the blend has them only for the periods it makes: 12 hour minimum and maximum)
            for (const auto& [id, label, variable, kind] : {std::tuple{"2m_min_temp", "2 m Minimum Temperature (12 hours ending)", "TMIN", "min"}, {"2m_max_temp", "2 m Maximum Temperature (12 hours ending)", "TMAX", "max"}}) {
                Product x;
                x.source = "NBM";
                x.id = id;
                x.label = label;
                x.needs = [=] (int hour) { return std::vector<GfsData::Need>{{hour, nbmWant("f", variable, "2 m above ground", window(hour, 12, kind))}}; };
                x.fill = [] (const Grids& g) { return pick(g, "f"); };
                x.ramp = temperature();
                x.fillTitle = "2 m temperature extreme";
                x.quantity = Quantity::Temperature;
                x.legendStep = 5;
                p.push_back(x);
            }
        }
        // precipitation and snowfall of the 1, 6 and 12 hours that end at the hour; the whole total is the sum of the 6 hour pieces
        const auto nbmAccum = [&] (const char * id, const char * label, const char * variable, int length, const Ramp& ramp, Quantity quantity, double scale) {
            Product x;
            x.source = "NBM";
            x.id = id;
            x.label = label;
            x.needs = [=] (int hour) {
                std::vector<GfsData::Need> needs;
                if (length > 0) {
                    needs.push_back({hour, nbmWant("a0", variable, "surface", window(hour, length, "acc"))});
                } else {   // the sum of the 6 hour pieces from the start to this hour
                    int n = 0;
                    for (int end = hour; end > 0; end -= 6) {
                        needs.push_back({end, nbmWant("a" + std::to_string(n++), variable, "surface", window(end, std::min(6, end), "acc"))});
                    }
                }
                return needs;
            };
            if (length != 1) {   // the 1 hour amounts of the same run added up: for the hours the 6 and 12 hour amounts are not made, as long as the hourly files go (the first 36 hours)
                x.fallbackNeeds = [=] (int hour) {
                    std::vector<GfsData::Need> needs;
                    const int count = length > 0 ? length : hour;
                    if (hour > 36 || hour - count < 0) {
                        return needs;
                    }
                    for (int i = 0; i < count; i++) {
                        needs.push_back({hour - i, nbmWant("a" + std::to_string(i), variable, "surface", window(hour - i, 1, "acc"))});
                    }
                    return needs;
                };
            }
            x.derive = [scale] (Grids& g, const Context&) {
                auto sum = g["a0"];
                for (const auto& [key, grid] : g) {
                    if (key != "a0" && key.size() >= 2 && key[0] == 'a' && std::isdigit(static_cast<unsigned char>(key[1]))) {
                        for (size_t i = 0; i < sum.values.size(); i++) {
                            sum.values[i] += grid.values[i];
                        }
                    }
                }
                for (auto& v : sum.values) {
                    v = static_cast<float>(std::max(0.0f, v) * scale);
                }
                g["sum"] = std::move(sum);
            };
            x.fill = [] (const Grids& g) { return pick(g, "sum"); };
            x.ramp = ramp;
            x.quantity = quantity;
            x.fillTitleFor = [length, label] (int hour) {
                return length > 0 ? std::string{label} + ", the " + std::to_string(length) + " hours ending at hour " + std::to_string(hour) : std::string{label} + ", from the start to hour " + std::to_string(hour);
            };
            x.legendStep = 0.0;
            return x;
        };
        p.push_back(nbmAccum("precip_p01", "Total Precipitation", "APCP", 1, precipitation(), Quantity::Millimeters, 1.0));
        p.push_back(nbmAccum("precip_p06", "Total Precipitation", "APCP", 6, precipitation(), Quantity::Millimeters, 1.0));
        p.push_back(nbmAccum("precip_p12", "Total Precipitation", "APCP", 12, precipitation(), Quantity::Millimeters, 1.0));
        p.push_back(nbmAccum("precip_ptot", "Accumulated Precipitation", "APCP", 0, precipitation(), Quantity::Millimeters, 1.0));
        p.push_back(nbmAccum("snow_p06", "Snowfall", "ASNOW", 6, snowfall(), Quantity::Centimeters, 100.0));   // the grid is in meters
        p.push_back(nbmAccum("snow_p12", "Snowfall", "ASNOW", 12, snowfall(), Quantity::Centimeters, 100.0));
        p.push_back(nbmAccum("snow_ptot", "Accumulated Snowfall", "ASNOW", 0, snowfall(), Quantity::Centimeters, 100.0));
        {   // the chance of thunder in the 6 hours that end at this hour, and the CAPE
            Product x;
            x.source = "NBM";
            x.id = "tstm_prob";
            x.label = "Thunderstorm Probability (6 hours ending)";
            x.needs = [=] (int hour) { return std::vector<GfsData::Need>{{hour, nbmWant("f", "TSTM", "surface", window(hour, 6, "acc"), "probability forecast")}}; };
            x.fill = [] (const Grids& g) { return pick(g, "f"); };
            x.ramp = probability();
            x.fillTitle = "Chance of a thunderstorm (%)";
            x.legendStep = 10;
            p.push_back(x);
            Product y;
            y.source = "NBM";
            y.id = "cape";
            y.label = "Surface-Based CAPE";
            y.needs = [=] (int hour) { return std::vector<GfsData::Need>{{hour, nbmWant("f", "CAPE", "surface", atHour(hour))}}; };
            y.fill = [] (const Grids& g) { return pick(g, "f"); };
            y.ramp = capeRamp();
            y.fillTitle = "Surface-based CAPE (J/kg)";
            y.legendStep = 500;
            p.push_back(y);
        }

        // ---- AIGFS: the same charts as the GFS, from its two files. Humidity comes as specific humidity (relative humidity is worked out from it with the temperature), and
        // precipitation as 6 hour amounts (a period is the amounts of its 6 hour pieces added up).
        const auto relativeHumidity = [] (const GfsGrid::Grid& temperature, const GfsGrid::Grid& specific, double hPa) {
            auto out = temperature;
            for (size_t i = 0; i < out.values.size(); i++) {
                const double t = temperature.values[i], q = specific.values[i];
                const double e = q * hPa / (0.622 + 0.378 * q);                       // the vapor pressure, hPa
                const double es = 6.112 * std::exp(17.67 * t / (t + 243.5));            // the saturation vapor pressure over water
                out.values[i] = static_cast<float>(std::clamp(100.0 * e / es, 0.0, 100.0));
            }
            return out;
        };
        const auto adapt = [relativeHumidity] (const Product& gfs, const char * model, bool specific) {
            Product x = gfs;
            x.source = model;
            x.needs = [gfs, specific] (int hour) {
                std::vector<GfsData::Need> out;
                int end = -1, start = 0;
                for (auto need : GfsChart::needs(gfs, hour)) {
                    if (need.want.variable == "APCP") {   // the running total is not in the file: its pieces are taken below
                        (need.want.key == "a0" ? start : end) = need.hour;
                        continue;
                    }
                    if (specific && need.want.variable == "RH") {     // the temperature and the specific humidity of that level, the level kept in the keys
                        const auto key = need.want.key + "|" + need.want.level;
                        auto t = need, q = need;
                        t.want.variable = "TMP";
                        t.want.key = key + "|t";
                        q.want.variable = "SPFH";
                        q.want.key = key + "|q";
                        out.push_back(t);
                        out.push_back(q);
                        continue;
                    }
                    out.push_back(std::move(need));
                }
                if (end > 0) {
                    int n = 0;
                    for (int e = end; e > start && e > 0; e -= 6) {
                        out.push_back({e, {"w" + std::to_string(n++), "APCP", "surface", std::to_string(e - 6) + "-" + std::to_string(e) + " hour acc fcst", ""}});
                    }
                }
                return out;
            };
            x.fallbackNeeds = nullptr;
            x.derive = [gfs, relativeHumidity] (Grids& g, const Context& context) {
                // relative humidity from each level's pair
                std::vector<std::string> keys;
                for (const auto& [key, grid] : g) {
                    const auto bar = key.find('|');
                    if (bar != std::string::npos && key.size() > 2 && key.compare(key.size() - 2, 2, "|q") == 0) {
                        keys.push_back(key.substr(0, key.size() - 2));
                    }
                }
                for (const auto& key : keys) {
                    const auto bar = key.find('|');
                    const double hPa = std::atof(key.c_str() + bar + 1);   // "rh|500 mb" -> 500
                    g[key.substr(0, bar)] = relativeHumidity(g[key + "|t"], g[key + "|q"], hPa);
                }
                // the precipitation of the period: its 6 hour pieces added up, as the running total less nothing (the pieces start where the period does)
                if (g.count("w0")) {
                    auto sum = g["w0"];
                    for (int i = 1; g.count("w" + std::to_string(i)); i++) {
                        const auto& piece = g["w" + std::to_string(i)];
                        for (size_t k = 0; k < sum.values.size(); k++) {
                            sum.values[k] += piece.values[k];
                        }
                    }
                    g["a1"] = std::move(sum);
                }
                if (gfs.derive) {
                    gfs.derive(g, context);
                }
            };
            if (gfs.fillTitleFor) {   // the pieces are 6 hours: the period is told from them
                x.fillTitleFor = [x, gfs] (int hour) {
                    int lowest = hour;
                    for (const auto& need : x.needs(hour)) {
                        if (need.want.variable == "APCP") {
                            lowest = std::min(lowest, need.hour - 6);
                        }
                    }
                    const auto original = gfs.fillTitleFor(hour);
                    const auto at = original.find(", hours ");
                    return (at == std::string::npos ? original : original.substr(0, at)) + ", hours " + std::to_string(std::max(0, lowest)) + "-" + std::to_string(hour);
                };
            }
            return x;
        };
        for (const char * id : {"precip_p06", "precip_p12", "precip_p24", "precip_p36", "precip_p48", "precip_p60", "precip_ptot", "1000_500_thick", "1000_850_thick", "850_700_thick", "850_temp_mslp_precip",
                                "10m_wnd_precip", "10m_wnd_2m_temp", "200_wnd_ht", "250_wnd_ht", "300_wnd_ht", "500_rh_ht", "500_wnd_ht", "500_vort_ht", "700_rh_ht", "850_rh_ht", "850_temp_ht", "850_vort_ht",
                                "850vor_500ht_200wd", "925_temp_ht"}) {
            const Product * original = nullptr;
            for (const auto& candidate : p) {
                if (candidate.id == id && candidate.source == "GFS") {
                    original = &candidate;
                    break;
                }
            }
            if (original != nullptr) {
                auto x = adapt(*original, "AIGFS", true);
                p.push_back(std::move(x));
            }
        }

        // ---- GEFS: the ensemble mean drawn like the GFS (every chart whose fields the mean files hold: they have relative humidity, and precipitation in 6 hour pieces like the AI model),
        // and the spread (the standard deviation of the 30 members) of the fields that matter, filled under the mean's lines
        {
            const std::set<std::string> have{"HGT", "TMP", "RH", "UGRD", "VGRD", "VVEL", "PRMSL", "PWAT", "CAPE", "CIN", "APCP", "CRAIN", "CSNOW", "CFRZR", "CICEP", "TCDC", "DPT", "GUST", "SNOD", "WEASD",
                                             "TMAX", "TMIN", "HLCY"};
            const std::set<std::string> levels{"10 mb", "50 mb", "100 mb", "200 mb", "250 mb", "300 mb", "400 mb", "500 mb", "700 mb", "850 mb", "925 mb", "1000 mb"};
            const auto size = p.size();
            for (size_t i = 0; i < size; i++) {
                if (p[i].source != "GFS") {
                    continue;
                }
                bool fits = true;
                for (const auto& need : GfsChart::needs(p[i], 24)) {
                    const auto& w = need.want;
                    const bool pressureLevel = w.level.size() > 3 && w.level.compare(w.level.size() - 3, 3, " mb") == 0;
                    if (!have.count(w.variable) || (pressureLevel && !levels.count(w.level))) {
                        fits = false;
                    }
                }
                if (fits) {
                    auto x = adapt(p[i], "GEFS", false);
                    x.label = "Mean " + x.label;
                    p.push_back(std::move(x));
                }
            }
            const auto spr = [] (const char * key, const char * variable, const char * level) { return GfsData::Want{key, variable, level, "", "", "spr"}; };
            const auto meanHeights = [&heights] (double interval) { auto c = heights(interval); c.title = "Mean height (dam)"; return c; };
            const auto spread = [&] (const char * id, const char * label, std::vector<GfsData::Want> wants, std::function<GfsGrid::Grid(const Grids&)> fill, const char * title, double top, double step) {
                Product x;
                x.source = "GEFS";
                x.id = id;
                x.label = label;
                x.wants = std::move(wants);
                x.fill = std::move(fill);
                x.ramp = spreadRamp(top);
                x.fillTitle = title;
                x.legendStep = step;
                return x;
            };
            for (const auto& [level, top, interval] : {std::tuple{"250 mb", 120.0, 12.0}, {"500 mb", 60.0, 6.0}, {"700 mb", 40.0, 3.0}, {"850 mb", 30.0, 3.0}}) {
                const std::string n = std::string{level}.substr(0, std::string{level}.size() - 3);
                auto z = spread(("spread_" + n + "_ht").c_str(), (n + "mb Height Spread and Mean Height").c_str(), {spr("s", "HGT", level), want("z", "HGT", level)},
                                [] (const Grids& g) { return pick(g, "s"); }, ("Spread of the " + n + " mb height (m)").c_str(), top, top / 6);
                z.contours = {meanHeights(interval)};
                p.push_back(z);
                auto w = spread(("spread_" + n + "_wnd").c_str(), (n + "mb Wind Spread, Mean Wind and Height").c_str(),
                                {spr("su", "UGRD", level), spr("sv", "VGRD", level), want("u", "UGRD", level), want("v", "VGRD", level), want("z", "HGT", level)},
                                speedOf("su", "sv"), ("Spread of the " + n + " mb wind (kt)").c_str(), n == "250" ? 30.0 : 20.0, 5);
                w.contours = {meanHeights(interval)};
                w.barbU = "u";
                w.barbV = "v";
                p.push_back(w);
                if (n != "250") {
                    auto t = spread(("spread_" + n + "_temp").c_str(), (n + "mb Temperature Spread and Mean Height").c_str(), {spr("s", "TMP", level), want("z", "HGT", level)},
                                    [] (const Grids& g) { return pick(g, "s"); }, ("Spread of the " + n + " mb temperature (C)").c_str(), 5.0, 1);
                    t.contours = {meanHeights(interval)};
                    p.push_back(t);
                }
            }
            {
                auto x = spread("spread_mslp", "Sea Level Pressure Spread and Mean", {spr("s", "PRMSL", "mean sea level"), want("p", "PRMSL", "mean sea level")},
                                [] (const Grids& g) { return GfsGrid::scaled(pick(g, "s"), 0.01); }, "Spread of the sea level pressure (mb)", 6.0, 1);
                x.contours = {pressure()};
                p.push_back(x);
            }
            {
                auto x = spread("spread_2m_temp", "2m Temperature Spread, Mean Pressure and Wind", {spr("s", "TMP", "2 m above ground"), want("p", "PRMSL", "mean sea level"), want("u", "UGRD", "10 m above ground"),
                                want("v", "VGRD", "10 m above ground")}, [] (const Grids& g) { return pick(g, "s"); }, "Spread of the 2 m temperature (C)", 6.0, 1);
                x.contours = {pressure()};
                x.barbU = "u";
                x.barbV = "v";
                p.push_back(x);
            }
            {
                auto x = spread("spread_10m_wnd", "10m Wind Spread and Mean Wind", {spr("su", "UGRD", "10 m above ground"), spr("sv", "VGRD", "10 m above ground"), want("u", "UGRD", "10 m above ground"),
                                want("v", "VGRD", "10 m above ground"), want("p", "PRMSL", "mean sea level")}, speedOf("su", "sv"), "Spread of the 10 m wind (kt)", 10.0, 2);
                x.contours = {pressure()};
                x.barbU = "u";
                x.barbV = "v";
                p.push_back(x);
            }
            {
                auto x = spread("spread_pwat", "Precipitable Water Spread", {spr("s", "PWAT", "entire atmosphere (considered as a single layer)"), want("p", "PRMSL", "mean sea level")},
                                [] (const Grids& g) { return pick(g, "s"); }, "Spread of the precipitable water (mm)", 12.0, 2);
                x.contours = {pressure()};
                p.push_back(x);
            }
            {
                auto x = spread("spread_cape", "Surface-Based CAPE Spread", {spr("s", "CAPE", "surface"), want("c", "CAPE", "surface")}, [] (const Grids& g) { return pick(g, "s"); },
                                "Spread of the CAPE (J/kg)", 1500.0, 250);
                p.push_back(x);
            }
        }

        // ---- HAFS: the hurricane model for one storm (the screen picks the storm): wind, simulated radar and satellite, rain, sea surface temperature, shear, waves
        for (const char * model : {"HAFSA", "HAFSB"}) {
            const auto hurricaneLines = [&pressure] {
                auto c = pressure();
                c.highsAndLows = false;
                return c;
            };
            const auto make = [model] (const char * id, const char * label) {
                Product x;
                x.source = model;
                x.id = id;
                x.label = label;
                return x;
            };
            {
                auto x = make("wind_mslp", "10m Wind, MSLP and Barbs");
                x.wants = {want("u", "UGRD", "10 m above ground"), want("v", "VGRD", "10 m above ground"), want("p", "PRMSL", "mean sea level")};
                x.fill = speedOf("u", "v");
                x.ramp = tropicalWind();
                x.fillTitle = "10 m wind speed (kt)";
                x.legendStep = 0;
                x.contours = {hurricaneLines()};
                x.barbU = "u";
                x.barbV = "v";
                p.push_back(x);
            }
            {
                auto x = make("reflectivity", "Simulated Radar (Composite Reflectivity) and MSLP");
                x.wants = {want("r", "REFC", "entire atmosphere (considered as a single layer)"), want("p", "PRMSL", "mean sea level")};
                x.fill = [] (const Grids& g) { return pick(g, "r"); };
                x.ramp = reflectivity();
                x.fillTitle = "Composite reflectivity (dBZ)";
                x.legendStep = 10;
                x.contours = {hurricaneLines()};
                p.push_back(x);
            }
            // the simulated satellite channels (the file names them only by number): 65 is a window channel (the surface shows through; the cold cloud tops in color), 53 / 54 / 55 are
            // water vapor channels from the upper level down
            for (const auto& [band, id, label, vapor] : {std::tuple{65, "sat_ir", "Simulated Satellite: Infrared (window)", false}, {53, "sat_wv_upper", "Simulated Satellite: Water Vapor, upper level", true},
                                                         {54, "sat_wv_mid", "Simulated Satellite: Water Vapor, middle level", true}, {55, "sat_wv_low", "Simulated Satellite: Water Vapor, lower level", true}}) {
                auto x = make(id, label);
                x.wants = {want("s", ("var discipline=3 center=7 local_table=1 parmcat=192 parm=" + std::to_string(band)).c_str(), "top of atmosphere")};
                x.fill = [] (const Grids& g) { return pick(g, "s"); };
                x.ramp = vapor ? waterVapor() : satelliteIr();
                x.fillTitle = "Brightness temperature (K)";
                x.legendStep = 10;
                p.push_back(x);
            }
            {
                auto x = make("rain_total", "Rainfall since the start of the run and MSLP");
                x.wants = {GfsData::Want{"a", "APCP", "surface", "0-*", ""}, want("p", "PRMSL", "mean sea level")};
                x.fill = [] (const Grids& g) { return pick(g, "a"); };
                x.ramp = precipitation();
                x.quantity = Quantity::Millimeters;
                x.fillTitle = "Rainfall since the start of the run";
                x.legendStep = 0;
                x.contours = {hurricaneLines()};
                p.push_back(x);
            }
            {
                auto x = make("sst", "Sea Surface Temperature and MSLP");
                x.wants = {want("t", "WTMP", "surface"), want("p", "PRMSL", "mean sea level")};
                x.fill = [] (const Grids& g) { return pick(g, "t"); };
                x.ramp = seaSurfaceTemperature();
                x.quantity = Quantity::Temperature;
                x.fillTitle = "Sea surface temperature";
                x.legendStep = 2;
                x.contours = {hurricaneLines()};
                p.push_back(x);
            }
            for (const char * id : {"shear_850_200", "850_vort_ht"}) {
                for (const auto& candidate : p) {
                    if (candidate.id == id && candidate.source == "GFS") {
                        auto x = candidate;
                        x.source = model;
                        p.push_back(std::move(x));
                        break;
                    }
                }
            }
            {   // the wave file holds every hour: the record says which ("anl", then "3 hour fcst" ...)
                auto x = make("waves", "Significant Wave Height, Peak Period and Wind");
                x.needs = [] (int hour) {
                    const std::string when = hour == 0 ? "anl" : std::to_string(hour) + " hour fcst";
                    return std::vector<GfsData::Need>{{hour, {"h", "HTSGW", "surface", when, "", "ww3"}}, {hour, {"t", "PERPW", "surface", when, "", "ww3"}}, {hour, {"u", "UGRD", "surface", when, "", "ww3"}},
                                                      {hour, {"v", "VGRD", "surface", when, "", "ww3"}}};
                };
                x.fill = [] (const Grids& g) { return pick(g, "h"); };
                x.ramp = waveHeight();
                x.fillTitle = "Significant wave height (m)";
                x.legendStep = 0;
                ContourSet period;
                period.key = "t";
                period.interval = 2;
                period.title = "Peak period (s)";
                period.color = QColor{255, 255, 255};
                x.contours = {period};
                x.barbU = "u";
                x.barbV = "v";
                p.push_back(x);
            }
        }
        return p;
    }();
    return list;
}

const Product * product(const std::string& id, const std::string& source) {
    for (const auto& p : products()) {
        if (p.id == id && p.source == source) {
            return &p;
        }
    }
    return nullptr;
}

std::string sourceLabel(const std::string& source) {
    return source == "NBM" ? "NOAA/NWS National Blend of Models v4, 2.5 km" : source == "AIGFS" ? "NOAA/NCEP AIGFS 0.25 degree (an AI model; experimental)" : source == "GEFS" ? "NOAA/NCEP GEFS 30 member ensemble, 0.5 degree" : source == "HAFSA" ? "NOAA/NCEP HAFS-A, 2 km storm-following grid" : source == "HAFSB" ? "NOAA/NCEP HAFS-B, 2 km storm-following grid" : "NOAA/NCEP GFS 0.25 degree";
}

std::vector<std::string> sectorIds(const std::string& source) {
    if (source == "NBM") {
        return {"CONUS", "NORTHEAST", "MID-ATLANTIC", "SOUTHEAST", "GREAT-LAKES", "OHIO-VALLEY", "S-PLAINS", "N-PLAINS", "ROCKIES", "SOUTHWEST", "PACIFIC-NW", "CALIFORNIA", "GULF-COAST"};
    }
    std::vector<std::string> all;
    for (const auto& s : sectors()) {
        all.push_back(s.id);
    }
    return all;
}

std::vector<GfsData::Need> needs(const Product& product, int hour) {
    if (product.needs) {
        return product.needs(hour);
    }
    std::vector<GfsData::Need> list;
    for (const auto& w : product.wants) {
        list.push_back({hour, w});
    }
    return list;
}


// ---- ticked onto a chart: lines and barbs
namespace {
    struct Overlay {
        std::string id;
        std::string label;
        std::string group;
        std::vector<std::string> sources;
        std::function<std::vector<GfsData::Need>(int hour)> needs;      // keys are the overlay's own; compose() prefixes them
        std::function<void(Grids&, const Context&)> derive;             // on the overlay's own keys
        bool barbs{false};
        ContourSet contour;
        std::string barbU, barbV;
    };

    std::string overlayPrefix(const Overlay& o) {
        return "o_" + o.id + "_";
    }

    const std::vector<Overlay>& overlayCatalog() {
        static const std::vector<Overlay> all = [] {
            std::vector<Overlay> list;
            const auto record = [] (const char * key, const char * variable, const char * level) {
                return [=] (int hour) { return GfsData::Need{hour, {key, variable, level, "", ""}}; };
            };
            {   // sea level pressure
                Overlay o;
                o.id = "mslp";
                o.label = "Sea level pressure";
                o.group = "Lines";
                o.sources = {"GFS", "AIGFS", "GEFS"};
                o.needs = [=] (int hour) { return std::vector<GfsData::Need>{record("p", "PRMSL", "mean sea level")(hour)}; };
                o.contour.key = "p";
                o.contour.scale = 0.01;
                o.contour.interval = 4;
                o.contour.title = "Sea level pressure (mb)";
                o.contour.highsAndLows = true;
                list.push_back(o);
            }
            for (const auto& [id, label, from, to, interval, edge] : {std::tuple{"thick_1000_500", "1000-500mb thickness", "1000 mb", "500 mb", 6.0, 540.0}, {"thick_1000_850", "1000-850mb thickness", "1000 mb", "850 mb", 3.0, 130.0},
                                                                         {"thick_850_700", "850-700mb thickness", "850 mb", "700 mb", 3.0, 154.0}}) {
                Overlay o;
                o.id = id;
                o.label = label;
                o.group = "Lines";
                o.sources = {"GFS", "AIGFS", "GEFS"};
                o.needs = [=] (int hour) { return std::vector<GfsData::Need>{record("zl", "HGT", from)(hour), record("zh", "HGT", to)(hour)}; };
                o.derive = [] (Grids& g, const Context&) { g["thick"] = GfsGrid::difference(g["zh"], g["zl"]); };
                o.contour.key = "thick";
                o.contour.scale = 0.1;
                o.contour.interval = interval;
                o.contour.title = std::string{label} + " (dam)";
                o.contour.color = QColor{190, 50, 40};
                o.contour.colorBelow = QColor{40, 90, 190};
                o.contour.split = edge;
                o.contour.dashed = true;
                o.contour.width = 1.3;
                list.push_back(o);
            }
            for (const auto& [level, interval] : {std::pair{"200", 12.0}, {"250", 12.0}, {"300", 12.0}, {"500", 6.0}, {"700", 3.0}, {"850", 3.0}}) {
                Overlay o;
                o.id = std::string{"z"} + level;
                o.label = std::string{level} + "mb height";
                o.group = "Lines";
                o.sources = {"GFS", "AIGFS", "GEFS"};
                const std::string levelText = std::string{level} + " mb";
                o.needs = [levelText] (int hour) { return std::vector<GfsData::Need>{{hour, {"z", "HGT", levelText, "", ""}}}; };
                o.contour.key = "z";
                o.contour.scale = 0.1;
                o.contour.interval = interval;
                o.contour.title = std::string{level} + "mb height (dam)";
                o.contour.color = QColor{70, 70, 70};
                list.push_back(o);
            }
            {   // the 850 mb temperature, blue under freezing and red over it
                Overlay o;
                o.id = "t850";
                o.label = "850mb temperature";
                o.group = "Lines";
                o.sources = {"GFS", "AIGFS", "GEFS"};
                o.needs = [] (int hour) { return std::vector<GfsData::Need>{{hour, {"t", "TMP", "850 mb", "", ""}}}; };
                o.contour.key = "t";
                o.contour.interval = 5;
                o.contour.title = "850mb temperature (C)";
                o.contour.color = QColor{190, 50, 40};
                o.contour.colorBelow = QColor{40, 90, 190};
                o.contour.split = 0.0;
                o.contour.dashed = true;
                o.contour.width = 1.3;
                list.push_back(o);
            }
            // wind barbs: the 10 m wind, and the wind of the main levels
            for (const auto& [id, label, level] : {std::tuple{"barbs_10m", "10 m wind", "10 m above ground"}, {"barbs_850", "850mb wind", "850 mb"}, {"barbs_700", "700mb wind", "700 mb"},
                                                    {"barbs_500", "500mb wind", "500 mb"}, {"barbs_300", "300mb wind", "300 mb"}, {"barbs_250", "250mb wind", "250 mb"}, {"barbs_200", "200mb wind", "200 mb"}}) {
                Overlay o;
                o.id = id;
                o.label = label;
                o.group = "Wind barbs";
                o.sources = {"GFS", "AIGFS", "GEFS"};
                o.barbs = true;
                const std::string levelText = level;
                o.needs = [levelText] (int hour) { return std::vector<GfsData::Need>{{hour, {"u", "UGRD", levelText, "", ""}}, {hour, {"v", "VGRD", levelText, "", ""}}}; };
                o.barbU = "u";
                o.barbV = "v";
                list.push_back(o);
            }
            {   // the blend gives the 10 m wind as a speed and a direction
                Overlay o;
                o.id = "barbs_10m";
                o.label = "10 m wind";
                o.group = "Wind barbs";
                o.sources = {"NBM"};
                o.barbs = true;
                o.needs = [] (int hour) {
                    const auto when = std::to_string(hour) + " hour fcst";
                    return std::vector<GfsData::Need>{{hour, {"ws", "WIND", "10 m above ground", when, ""}}, {hour, {"wd", "WDIR", "10 m above ground", when, ""}}};
                };
                o.derive = [] (Grids& g, const Context&) {
                    auto u = g["ws"], v = g["ws"];
                    for (size_t i = 0; i < u.values.size(); i++) {
                        const double speedNow = g["ws"].values[i], from = g["wd"].values[i] * pi / 180.0;
                        u.values[i] = static_cast<float>(-speedNow * std::sin(from));
                        v.values[i] = static_cast<float>(-speedNow * std::cos(from));
                    }
                    g["u"] = std::move(u);
                    g["v"] = std::move(v);
                };
                o.barbU = "u";
                o.barbV = "v";
                list.push_back(o);
            }
            return list;
        }();
        return all;
    }

    const Overlay * overlayNamed(const std::string& id, const std::string& source) {
        for (const auto& o : overlayCatalog()) {
            if (o.id == id && std::find(o.sources.begin(), o.sources.end(), source) != o.sources.end()) {
                return &o;
            }
        }
        return nullptr;
    }
}

std::vector<OverlayChoice> overlayChoices(const std::string& source) {
    std::vector<OverlayChoice> out;
    for (const auto& o : overlayCatalog()) {
        if (std::find(o.sources.begin(), o.sources.end(), source) != o.sources.end()) {
            out.push_back({o.id, o.label, o.group});
        }
    }
    return out;
}

Product compose(const Product& base, const std::vector<std::string>& ids) {
    std::vector<const Overlay *> used;
    for (const auto& id : ids) {
        const auto * o = overlayNamed(id, base.source);
        if (o == nullptr || std::find(used.begin(), used.end(), o) != used.end()) {
            continue;
        }
        // a line the base chart already draws is not drawn twice
        const bool already = !o->barbs && std::any_of(base.contours.begin(), base.contours.end(), [o] (const ContourSet& c) { return c.title == o->contour.title; });
        if (!already) {
            used.push_back(o);
        }
    }
    if (used.empty()) {
        return base;
    }
    Product p = base;
    const auto add = [used] (std::vector<GfsData::Need> needs, int hour) {
        for (const auto * o : used) {
            for (auto need : o->needs(hour)) {
                need.want.key = overlayPrefix(*o) + need.want.key;
                needs.push_back(std::move(need));
            }
        }
        return needs;
    };
    p.needs = [base, add] (int hour) { return add(GfsChart::needs(base, hour), hour); };
    if (base.fallbackNeeds) {
        p.fallbackNeeds = [base, add] (int hour) {
            auto needs = base.fallbackNeeds(hour);
            return needs.empty() ? needs : add(std::move(needs), hour);
        };
    }
    p.derive = [base, used] (Grids& g, const Context& context) {
        if (base.derive) {
            base.derive(g, context);
        }
        for (const auto * o : used) {
            if (!o->derive) {
                continue;
            }
            const auto prefix = overlayPrefix(*o);
            Grids own;   // the overlay's keys without the prefix, so its derivation is written as if it were alone
            for (const auto& [key, grid] : g) {
                if (key.compare(0, prefix.size(), prefix) == 0) {
                    own[key.substr(prefix.size())] = grid;
                }
            }
            o->derive(own, context);
            for (auto& [key, grid] : own) {
                g[prefix + key] = std::move(grid);
            }
        }
    };
    std::string idText = base.id, labelText = base.label;
    for (const auto * o : used) {
        const auto prefix = overlayPrefix(*o);
        idText += "+" + o->id;
        labelText += (o == used.front() ? ", with " : ", ") + o->label;
        if (o->barbs) {
            p.barbU = prefix + o->barbU;
            p.barbV = prefix + o->barbV;
        } else {
            auto set = o->contour;
            set.key = prefix + set.key;
            p.contours.push_back(std::move(set));
        }
    }
    p.id = idText;
    p.label = labelText;
    return p;
}

std::vector<GfsData::Need> fallbackNeeds(const Product& product, int hour) {
    return product.fallbackNeeds ? product.fallbackNeeds(hour) : std::vector<GfsData::Need>{};
}

namespace {
    // a number shown in the user's units
    double shown(Quantity quantity, double value, bool us) {
        if (!us) {
            return value;
        }
        switch (quantity) {
            case Quantity::Temperature: return value * 1.8 + 32.0;
            case Quantity::Millimeters: return value / 25.4;
            case Quantity::Centimeters: return value / 2.54;
            default: return value;
        }
    }
    QString unitText(Quantity quantity, bool us) {
        switch (quantity) {
            case Quantity::Temperature: return us ? "°F" : "°C";
            case Quantity::Millimeters: return us ? "in" : "mm";
            case Quantity::Centimeters: return us ? "in" : "cm";
            default: return {};
        }
    }
    QString numberText(double value) {
        const double a = std::abs(value);
        return QString::number(value, 'f', a >= 10 || a == std::floor(a) ? 0 : a >= 1 ? 1 : 2);
    }
}

QImage render(const Product& product, const Sector& sector, const Grids& fetched, const GfsData::Run& run, int forecastHour, const Options& options) {
    Grids grids = fetched;
    if (product.derive) {
        product.derive(grids, Context{forecastHour, run, options.climate});
    }
    const auto fill = product.fill(grids);
    if (fill.empty()) {
        return {};
    }
    for (const auto& set : product.contours) {
        if (grids.find(set.key) == grids.end()) {
            return {};
        }
    }
    const bool us = options.fahrenheit;
    const int headerHeight = 54, legendHeight = 62, margin = 8;
    const QRectF mapArea{static_cast<double>(margin), static_cast<double>(headerHeight), static_cast<double>(options.width - 2 * margin), 0.0};
    const double mapHeight = std::clamp(View::heightFor(sector, mapArea.width()), 200.0, 1400.0);
    const QRectF area{mapArea.left(), mapArea.top(), mapArea.width(), mapHeight};
    View view{sector, area};
    QImage image(options.width, static_cast<int>(headerHeight + mapHeight + legendHeight), QImage::Format_ARGB32_Premultiplied);
    image.fill(QColor{250, 250, 250});

    // the fill, a pixel at a time (the ramp is in the grid's units; fully clear parts show the pale ground)
    const Ramp * ramp = &product.ramp;
    QImage map(static_cast<int>(area.width()), static_cast<int>(area.height()), QImage::Format_ARGB32_Premultiplied);
    map.fill(QColor{244, 244, 244});
    for (int y = 0; y < map.height(); y++) {
        auto * row = reinterpret_cast<QRgb *>(map.scanLine(y));
        const double lat = view.latAt(area.top() + y + 0.5);
        for (int x = 0; x < map.width(); x++) {
            const float value = fill.sample(view.lonAt(area.left() + x + 0.5), lat);
            if (!std::isnan(value)) {
                const QRgb c = ramp->at(value);
                const int a = qAlpha(c);
                if (a == 255) {
                    row[x] = qPremultiply(c);
                } else if (a > 0) {   // a ramp that fades in over the ground
                    const QRgb under = row[x];
                    row[x] = qRgb((qRed(c) * a + qRed(under) * (255 - a)) / 255, (qGreen(c) * a + qGreen(under) * (255 - a)) / 255, (qBlue(c) * a + qBlue(under) * (255 - a)) / 255);
                }
            }
        }
    }
    for (const auto& [key, overlayRamp] : product.overlays) {
        const auto found = grids.find(key);
        if (found == grids.end()) {
            continue;
        }
        for (int y = 0; y < map.height(); y++) {
            auto * row = reinterpret_cast<QRgb *>(map.scanLine(y));
            const double lat = view.latAt(area.top() + y + 0.5);
            for (int x = 0; x < map.width(); x++) {
                const float value = found->second.sample(view.lonAt(area.left() + x + 0.5), lat);
                if (std::isnan(value)) {
                    continue;
                }
                const QRgb c = overlayRamp.at(value);
                const int a = qAlpha(c);
                if (a > 0) {
                    const QRgb under = qUnpremultiply(row[x]);
                    row[x] = qRgb((qRed(c) * a + qRed(under) * (255 - a)) / 255, (qGreen(c) * a + qGreen(under) * (255 - a)) / 255, (qBlue(c) * a + qBlue(under) * (255 - a)) / 255);
                }
            }
        }
    }
    QPainter p{&image};
    p.setRenderHint(QPainter::Antialiasing);
    p.drawImage(area.topLeft(), map);
    p.setClipRect(area);

    // coastlines and borders
    p.setPen(QPen{QColor{40, 40, 40, 210}, 0.9});
    for (const auto& line : options.lines) {
        QPainterPath path;
        bool started = false;
        for (const auto& pt : line) {
            if (pt.second < sector.south - 3 || pt.second > sector.north + 3) {
                started = false;
                continue;
            }
            // a longitude equivalent to the view's: the lines are -180..180, the view may reach past either
            double lon = pt.first;
            while (lon < sector.west - 25.0) {
                lon += 360.0;
            }
            while (lon >= sector.west + 335.0) {
                lon -= 360.0;
            }
            if (lon > sector.east + 25.0) {   // far off the right edge (the far side of the dateline, say): a line to it would run across the chart
                started = false;
                continue;
            }
            const auto at = view.toPixel(lon, pt.second);
            started ? path.lineTo(at) : path.moveTo(at);
            started = true;
        }
        p.drawPath(path);
    }

    QFont font = p.font();
    font.setPixelSize(11);
    font.setBold(true);
    p.setFont(font);
    const QFontMetricsF metrics{font};
    const auto halo = [&] (const QPointF& at, const QString& text, const QColor& color) {   // a label with a light edge so it reads over the colors
        const QPointF origin{at.x() - metrics.horizontalAdvance(text) / 2.0, at.y() + metrics.ascent() / 2.0 - 1};
        QPainterPath path;
        path.addText(origin, font, text);
        p.setPen(QPen{QColor{255, 255, 255, 220}, 3.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin});
        p.setBrush(Qt::NoBrush);
        p.drawPath(path);
        p.setPen(Qt::NoPen);
        p.setBrush(color);
        p.drawPath(path);
    };

    // streamlines: evenly spaced lines along the wind (the conformal map puts the wind's direction on the screen as it is: east to the right, north up)
    if (!product.streamU.empty() && grids.count(product.streamU) && grids.count(product.streamV)) {
        const auto& su = grids.at(product.streamU);
        const auto& sv = grids.at(product.streamV);
        const double separation = 17.0, testDistance = 8.0, step = 2.5;
        struct Spot {
            QPointF at;
        };
        const double cell = testDistance;
        const int cols = static_cast<int>(area.width() / cell) + 2, rowsN = static_cast<int>(area.height() / cell) + 2;
        std::vector<std::vector<QPointF>> hash(static_cast<size_t>(cols) * static_cast<size_t>(rowsN));
        const auto slot = [&] (const QPointF& pt) { return static_cast<size_t>(static_cast<int>((pt.y() - area.top()) / cell)) * static_cast<size_t>(cols) + static_cast<size_t>(static_cast<int>((pt.x() - area.left()) / cell)); };
        const auto tooClose = [&] (const QPointF& pt, double distance) {
            const int cx = static_cast<int>((pt.x() - area.left()) / cell), cy = static_cast<int>((pt.y() - area.top()) / cell);
            const int reach = static_cast<int>(std::ceil(distance / cell));
            for (int dy = -reach; dy <= reach; dy++) {
                for (int dx = -reach; dx <= reach; dx++) {
                    const int x = cx + dx, y = cy + dy;
                    if (x < 0 || y < 0 || x >= cols || y >= rowsN) {
                        continue;
                    }
                    for (const auto& other : hash[static_cast<size_t>(y) * static_cast<size_t>(cols) + static_cast<size_t>(x)]) {
                        if (std::hypot(other.x() - pt.x(), other.y() - pt.y()) < distance) {
                            return true;
                        }
                    }
                }
            }
            return false;
        };
        const auto wind = [&] (const QPointF& pt, double& ux, double& vy) {   // the unit direction on the screen, false where there is no wind
            const double lon = view.lonAt(pt.x()), lat = view.latAt(pt.y());
            const float uu = su.sample(lon, lat), vv = sv.sample(lon, lat);
            const double speedNow = std::hypot(uu, vv);
            if (std::isnan(uu) || std::isnan(vv) || speedNow < 0.3) {
                return false;
            }
            ux = uu / speedNow;
            vy = -vv / speedNow;
            return true;
        };
        const auto trace = [&] (QPointF from, double sign, std::vector<QPointF>& out) {
            QPointF at = from;
            for (int i = 0; i < 500; i++) {
                double ux, vy;
                if (!wind(at, ux, vy)) {
                    break;
                }
                const QPointF mid = at + QPointF{ux, vy} * (sign * step / 2.0);
                double mx, my;
                if (!wind(mid, mx, my)) {
                    break;
                }
                at += QPointF{mx, my} * (sign * step);
                if (!area.contains(at) || tooClose(at, testDistance)) {
                    break;
                }
                out.push_back(at);
            }
        };
        p.setBrush(Qt::NoBrush);
        const QColor lineColor{25, 25, 25, 215};
        for (double y = area.top() + separation / 2; y < area.bottom(); y += separation) {
            for (double x = area.left() + separation / 2; x < area.right(); x += separation) {
                const QPointF seed{x, y};
                if (tooClose(seed, separation * 0.8)) {
                    continue;
                }
                std::vector<QPointF> forward, backward;
                trace(seed, 1.0, forward);
                trace(seed, -1.0, backward);
                if (forward.size() + backward.size() < 8) {
                    continue;
                }
                std::vector<QPointF> line(backward.rbegin(), backward.rend());
                line.push_back(seed);
                line.insert(line.end(), forward.begin(), forward.end());
                QPainterPath path;
                path.moveTo(line.front());
                for (const auto& pt : line) {
                    path.lineTo(pt);
                    hash[slot(pt)].push_back(pt);
                }
                p.setPen(QPen{lineColor, 1.1});
                p.drawPath(path);
                // an arrowhead at the middle
                const size_t m = line.size() / 2;
                if (m > 2 && m + 2 < line.size()) {
                    const QPointF dir = line[m + 2] - line[m - 2];
                    const double length = std::hypot(dir.x(), dir.y());
                    if (length > 0.1) {
                        const QPointF unit = dir / length, normal{-unit.y(), unit.x()};
                        QPolygonF head;
                        head << line[m] + unit * 4.0 << line[m] - unit * 3.0 + normal * 3.2 << line[m] - unit * 3.0 - normal * 3.2;
                        p.setBrush(lineColor);
                        p.setPen(Qt::NoPen);
                        p.drawPolygon(head);
                        p.setBrush(Qt::NoBrush);
                    }
                }
            }
        }
    }

    // the wider the view, the sparser the lines, the highs and lows, and the barbs
    const double span = sector.east - sector.west;
    const double extremeRadius = span > 150.0 ? 12.0 : span > 80.0 ? 8.0 : 6.0;
    for (const auto& set : product.contours) {
        const double interval = set.interval * (span > 150.0 ? 2.0 : 1.0);
        const auto& grid = grids.at(set.key);
        double lo = 1e18, hi = -1e18;
        for (int y = 0; y < map.height(); y += 6) {
            for (int x = 0; x < map.width(); x += 6) {
                const float value = grid.sample(view.lonAt(area.left() + x), view.latAt(area.top() + y));
                if (!std::isnan(value)) {
                    lo = std::min<double>(lo, value * set.scale);
                    hi = std::max<double>(hi, value * set.scale);
                }
            }
        }
        if (set.onlyBelowZero) {
            hi = std::min(hi, -interval / 2.0);
        }
        for (double level = std::ceil((lo - set.base) / interval) * interval + set.base; level <= hi; level += interval) {
            const bool heavy = std::fmod(std::abs(level - set.base), interval * 5) < 1e-6 || (set.colorBelow.isValid() && std::abs(level - set.split) < 1e-6);
            const QColor color = set.colorBelow.isValid() && level <= set.split ? set.colorBelow : set.color;
            for (const auto& line : GfsGrid::contour(grid, level / set.scale, sector.west, sector.south, sector.east, sector.north)) {
                QPainterPath path;
                bool first = true;
                for (const auto& pt : line) {
                    const auto at = view.toPixel(pt.lon, pt.lat);   // already continuous across west..east
                    first ? path.moveTo(at) : path.lineTo(at);
                    first = false;
                }
                p.setBrush(Qt::NoBrush);
                QPen pen{QColor{color.red(), color.green(), color.blue(), 230}, (heavy ? 1.6 : 1.0) * set.width};
                if (set.dashed) {
                    pen.setStyle(Qt::DashLine);
                }
                p.setPen(pen);
                p.drawPath(path);
                // one label near the middle of a line long enough to carry it
                if (path.length() > 150.0) {
                    const auto mid = path.pointAtPercent(0.5);
                    if (area.adjusted(24, 14, -24, -14).contains(mid)) {
                        halo(mid, QString::number(static_cast<int>(std::lround(level))), color.darker(130));
                    }
                }
            }
        }
        if (set.highsAndLows) {
            for (const auto& e : GfsGrid::extremes(grid, extremeRadius, sector.west, sector.south, sector.east, sector.north)) {
                const auto at = view.toPixel(e.lon, e.lat);
                if (area.adjusted(20, 20, -20, -20).contains(at)) {
                    halo(at, e.high ? "H" : "L", e.high ? QColor{20, 60, 170} : QColor{190, 30, 30});
                    halo(at + QPointF{0, 13}, QString::number(static_cast<int>(std::lround(e.value * set.scale))), QColor{40, 40, 40});
                }
            }
        }
    }

    // wind barbs on a grid of pixels
    if (!product.barbU.empty() && grids.count(product.barbU) && grids.count(product.barbV)) {
        const auto& u = grids.at(product.barbU);
        const auto& v = grids.at(product.barbV);
        const double spacing = span > 150.0 ? 62.0 : span > 80.0 ? 52.0 : 48.0;
        p.setPen(QPen{QColor{20, 20, 20, 230}, 1.0});
        p.setBrush(QColor{20, 20, 20});
        for (double y = area.top() + spacing / 2; y < area.bottom(); y += spacing) {
            for (double x = area.left() + spacing / 2; x < area.right(); x += spacing) {
                const double lon = view.lonAt(x), lat = view.latAt(y);
                const float uu = u.sample(lon, lat), vv = v.sample(lon, lat);
                if (std::isnan(uu) || std::isnan(vv)) {
                    continue;
                }
                const double from = std::fmod(std::atan2(-uu, -vv) * 180.0 / pi + 360.0, 360.0);
                WindBarb::draw(p, {x, y}, from, std::hypot(uu, vv) * msToKnots, 24.0, lat < 0.0);
            }
        }
    }
    p.setClipping(false);
    p.setPen(QPen{QColor{60, 60, 60}, 1.0});
    p.setBrush(Qt::NoBrush);
    p.drawRect(area);

    // header
    const auto init = QDateTime::fromString(QString::fromStdString(run.id()), "yyyyMMddHH");
    auto initUtc = QDateTime{init.date(), init.time(), Qt::UTC};
    const auto valid = initUtc.addSecs(forecastHour * 3600);
    QFont big = font;
    big.setPixelSize(17);
    p.setFont(big);
    p.setPen(QColor{25, 25, 25});
    p.drawText(QPointF{static_cast<double>(margin), 22.0}, QString::fromStdString(product.source + "  " + product.label));
    font.setBold(false);
    p.setFont(font);
    p.setPen(QColor{70, 70, 70});
    const auto unit = unitText(product.quantity, us);
    QString fillTitle = QString::fromStdString(product.fillTitleFor ? product.fillTitleFor(forecastHour) : product.fillTitle);
    if (!unit.isEmpty()) {
        fillTitle += " (" + unit + ")";
    }
    QStringList lines;
    for (const auto& set : product.contours) {
        if (!set.title.empty()) {
            lines << QString::fromStdString(set.title);
        }
    }
    p.drawText(QPointF{static_cast<double>(margin), 43.0}, fillTitle + (lines.isEmpty() ? "" : ";  lines: " + lines.join(", ")) + (product.barbU.empty() ? "" : ";  barbs: kt"));
    const QString times = "Run " + initUtc.toString("ddd yyyy-MM-dd HH") + "Z   F" + QString::number(forecastHour).rightJustified(3, '0') + "   Valid " + valid.toString("ddd yyyy-MM-dd HH") + "Z";
    p.drawText(QRectF{0, 8, image.width() - static_cast<double>(margin), 20}, Qt::AlignRight, times);

    // legend: even steps in value, or (legendStep 0) one equal-width block per ramp stop
    const double barLeft = margin + 6.0, barTop = area.bottom() + 12.0, barWidth = area.width() - 12.0, barHeight = 16.0;
    const auto& stops = ramp->stops;
    const double first = stops.front().first, last = stops.back().first;
    const bool stepped = product.legendStep <= 0.0;
    const auto valueAt = [&] (double t) {   // t 0..1 along the bar
        if (!stepped) {
            return first + (last - first) * t;
        }
        const double position = t * static_cast<double>(stops.size() - 1);
        const size_t i = std::min(static_cast<size_t>(position), stops.size() - 2);
        return stops[i].first + (stops[i + 1].first - stops[i].first) * (position - static_cast<double>(i));
    };
    for (int x = 0; x < static_cast<int>(barWidth); x++) {
        const QRgb c = ramp->at(valueAt(x / barWidth));
        // a ramp that begins clear is drawn as the pale ground on the bar
        const int a = qAlpha(c);
        const QColor ground{244, 244, 244};
        p.fillRect(QRectF{barLeft + x, barTop, 1.5, barHeight}, QColor{(qRed(c) * a + ground.red() * (255 - a)) / 255, (qGreen(c) * a + ground.green() * (255 - a)) / 255, (qBlue(c) * a + ground.blue() * (255 - a)) / 255});
    }
    p.setPen(QColor{60, 60, 60});
    p.setBrush(Qt::NoBrush);
    p.drawRect(QRectF{barLeft, barTop, barWidth, barHeight});
    const auto tick = [&] (double t, double value) {
        const double x = barLeft + t * barWidth;
        p.drawLine(QPointF{x, barTop + barHeight}, QPointF{x, barTop + barHeight + 4});
        p.drawText(QRectF{x - 24, barTop + barHeight + 4, 48, 14}, Qt::AlignHCenter, numberText(shown(product.quantity, value, us)));
    };
    if (stepped) {
        for (size_t i = 0; i < stops.size(); i++) {
            if (i > 0 || stops[i].second.alpha() == 255) {
                tick(static_cast<double>(i) / static_cast<double>(stops.size() - 1), stops[i].first);
            }
        }
    } else {
        // steps are in displayed units: walk them in displayed value and map back
        const double lowShown = shown(product.quantity, first, us), highShown = shown(product.quantity, last, us);
        for (double value = std::ceil(lowShown / product.legendStep) * product.legendStep; value <= highShown + 1e-9; value += product.legendStep) {
            const double base = (value - lowShown) / (highShown - lowShown);
            tick(base, first + (last - first) * base);
        }
    }
    // a small key for each overlay's colors (the same amounts as the main scale, from light to heavy)
    double keyLeft = barLeft;
    for (const auto& [key, overlayRamp] : product.overlays) {
        const QString name = key == "snow" ? "Snow" : key == "mix" ? "Mixed" : QString::fromStdString(key);
        const double top = image.height() - 15.0, width = 80.0;
        p.setPen(QColor{70, 70, 70});
        p.drawText(QRectF{keyLeft, top - 1, 44, 12}, Qt::AlignLeft | Qt::AlignVCenter, name);
        const auto& s = overlayRamp.stops;
        for (int x = 0; x < static_cast<int>(width); x++) {
            const QRgb c = overlayRamp.at(s[1].first + (s.back().first - s[1].first) * x / width);
            p.fillRect(QRectF{keyLeft + 44 + x, top, 1.5, 9}, QColor{qRed(c), qGreen(c), qBlue(c)});
        }
        p.setPen(QColor{60, 60, 60});
        p.setBrush(Qt::NoBrush);
        p.drawRect(QRectF{keyLeft + 44, top, width, 9});
        keyLeft += 44 + width + 22;
    }
    p.setPen(QColor{110, 110, 110});
    p.drawText(QRectF{0, image.height() - 16.0, image.width() - static_cast<double>(margin), 14}, Qt::AlignRight, QString::fromStdString("Data: " + sourceLabel(product.source)));
    return image;
}
}
