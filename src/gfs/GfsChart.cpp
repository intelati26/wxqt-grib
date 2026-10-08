// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "gfs/GfsChart.h"
#include <algorithm>
#include <cmath>
#include <QDateTime>
#include <QFontMetricsF>
#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>
#include <QStringList>
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
        return p;
    }();
    return list;
}

const Product * product(const std::string& id) {
    for (const auto& p : products()) {
        if (p.id == id) {
            return &p;
        }
    }
    return nullptr;
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
    p.drawText(QPointF{static_cast<double>(margin), 22.0}, QString::fromStdString("GFS  " + product.label));
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
    p.drawText(QRectF{0, image.height() - 16.0, image.width() - static_cast<double>(margin), 14}, Qt::AlignRight, "Data: NOAA/NCEP GFS 0.25 degree");
    return image;
}
}
