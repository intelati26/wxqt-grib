// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "gfs/GfsChart.h"
#include <algorithm>
#include <cctype>
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
#include "gfs/GfsModels.h"
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
    Ramp helicityRamp() {   // m2/s2: storm-relative helicity, the 150 and 300 steps are where rotation starts to matter
        return {{{0, QColor{240, 244, 250, 0}}, {50, QColor{"#d6e6f2"}}, {100, QColor{"#9fd0e0"}}, {150, QColor{"#7bc47f"}}, {250, QColor{"#f3e04a"}}, {300, QColor{"#f0a63a"}}, {400, QColor{"#e0602e"}},
                 {500, QColor{"#c4262c"}}, {700, QColor{"#a8206b"}}, {1000, QColor{"#6a2a9a"}}}};
    }
    Ramp updraftHelicity() {   // m2/s2: rotating updrafts, from 25 (weak) to 200 and more (a strong supercell)
        return {{{0, QColor{240, 244, 250, 0}}, {10, QColor{240, 244, 250, 0}}, {25, QColor{"#c4e3a4"}}, {50, QColor{"#7bc47f"}}, {75, QColor{"#f3e04a"}}, {100, QColor{"#f0a63a"}}, {150, QColor{"#e0602e"}},
                 {200, QColor{"#c4262c"}}, {300, QColor{"#a8206b"}}, {500, QColor{"#6a2a9a"}}}};
    }
    Ramp echoTopRamp() {   // thousands of feet
        return {{{0, QColor{240, 244, 250, 0}}, {5, QColor{240, 244, 250, 0}}, {10, QColor{"#cfe8f3"}}, {20, QColor{"#8ccbe0"}}, {30, QColor{"#55a868"}}, {35, QColor{"#e3d44a"}}, {40, QColor{"#f0a63a"}},
                 {45, QColor{"#e0702e"}}, {50, QColor{"#cc3d2c"}}, {55, QColor{"#a8206b"}}, {60, QColor{"#7a2a9a"}}}};
    }
    Ramp ceilingRamp() {   // feet: the flight category steps (500 LIFR, 1000 IFR, 3000 MVFR) in the strong colors
        return {{{0, QColor{"#b5368f"}}, {500, QColor{"#d9435f"}}, {1000, QColor{"#ee7a47"}}, {2000, QColor{"#f6b24e"}}, {3000, QColor{"#e3d44a"}}, {5000, QColor{"#a9d98a"}}, {10000, QColor{"#cfe8f3"}}, {20000, QColor{"#f4f8fb"}}}};
    }
    Ramp visibilityRamp() {   // statute miles: 1 and 3 are the IFR and MVFR steps
        return {{{0, QColor{"#b5368f"}}, {0.5, QColor{"#d9435f"}}, {1, QColor{"#ee7a47"}}, {3, QColor{"#f6b24e"}}, {5, QColor{"#e3d44a"}}, {7, QColor{"#a9d98a"}}, {10, QColor{"#f4f8fb"}}}};
    }
    Ramp lightningRamp() {   // flashes: any lightning is yellow, a lot is purple
        return {{{0, QColor{240, 244, 250, 0}}, {0.1, QColor{255, 240, 120, 150}}, {1, QColor{"#ffd34a"}}, {3, QColor{"#f09a2e"}}, {6, QColor{"#e0502e"}}, {12, QColor{"#b02060"}}, {25, QColor{"#6a2a9a"}}}};
    }
    Ramp wavePeriod() {   // seconds: short choppy seas pale, long swell in deep colors
        return {{{0, QColor{240, 244, 250}}, {4, QColor{"#cfe8f3"}}, {6, QColor{"#8ccbe0"}}, {8, QColor{"#55b6a8"}}, {10, QColor{"#7bc47f"}}, {12, QColor{"#e3d44a"}}, {14, QColor{"#f0a63a"}}, {16, QColor{"#e0602e"}},
                 {18, QColor{"#c4262c"}}, {22, QColor{"#6a2a9a"}}}};
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

Sector cropToTrack(const Sector& within, const std::vector<TrackPoint>& track, int hour, double margin) {
    double west = 1e9, east = -1e9, south = 1e9, north = -1e9;
    for (const auto& t : track) {
        if (t.hour <= hour) {
            west = std::min(west, t.lon);
            east = std::max(east, t.lon);
            south = std::min(south, t.lat);
            north = std::max(north, t.lat);
        }
    }
    if (east < west) {
        return within;
    }
    Sector s{within.id, std::max(within.west, west - margin), std::max(within.south, south - margin), std::min(within.east, east + margin), std::min(within.north, north + margin)};
    const double minimum = 8.0;   // never narrower than this, in either direction
    if (s.east - s.west < minimum) {
        const double mid = (s.east + s.west) / 2.0;
        s.west = std::max(within.west, mid - minimum / 2.0);
        s.east = std::min(within.east, mid + minimum / 2.0);
    }
    if (s.north - s.south < minimum) {
        const double mid = (s.north + s.south) / 2.0;
        s.south = std::max(within.south, mid - minimum / 2.0);
        s.north = std::min(within.north, mid + minimum / 2.0);
    }
    return s;
}

std::vector<TrackPoint> parseTrack(const std::string& text) {
    std::vector<TrackPoint> points;
    const auto position = [] (const std::string& s, char negative) {   // "235N" -> 23.5, "906W" -> -90.6
        if (s.size() < 2) {
            return 0.0;
        }
        const double value = std::atof(s.c_str()) / 10.0;
        return s.back() == negative ? -value : value;
    };
    size_t from = 0;
    while (from < text.size()) {
        auto end = text.find('\n', from);
        if (end == std::string::npos) {
            end = text.size();
        }
        const auto line = text.substr(from, end - from);
        from = end + 1;
        std::vector<std::string> f;
        size_t at = 0;
        while (true) {
            const auto comma = line.find(',', at);
            std::string field = line.substr(at, comma == std::string::npos ? std::string::npos : comma - at);
            while (!field.empty() && field.front() == ' ') {
                field.erase(field.begin());
            }
            while (!field.empty() && field.back() == ' ') {
                field.pop_back();
            }
            f.push_back(std::move(field));
            if (comma == std::string::npos) {
                break;
            }
            at = comma + 1;
        }
        if (f.size() < 17) {
            continue;
        }
        const int hour = std::atoi(f[5].c_str());
        if (points.empty() || points.back().hour != hour) {
            TrackPoint p;
            p.hour = hour;
            p.lat = position(f[6], 'S');
            p.lon = position(f[7], 'W');
            p.wind = std::atoi(f[8].c_str());
            p.pressure = std::atoi(f[9].c_str());
            points.push_back(p);
        }
        const int threshold = std::atoi(f[11].c_str());
        const int row = threshold == 34 ? 0 : threshold == 50 ? 1 : threshold == 64 ? 2 : -1;
        if (row >= 0) {
            for (int q = 0; q < 4; q++) {
                points.back().radii[row][q] = std::atof(f[static_cast<size_t>(13 + q)].c_str());
            }
        }
    }
    return points;
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
        {
            auto x = upper("200_wnd_ht", "200mb Wind and Height", "200 mb", 12);
            x.barbU.clear();   // the model guidance site draws the isotachs and the heights only
            x.barbV.clear();
            p.push_back(x);
        }
        {
            auto x = upper("250_wnd_ht", "250mb Wind and Height", "250 mb", 12);
            x.barbU.clear();   // the model guidance site draws the isotachs and the heights only
            x.barbV.clear();
            p.push_back(x);
        }
        {
            auto x = upper("300_wnd_ht", "300mb Wind and Height", "300 mb", 12);
            x.barbU.clear();   // the model guidance site draws the isotachs and the heights only
            x.barbV.clear();
            p.push_back(x);
        }
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
            x.wants = {want("z", "HGT", "500 mb"), want("rh", "RH", "500 mb"), want("u", "UGRD", "500 mb"), want("v", "VGRD", "500 mb")};
            x.fill = [] (const Grids& g) { return pick(g, "rh"); };
            x.ramp = humidity();
            x.fillTitle = "Relative humidity (%)";
            x.legendStep = 10;
            p.push_back(x);
            auto y = x;
            y.id = "850_rh_ht";
            y.label = "850mb Relative Humidity and Height";
            y.wants = {want("z", "HGT", "850 mb"), want("rh", "RH", "850 mb"), want("u", "UGRD", "850 mb"), want("v", "VGRD", "850 mb")};
            y.contours = {heights(3)};
            p.push_back(y);
            auto w = x;   // 700 mb: with the vertical motion (omega) as lines, the rising air only
            w.id = "700_rh_ht";
            w.label = "700mb Relative Humidity, Height and Omega";
            w.wants = {want("z", "HGT", "700 mb"), want("rh", "RH", "700 mb"), want("o", "VVEL", "700 mb"), want("u", "UGRD", "700 mb"), want("v", "VGRD", "700 mb")};
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
        std::map<std::string, Product> plainPrecip;   // the precipitation charts before the lines are put on them: for the models whose maps have none
        // MAG draws the sea level pressure and the 1000-500 mb thickness over its precipitation maps (the accumulation of the whole run with the pressure only): the same lines here
        for (auto& chart : p) {
            if (chart.source != "GFS" || chart.id.compare(0, 8, "precip_p") != 0) {
                continue;
            }
            plainPrecip[chart.id] = chart;
            const bool withThickness = chart.id != "precip_ptot";
            const auto inner = chart.needs;
            chart.needs = [inner, withThickness] (int hour) {
                auto needs = inner(hour);
                needs.push_back({hour, {"p", "PRMSL", "mean sea level", ""}});
                if (withThickness) {
                    needs.push_back({hour, {"zl", "HGT", "1000 mb", ""}});
                    needs.push_back({hour, {"zh", "HGT", "500 mb", ""}});
                }
                return needs;
            };
            const auto derived = chart.derive;
            chart.derive = [derived, withThickness] (Grids& g, const Context& context) {
                if (derived) {
                    derived(g, context);
                }
                if (withThickness) {
                    g["thick"] = GfsGrid::difference(g["zh"], g["zl"]);
                }
            };
            chart.contours = {pressure()};
            if (withThickness) {
                ContourSet t;
                t.key = "thick";
                t.scale = 0.1;
                t.interval = 6;
                t.title = "1000-500mb thickness (dam)";
                t.color = QColor{190, 50, 40};
                t.colorBelow = QColor{40, 90, 190};
                t.split = 540;
                t.dashed = true;
                t.width = 1.3;
                chart.contours.push_back(t);
            }
        }
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
                needs.push_back({hour, {"zl", "HGT", "1000 mb", ""}});
                needs.push_back({hour, {"zh", "HGT", "500 mb", ""}});
                for (const auto& [key, variable] : {std::pair{"cr", "CRAIN"}, {"cs", "CSNOW"}, {"cf", "CFRZR"}, {"ci", "CICEP"}}) {
                    needs.push_back({hour, {key, variable, "surface", ""}});
                }
                return needs;
            };
            x.derive = [totalDerive] (Grids& g, const Context& context) {
                totalDerive(g, context);
                g["thick"] = GfsGrid::difference(g["zh"], g["zl"]);
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
            ContourSet thick;
            thick.key = "thick";
            thick.scale = 0.1;
            thick.interval = 6;
            thick.title = "1000-500mb thickness (dam)";
            thick.color = QColor{190, 50, 40};
            thick.colorBelow = QColor{40, 90, 190};
            thick.split = 540;
            thick.dashed = true;
            thick.width = 1.3;
            x.contours = {pressure(), thick};
            p.push_back(x);
        }
        {   // the same by rate: the precipitation rate (inches per hour) in the colors of what is falling, with the pressure and the thickness
            Product x;
            x.id = "precip_rate_type";
            x.label = "MSLP, 1000-500mb thickness and Precipitation Rate (Rain / Snow / Mixed)";
            x.needs = [] (int hour) {
                std::vector<GfsData::Need> needs{{hour, {"rate", "PRATE", "surface", ""}}, {hour, {"p", "PRMSL", "mean sea level", ""}}, {hour, {"zl", "HGT", "1000 mb", ""}}, {hour, {"zh", "HGT", "500 mb", ""}}};
                for (const auto& [key, variable] : {std::pair{"cr", "CRAIN"}, {"cs", "CSNOW"}, {"cf", "CFRZR"}, {"ci", "CICEP"}}) {
                    needs.push_back({hour, {key, variable, "surface", ""}});
                }
                return needs;
            };
            x.derive = [] (Grids& g, const Context&) {
                g["thick"] = GfsGrid::difference(g["zh"], g["zl"]);
                const auto perHour = GfsGrid::scaled(g["rate"], 3600.0);   // millimeters per second -> millimeters per hour
                auto rain = perHour, snow = perHour, mix = perHour;
                for (size_t i = 0; i < perHour.values.size(); i++) {
                    const bool isSnow = g["cs"].values[i] >= 0.5f;
                    const bool isMix = g["cf"].values[i] >= 0.5f || g["ci"].values[i] >= 0.5f;
                    rain.values[i] = (!isSnow && !isMix) ? perHour.values[i] : 0.0f;
                    snow.values[i] = isSnow && !isMix ? perHour.values[i] : 0.0f;
                    mix.values[i] = isMix ? perHour.values[i] : 0.0f;
                }
                g["rain"] = std::move(rain);
                g["snow"] = std::move(snow);
                g["mix"] = std::move(mix);
            };
            x.fill = [] (const Grids& g) { return pick(g, "rain"); };
            x.ramp = precipitation();
            x.overlays = {{"snow", snowPrecipitation()}, {"mix", mixedPrecipitation()}};
            x.fillTitle = "Precipitation rate per hour (green rain, blue snow, pink mixed)";
            x.quantity = Quantity::Millimeters;
            x.legendStep = 0.0;
            ContourSet thick;
            thick.key = "thick";
            thick.scale = 0.1;
            thick.interval = 6;
            thick.title = "1000-500mb thickness (dam)";
            thick.color = QColor{190, 50, 40};
            thick.colorBelow = QColor{40, 90, 190};
            thick.split = 540;
            thick.dashed = true;
            thick.width = 1.3;
            x.contours = {pressure(), thick};
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

        // ---- the rest of the blend's maps: chances of precipitation and of each type, snow (hourly, the percentiles, the chance of reaching an amount), the snow to liquid ratio, thunderstorm
        // coverage and chance, visibility, ceiling and echo top, and the chances of low visibility and low ceilings
        {
            const auto nbmChart = [&] (const char * id, const char * label) {
                Product x;
                x.source = "NBM";
                x.id = id;
                x.label = label;
                return x;
            };
            const auto chance = [&] (const char * id, const char * label, const char * variable, const char * level, std::function<std::string(int)> forecast, const char * detail, const char * title) {
                auto x = nbmChart(id, label);
                const std::string var = variable, lev = level, det = detail;
                x.needs = [=] (int hour) { return std::vector<GfsData::Need>{{hour, nbmWant("f", var.c_str(), lev.c_str(), forecast(hour), det)}}; };
                x.fill = [] (const Grids& g) { return pick(g, "f"); };
                x.ramp = probability();
                x.fillTitle = title;
                x.legendStep = 10;
                return x;
            };
            const auto accWindow = [window] (int length) { return [window, length] (int hour) { return window(hour, std::min(length, hour), "acc"); }; };
            p.push_back(chance("1hour_precip_chance", "1-hour Chance of Precipitation", "APCP", "surface", accWindow(1), "prob >0.254", "Chance of 0.01 in or more of precipitation in the hour (%)"));
            p.push_back(chance("6hour_precip_chance", "6-hour Chance of Precipitation", "APCP", "surface", accWindow(6), "prob >0.254", "Chance of 0.01 in or more of precipitation in 6 hours (%)"));
            p.push_back(chance("12hour_precip_chance", "12-hour Chance of Precipitation", "APCP", "surface", accWindow(12), "prob >0.254", "Chance of 0.01 in or more of precipitation in 12 hours (%)"));
            const auto now = [atHour] (int hour) { return atHour(hour); };
            p.push_back(chance("prob_rain", "Probability of Rain", "PTYPE", "surface", now, "prob >=1 <2", "Chance of rain (%)"));
            p.push_back(chance("prob_snow", "Probability of Snow", "PTYPE", "surface", now, "prob >=8 <9", "Chance of snow (%)"));
            p.push_back(chance("prob_sleet", "Probability of Sleet", "PTYPE", "surface", now, "prob >=5 <7", "Chance of sleet (%)"));
            p.push_back(chance("prob_freezing_rain", "Probability of Freezing Rain", "PTYPE", "surface", now, "prob >=3 <4", "Chance of freezing rain (%)"));
            for (const auto& [id, label, inches, meters] : {std::tuple{"prob_1h_snow_0.1in", "Probability of 1-hour 0.1 inch of snow", "0.1 in", "0.00254"}, {"prob_1h_snow_0.5in", "Probability of 1-hour 0.5 inch of snow", "0.5 in", "0.0127"},
                                                            {"prob_1h_snow_1in", "Probability of 1-hour 1 inch of snow", "1 in", "0.0254"}, {"prob_1h_snow_1.5in", "Probability of 1-hour 1.5 inch of snow", "1.5 in", "0.0381"},
                                                            {"prob_1h_snow_2in", "Probability of 1-hour 2 inches of snow", "2 in", "0.0508"}, {"prob_1h_snow_3in", "Probability of 1-hour 3 inches of snow", "3 in", "0.0762"},
                                                            {"prob_1h_snow_4in", "Probability of 1-hour 4 inches of snow", "4 in", "0.1016"}}) {
                p.push_back(chance(id, label, "ASNOW", "surface", accWindow(1), (std::string{"prob >"} + meters).c_str(), (std::string{"Chance of "} + inches + " or more of snow in the hour (%)").c_str()));
            }
            p.push_back(nbmAccum("1hour_accu_snow", "1-hour accumulated snow", "ASNOW", 1, snowfall(), Quantity::Centimeters, 100.0));
            for (const int level : {10, 50, 90}) {   // the amounts that 10, 50 and 90 per cent of the blend's members stay under
                auto x = nbmChart((std::to_string(level) + "th_percentile_1hr_snow").c_str(), (std::to_string(level) + "th Percentile Hourly Snowfall").c_str());
                const auto detail = std::to_string(level) + "% level";
                x.needs = [=] (int hour) { return std::vector<GfsData::Need>{{hour, nbmWant("f", "ASNOW", "surface", std::to_string(hour - 1) + "-" + std::to_string(hour) + " hour acc@*", detail)}}; };
                x.fill = [] (const Grids& g) { return GfsGrid::scaled(pick(g, "f"), 100.0); };
                x.ramp = snowfall();
                x.quantity = Quantity::Centimeters;
                x.fillTitle = std::to_string(level) + "th percentile of the snow in the hour";
                x.legendStep = 0;
                p.push_back(x);
            }
            {
                auto x = nbmChart("snow_liquid_ratio", "Snow Liquid Ratio");
                x.needs = [=] (int hour) { return std::vector<GfsData::Need>{{hour, nbmWant("f", "SNOWLR", "surface", atHour(hour))}}; };
                x.fill = [] (const Grids& g) { return pick(g, "f"); };
                x.ramp = Ramp{{{0, QColor{"#b5368f"}}, {5, QColor{"#d9435f"}}, {8, QColor{"#ee7a47"}}, {10, QColor{"#f6e04a"}}, {13, QColor{"#a9d98a"}}, {16, QColor{"#55b6a8"}}, {20, QColor{"#2b7fc0"}}, {30, QColor{"#5b43a8"}}}};
                x.fillTitle = "Inches of snow for an inch of water (x to 1)";
                x.legendStep = 5;
                p.push_back(x);
            }
            {   // coverage: the 1 hour chance of thunder in the bands the forecasters use (isolated 10-20 %, scattered 30-50 %, numerous from 60 %)
                auto x = chance("tstm_coverage", "Thunderstorm Coverage", "TSTM", "surface", accWindow(1), "probability forecast", "Thunderstorm coverage in the hour: isolated, scattered, numerous");
                x.ramp = Ramp{{{0, QColor{255, 255, 255, 0}}, {9.9, QColor{255, 255, 255, 0}}, {10, QColor{"#ffe97a"}}, {29.9, QColor{"#ffe97a"}}, {30, QColor{"#f09a2e"}}, {59.9, QColor{"#f09a2e"}}, {60, QColor{"#c4262c"}}, {100, QColor{"#c4262c"}}}};
                x.legendStep = 0;
                p.push_back(x);
            }
            p.push_back(chance("prob_tstm", "Thunderstorm Probability", "TSTM", "surface", accWindow(1), "probability forecast", "Chance of a thunderstorm in the hour (%)"));
            {
                auto x = nbmChart("visibility", "2-meter Visibility");
                x.needs = [=] (int hour) { return std::vector<GfsData::Need>{{hour, nbmWant("f", "VIS", "surface", atHour(hour))}}; };
                x.fill = [] (const Grids& g) { return GfsGrid::scaled(pick(g, "f"), 1.0 / 1609.344); };
                x.ramp = visibilityRamp();
                x.fillTitle = "Visibility (miles)";
                x.legendStep = 0;
                p.push_back(x);
            }
            {
                auto x = nbmChart("ceiling", "Ceiling");
                x.needs = [=] (int hour) { return std::vector<GfsData::Need>{{hour, nbmWant("f", "CEIL", "cloud ceiling", atHour(hour))}}; };
                x.fill = [] (const Grids& g) { return GfsGrid::scaled(pick(g, "f"), 3.28084); };
                x.ramp = ceilingRamp();
                x.fillTitle = "Ceiling (feet)";
                x.legendStep = 0;
                p.push_back(x);
            }
            {
                auto x = nbmChart("echo_top", "Echo Top Height");
                x.needs = [=] (int hour) { return std::vector<GfsData::Need>{{hour, nbmWant("f", "RETOP", "cloud top", atHour(hour))}}; };
                x.fill = [] (const Grids& g) {   // where there is no echo the blend writes a huge number, not a missing value
                    auto out = GfsGrid::scaled(pick(g, "f"), 3.28084 / 1000.0);
                    for (auto& v : out.values) {
                        if (v > 80.0f || v < 0.0f) {
                            v = std::nanf("");
                        }
                    }
                    return out;
                };
                x.ramp = echoTopRamp();
                x.fillTitle = "Echo top (thousands of feet)";
                x.legendStep = 10;
                p.push_back(x);
            }
            for (const auto& [id, label, miles, meters] : {std::tuple{"prob_vis_5mi", "Probability of Visibility 5 miles or less", "5 miles", "8046.73"}, {"prob_vis_3mi", "Probability of Visibility less than 3 miles", "3 miles", "4828.03"},
                                                           {"prob_vis_2mi", "Probability of Visibility less than 2 miles", "2 miles", "3218.69"}, {"prob_vis_1mi", "Probability of Visibility less than 1 mile", "1 mile", "1609.34"}}) {
                p.push_back(chance(id, label, "VIS", "surface", now, (std::string{"prob <"} + meters).c_str(), (std::string{"Chance of visibility under "} + miles + " (%)").c_str()));
            }
            for (const auto& [id, label, feet, meters] : {std::tuple{"prob_ceil_1000ft", "Probability of Ceiling Less than 1000 feet", "1000 feet", "304.8"}, {"prob_ceil_2000ft", "Probability of Ceiling Less than 2000 feet", "2000 feet", "609.6"},
                                                          {"prob_ceil_3000ft", "Probability of Ceiling 3000 feet or less", "3000 feet", "914.5"}, {"prob_ceil_6500ft", "Probability of Ceiling 6500 feet or less", "6500 feet", "2011.68"}}) {
                p.push_back(chance(id, label, "CEIL", "cloud ceiling", now, (std::string{"prob <"} + meters).c_str(), (std::string{"Chance of a ceiling under "} + feet + " (%)").c_str()));
            }
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
        const auto adapt = [relativeHumidity] (const Product& gfs, const char * model, bool specific, bool pieces = true) {
            Product x = gfs;
            x.source = model;
            x.needs = [gfs, specific, pieces] (int hour) {
                std::vector<GfsData::Need> out;
                int end = -1, start = 0;
                for (auto need : GfsChart::needs(gfs, hour)) {
                    if (pieces && need.want.variable == "APCP") {   // the running total is not in the file: its pieces are taken below
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
                if (pieces && end > 0) {
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
            if (pieces && gfs.fillTitleFor) {   // the pieces are 6 hours: the period is told from them
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
        // The clone: a model's charts made from the GFS ones as its registry entry says (GfsModels.cpp): the whole list, or only the charts whose fields the model has.
        const auto cloneFrom = [&] (const GfsModels::Def& def) {
            const auto& rule = def.clone;
            const auto addChart = [&] (const Product& original) {
                auto x = adapt(original, def.id.c_str(), rule.specificHumidity, rule.pieces);
                if (!rule.rename.empty()) {
                    const auto inner = x.needs;
                    const auto rename = rule.rename;
                    x.needs = [inner, rename] (int hour) {
                        auto list = inner(hour);
                        for (auto& need : list) {
                            const auto found = rename.find(need.want.variable);
                            if (found != rename.end()) {
                                need.want.variable = found->second;
                            }
                        }
                        return list;
                    };
                }
                x.label = rule.labelPrefix + x.label;
                p.push_back(std::move(x));
            };
            const auto fits = [&] (const Product& chart) {
                for (const auto& part : rule.skip) {
                    if (chart.id.find(part) != std::string::npos) {
                        return false;
                    }
                }
                for (const auto& need : GfsChart::needs(chart, 24)) {
                    const auto& w = need.want;
                    const bool pressureLevel = w.level.size() > 3 && w.level.compare(w.level.size() - 3, 3, " mb") == 0 && w.level.find("above ground") == std::string::npos;
                    if ((!rule.variables.empty() && !rule.variables.count(w.variable)) || (pressureLevel && !rule.levels.empty() && !rule.levels.count(w.level))) {
                        return false;
                    }
                }
                return true;
            };
            const auto plainOf = [&] (const Product& chart) -> const Product& {   // the chart before the lines were put on it, when the model's maps have none
                for (const auto& prefix : rule.plain) {
                    const auto found = plainPrecip.find(chart.id);
                    if (chart.id.compare(0, prefix.size(), prefix) == 0 && found != plainPrecip.end()) {
                        return found->second;
                    }
                }
                return chart;
            };
            const auto size = p.size();
            if (!rule.ids.empty()) {
                for (const auto& id : rule.ids) {
                    for (size_t i = 0; i < size; i++) {
                        if (p[i].id == id && p[i].source == "GFS") {
                            const auto chart = plainOf(p[i]);
                            if (fits(chart)) {
                                addChart(chart);
                            }
                            break;
                        }
                    }
                }
            } else {
                for (size_t i = 0; i < size; i++) {
                    if (p[i].source == "GFS") {
                        const auto chart = plainOf(p[i]);
                        if (fits(chart)) {
                            addChart(chart);
                        }
                    }
                }
            }
        };

        // ---- GEFS: the ensemble mean drawn like the GFS (every chart whose fields the mean files hold: they have relative humidity, and precipitation in 6 hour pieces like the AI model),
        // and the spread (the standard deviation of the 30 members) of the fields that matter, filled under the mean's lines
        const auto gefsRecipes = [&] {
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

            // ---- the maps of the model guidance site that put the mean and the spread of the 30 members on one chart: the mean as lines and barbs with the spread as the fill, or the mean as the
            // fill with the spread as lines
            const auto windSpread = [] (Grids& g) {   // the spread of the wind speed from the spread of its two components
                auto out = g["su"];
                for (size_t i = 0; i < out.values.size(); i++) {
                    out.values[i] = static_cast<float>(std::hypot(g["su"].values[i], g["sv"].values[i]));
                }
                g["ws"] = std::move(out);
            };
            const auto meanContours = [] (const char * key, double interval, const char * title, QColor color) {
                ContourSet c;
                c.key = key;
                c.interval = interval;
                c.title = title;
                c.color = color;
                return c;
            };
            for (const auto& [lvl, top] : {std::pair{"250", 20.0}, {"500", 16.0}, {"700", 12.0}, {"850", 12.0}, {"925", 12.0}}) {   // winds: the mean as barbs, the spread of the speed as the fill (the scale reaches the 10 m/s and more a storm makes)
                const std::string level = std::string{lvl} + " mb";
                Product x;
                x.source = "GEFS";
                x.id = std::string{lvl} + "_wnd";
                x.label = "Mean " + std::string{lvl} + "mb Winds and Spread";
                x.wants = {want("u", "UGRD", level.c_str()), want("v", "VGRD", level.c_str()), spr("su", "UGRD", level.c_str()), spr("sv", "VGRD", level.c_str())};
                x.derive = [windSpread] (Grids& g, const Context&) { windSpread(g); };
                x.fill = [] (const Grids& g) { return pick(g, "ws"); };
                x.ramp = spreadRamp(top);
                x.fillTitle = "Spread of the " + std::string{lvl} + " mb wind speed (m/s)";
                x.legendStep = 2;
                x.barbU = "u";
                x.barbV = "v";
                p.push_back(x);
            }
            for (const auto& [lvl, top] : {std::pair{"250", 5.0}, {"500", 3.0}, {"700", 3.0}, {"850", 4.0}}) {   // temperatures: the mean as lines, the spread as the fill
                const std::string level = std::string{lvl} + " mb";
                Product x;
                x.source = "GEFS";
                x.id = std::string{lvl} + "_temp";
                x.label = "Mean " + std::string{lvl} + "mb Temperature and Spread";
                x.wants = {want("t", "TMP", level.c_str()), spr("s", "TMP", level.c_str())};
                x.fill = [] (const Grids& g) { return pick(g, "s"); };
                x.ramp = spreadRamp(top);
                x.fillTitle = "Spread of the " + std::string{lvl} + " mb temperature (K)";
                x.legendStep = 1;
                x.contours = {meanContours("t", 5, "Mean temperature (C)", QColor{30, 30, 30})};
                p.push_back(x);
            }
            for (const auto& [lvl, interval, top] : {std::tuple{"500", 6.0, 60.0}, {"700", 3.0, 40.0}, {"850", 3.0, 30.0}}) {   // height (mean solid, spread the fill) with the mean vorticity dashed
                const std::string level = std::string{lvl} + " mb";
                Product x;
                x.source = "GEFS";
                x.id = std::string{lvl} + "_vort_ht";
                x.label = "Mean " + std::string{lvl} + "mb Height, Vorticity and Spread";
                x.wants = {want("z", "HGT", level.c_str()), spr("s", "HGT", level.c_str()), want("u", "UGRD", level.c_str()), want("v", "VGRD", level.c_str())};
                x.derive = [] (Grids& g, const Context&) { g["vo"] = GfsGrid::scaled(GfsGrid::vorticity(GfsGrid::smoothed(g["u"], 1), GfsGrid::smoothed(g["v"], 1)), 1e5); };
                x.fill = [] (const Grids& g) { return pick(g, "s"); };
                x.ramp = spreadRamp(top);
                x.fillTitle = "Spread of the " + std::string{lvl} + " mb height (m)";
                x.legendStep = top / 6;
                auto vort = meanContours("vo", 4, "Mean vorticity (1e-5 /s)", QColor{20, 90, 150});
                vort.dashed = true;
                vort.minimum = 4;   // the cyclonic side, as the model guidance site draws it
                x.contours = {meanHeights(interval), vort};
                p.push_back(x);
            }
            {   // sea level pressure: the mean as lines, the spread as the fill
                Product x;
                x.source = "GEFS";
                x.id = "mslp";
                x.label = "Mean Sea Level Pressure and Spread";
                x.wants = {want("p", "PRMSL", "mean sea level"), spr("s", "PRMSL", "mean sea level")};
                x.fill = [] (const Grids& g) { return GfsGrid::scaled(pick(g, "s"), 0.01); };
                x.ramp = spreadRamp(6.0);
                x.fillTitle = "Spread of the sea level pressure (mb)";
                x.legendStep = 1;
                x.contours = {pressure()};
                p.push_back(x);
            }
            {
                Product x;
                x.source = "GEFS";
                x.id = "10m_wnd";
                x.label = "Mean 10m Winds and Spread";
                x.wants = {want("u", "UGRD", "10 m above ground"), want("v", "VGRD", "10 m above ground"), spr("su", "UGRD", "10 m above ground"), spr("sv", "VGRD", "10 m above ground")};
                x.derive = [windSpread] (Grids& g, const Context&) { windSpread(g); };
                x.fill = [] (const Grids& g) { return pick(g, "ws"); };
                x.ramp = spreadRamp(12.0);
                x.fillTitle = "Spread of the 10 m wind speed (m/s)";
                x.legendStep = 2;
                x.barbU = "u";
                x.barbV = "v";
                p.push_back(x);
            }
            {
                Product x;
                x.source = "GEFS";
                x.id = "2m_temp";
                x.label = "Mean 2m Temperature and Spread";
                x.wants = {want("t", "TMP", "2 m above ground"), spr("s", "TMP", "2 m above ground")};
                x.fill = [] (const Grids& g) { return pick(g, "s"); };
                x.ramp = spreadRamp(5.0);
                x.fillTitle = "Spread of the 2 m temperature (K)";
                x.legendStep = 1;
                x.contours = {meanContours("t", 5, "Mean temperature (C)", QColor{30, 30, 30})};
                p.push_back(x);
            }
            {   // CAPE: the mean the fill, the spread lines
                Product x;
                x.source = "GEFS";
                x.id = "cape";
                x.label = "Mean CAPE and Spread";
                x.wants = {want("c", "CAPE", "surface"), spr("s", "CAPE", "surface")};
                x.fill = [] (const Grids& g) { return pick(g, "c"); };
                x.ramp = capeRamp();
                x.fillTitle = "Mean CAPE (J/kg)";
                x.legendStep = 500;
                {
                    auto c = meanContours("s", 250, "Spread of the CAPE (J/kg)", QColor{30, 30, 30});
                    c.minimum = 250;
                    x.contours = {c};
                }
                p.push_back(x);
            }
            for (const int length : {6, 24}) {   // the precipitation of 6 and 24 hours: the mean the fill, the spread lines. The 24 hour mean is its four 6 hour means; its spread is estimated from theirs
                Product x;
                x.source = "GEFS";
                x.id = length == 6 ? "precip_p06" : "precip_p24";
                x.label = length == 6 ? "Mean 6-hour Precipitation and Spread" : "Mean 24-hour Precipitation and Spread";
                const int pieces = length / 6;
                x.needs = [pieces] (int hour) {
                    std::vector<GfsData::Need> out;
                    for (int k = 0; k < pieces; k++) {
                        const int end = hour - 6 * k;
                        if (end < 6) {
                            break;
                        }
                        const auto when = std::to_string(end - 6) + "-" + std::to_string(end) + " hour acc fcst";
                        out.push_back({end, GfsData::Want{"a" + std::to_string(k), "APCP", "surface", when, "", ""}});
                        out.push_back({end, GfsData::Want{"s" + std::to_string(k), "APCP", "surface", when, "", "spr"}});
                    }
                    return out;
                };
                x.derive = [pieces] (Grids& g, const Context&) {
                    auto mean = g["a0"], sprd = g["s0"];
                    for (size_t i = 0; i < mean.values.size(); i++) {
                        double m = 0.0, q = 0.0;
                        for (int k = 0; k < pieces && g.count("a" + std::to_string(k)); k++) {
                            m += g["a" + std::to_string(k)].values[i];
                            q += static_cast<double>(g["s" + std::to_string(k)].values[i]) * g["s" + std::to_string(k)].values[i];
                        }
                        mean.values[i] = static_cast<float>(m);
                        sprd.values[i] = static_cast<float>(std::sqrt(q));
                    }
                    g["mean"] = mean;
                    g["spread"] = sprd;
                };
                x.fill = [] (const Grids& g) { return pick(g, "mean"); };
                x.ramp = precipitation();
                x.quantity = Quantity::Millimeters;
                x.fillTitle = std::string{"Mean precipitation, "} + std::to_string(length) + " hours";
                x.legendStep = 0;
                ContourSet c;
                c.key = "spread";
                c.scale = 1.0 / 25.4;   // millimeters -> inches
                c.interval = 0.1;
                c.minimum = 0.1;   // lines of real spread, not a 0 line round every dry area
                c.title = "Spread (in)";
                c.color = QColor{30, 30, 30};
                x.contours = {c};
                p.push_back(x);
            }
        };

        // ---- RRFS: the 3 km Rapid Refresh Forecast System (NAM, HRRR, RAP and the high resolution windows all end up here). Every GFS chart whose fields it holds is made again from it (sea level
        // pressure is its MSLET; precipitation has running totals like the GFS), then the charts only a convection-allowing model has: radar, helicity, updraft helicity, echo tops, ceiling,
        // visibility, lightning, gusts and CAPE with CIN.
        const auto rrfsRecipes = [&] {
            const auto make = [] (const char * id, const char * label) {
                Product x;
                x.source = "RRFS";
                x.id = id;
                x.label = label;
                return x;
            };
            const auto scaledOf = [] (const char * key, double factor) { return [key, factor] (const Grids& g) { return GfsGrid::scaled(pick(g, key), factor); }; };
            const auto sea = [] { return want("p", "MSLET", "mean sea level"); };
            const auto wind10 = [&] (Product& x) {
                x.wants.push_back(want("u", "UGRD", "10 m above ground"));
                x.wants.push_back(want("v", "VGRD", "10 m above ground"));
                x.barbU = "u";
                x.barbV = "v";
            };
            const auto hourly = [] (int hour) { return std::to_string(std::max(hour - 1, 0)) + "-" + std::to_string(hour) + " hour max fcst"; };   // the last hour's maximum
            {
                auto x = make("10m_wnd", "10m Wind and MSLP");
                wind10(x);
                x.wants.push_back(sea());
                x.fill = speedOf("u", "v");
                x.ramp = lowWind();
                x.fillTitle = "10 m wind speed (kt)";
                x.legendStep = 10;
                x.contours = {pressure()};
                p.push_back(x);
            }
            {
                auto x = make("10m_wnd_sfc_gust", "10m Wind Barbs and Surface Gust");
                wind10(x);
                x.wants.push_back(want("g", "GUST", "surface"));
                x.fill = scaledOf("g", msToKnots);
                x.ramp = lowWind();
                x.fillTitle = "Surface wind gust (kt)";
                x.legendStep = 10;
                p.push_back(x);
            }
            {
                auto x = make("10m_maxwnd", "Maximum 10m Wind in the last hour");
                x.needs = [hourly] (int hour) {
                    return std::vector<GfsData::Need>{{hour, {"u", "MAXUW", "10 m above ground", hourly(hour), ""}}, {hour, {"v", "MAXVW", "10 m above ground", hourly(hour), ""}}};
                };
                x.fill = speedOf("u", "v");
                x.ramp = lowWind();
                x.fillTitle = "Strongest 10 m wind of the last hour (kt)";
                x.legendStep = 10;
                p.push_back(x);
            }
            for (const auto& [id, label, layer] : {std::tuple{"sfc_cape_cin", "Surface-Based CAPE and CIN", "surface"}, {"best_cape_cin", "Most Unstable CAPE and CIN", "255-0 mb above ground"}}) {
                auto x = make(id, label);
                x.wants = {want("c", "CAPE", layer), want("n", "CIN", layer)};
                x.fill = [] (const Grids& g) { return pick(g, "c"); };
                x.ramp = capeRamp();
                x.fillTitle = "CAPE (J/kg)";
                x.legendStep = 500;
                ContourSet cin;
                cin.key = "n";
                cin.interval = 100;
                cin.title = "CIN (J/kg)";
                cin.color = QColor{70, 70, 70};
                cin.dashed = true;
                x.contours = {cin};
                p.push_back(x);
            }
            for (const auto& [id, label, layer] : {std::tuple{"helicity_1km", "0-1 km Storm-Relative Helicity", "1000-0 m above ground"}, {"helicity_3km", "0-3 km Storm-Relative Helicity", "3000-0 m above ground"}}) {
                auto x = make(id, label);
                x.wants = {want("h", "HLCY", layer)};
                x.fill = [] (const Grids& g) { return pick(g, "h"); };
                x.ramp = helicityRamp();
                x.fillTitle = "Storm-relative helicity (m2/s2)";
                x.legendStep = 100;
                p.push_back(x);
            }
            {
                auto x = make("max_updraft_hlcy", "Maximum 2-5 km Updraft Helicity in the last hour");
                x.needs = [hourly] (int hour) { return std::vector<GfsData::Need>{{hour, {"h", "MXUPHL", "5000-2000 m above ground", hourly(hour), ""}}}; };
                x.fill = [] (const Grids& g) { return pick(g, "h"); };
                x.ramp = updraftHelicity();
                x.fillTitle = "Updraft helicity (m2/s2)";
                x.legendStep = 0;
                p.push_back(x);
            }
            {
                // MAG colors the echoes by what is falling: green rain, blue snow, purple sleet, red freezing rain (the type flags are the model's, at the hour shown)
                const auto shades = [] (QColor light, QColor dark) {
                    Ramp r;
                    r.stops.push_back({0.0, QColor{light.red(), light.green(), light.blue(), 0}});
                    r.stops.push_back({4.9, QColor{light.red(), light.green(), light.blue(), 0}});
                    const double dbz[] = {5, 15, 25, 35, 45, 55, 65, 75};
                    for (int i = 0; i < 8; i++) {
                        const double f = i / 7.0;
                        r.stops.push_back({dbz[i], QColor{static_cast<int>(light.red() + (dark.red() - light.red()) * f), static_cast<int>(light.green() + (dark.green() - light.green()) * f),
                                                         static_cast<int>(light.blue() + (dark.blue() - light.blue()) * f)}});
                    }
                    return r;
                };
                auto x = make("sim_radar_1km", "Simulated Radar at 1 km");
                x.wants = {want("r", "REFD", "1000 m above ground"), want("cr", "CRAIN", "surface"), want("cs", "CSNOW", "surface"), want("cf", "CFRZR", "surface"), want("ci", "CICEP", "surface")};
                x.derive = [] (Grids& g, const Context&) {
                    auto rain = g["r"], snow = g["r"], sleet = g["r"], freezing = g["r"];
                    for (size_t i = 0; i < rain.values.size(); i++) {
                        const bool isSnow = g["cs"].values[i] >= 0.5f, isFreezing = g["cf"].values[i] >= 0.5f, isSleet = g["ci"].values[i] >= 0.5f;
                        const float z = g["r"].values[i], none = std::nanf("");
                        rain.values[i] = !isSnow && !isFreezing && !isSleet ? z : none;
                        snow.values[i] = isSnow && !isFreezing && !isSleet ? z : none;
                        sleet.values[i] = isSleet && !isFreezing ? z : none;
                        freezing.values[i] = isFreezing ? z : none;
                    }
                    g["rain"] = rain;
                    g["snow"] = snow;
                    g["sleet"] = sleet;
                    g["freezing"] = freezing;
                };
                x.fill = [] (const Grids& g) { return pick(g, "rain"); };
                x.ramp = Ramp{{{0, QColor{160, 255, 0, 0}}, {4.9, QColor{160, 255, 0, 0}}, {5, QColor{160, 255, 0}}, {15, QColor{80, 220, 0}}, {25, QColor{0, 170, 0}}, {35, QColor{255, 230, 0}}, {45, QColor{255, 150, 0}},
                                {55, QColor{230, 0, 0}}, {65, QColor{255, 170, 170}}, {75, QColor{160, 60, 220}}}};   // the rain scale of the model guidance site: greens, then yellow, orange, red, pink and purple for the heaviest
                x.overlays = {{"snow", shades(QColor{205, 225, 255}, QColor{0, 0, 150})}, {"sleet", shades(QColor{235, 205, 255}, QColor{95, 0, 135})}, {"freezing", shades(QColor{255, 205, 205}, QColor{150, 0, 0})}};
                x.fillTitle = "Reflectivity at 1 km (dBZ): green rain, blue snow, purple sleet, red freezing rain";
                x.legendStep = 10;
                p.push_back(x);
            }
            {
                auto x = make("sim_radar_max", "Maximum Simulated Radar of the last hour");
                x.needs = [hourly] (int hour) { return std::vector<GfsData::Need>{{hour, {"r", "MAXREF", "1000 m above ground", hourly(hour), ""}}}; };
                x.fill = [] (const Grids& g) { return pick(g, "r"); };
                x.ramp = reflectivity();
                x.fillTitle = "Maximum reflectivity of the last hour (dBZ)";
                x.legendStep = 10;
                p.push_back(x);
            }
            {
                auto x = make("echo_top", "Echo Top");
                x.wants = {want("t", "RETOP", "entire atmosphere (considered as a single layer)")};
                x.fill = scaledOf("t", 3.28084 / 1000.0);
                x.ramp = echoTopRamp();
                x.fillTitle = "Echo top (thousands of feet)";
                x.legendStep = 10;
                p.push_back(x);
            }
            {
                auto x = make("ceiling", "Cloud Ceiling");
                x.wants = {want("c", "CEIL", "cloud ceiling")};
                x.fill = scaledOf("c", 3.28084);
                x.ramp = ceilingRamp();
                x.fillTitle = "Ceiling (feet)";
                x.legendStep = 0;
                p.push_back(x);
            }
            {
                auto x = make("vis", "Surface Visibility");
                x.wants = {want("v", "VIS", "surface")};
                x.fill = scaledOf("v", 1.0 / 1609.344);
                x.ramp = visibilityRamp();
                x.fillTitle = "Visibility (miles)";
                x.legendStep = 0;
                p.push_back(x);
            }
            {
                auto x = make("lightning", "Lightning (flashes in the last hour)");
                x.needs = [hourly] (int hour) { return std::vector<GfsData::Need>{{hour, {"l", "LTNG", "entire atmosphere", hourly(hour), ""}}}; };
                x.fill = [] (const Grids& g) { return pick(g, "l"); };
                x.ramp = lightningRamp();
                x.fillTitle = "Lightning";
                x.legendStep = 0;
                p.push_back(x);
            }
            {
                auto x = make("snow_total", "Total Snowfall since the start of the run");
                x.needs = [] (int hour) { return std::vector<GfsData::Need>{{hour, {"s", "ASNOW", "surface", "0-*", ""}}}; };
                x.fill = scaledOf("s", 100.0);   // meters -> centimeters
                x.ramp = snowfall();
                x.quantity = Quantity::Centimeters;
                x.fillTitle = "Snowfall since the start of the run";
                x.legendStep = 0;
                p.push_back(x);
            }
            {
                auto x = make("precip_rate", "Precipitation Rate");
                x.wants = {want("r", "PRATE", "surface")};
                x.fill = scaledOf("r", 3600.0);   // mm per second -> mm per hour
                x.ramp = precipitation();
                x.quantity = Quantity::Millimeters;
                x.fillTitle = "Precipitation rate (per hour)";
                x.legendStep = 0;
                p.push_back(x);
            }
            {
                auto x = make("500_temp_ht", "500mb Temperature and Height");
                x.wants = {want("t", "TMP", "500 mb"), want("z", "HGT", "500 mb"), want("u", "UGRD", "500 mb"), want("v", "VGRD", "500 mb")};
                x.fill = [] (const Grids& g) { return pick(g, "t"); };
                x.ramp = temperature();
                x.quantity = Quantity::Temperature;
                x.fillTitle = "500 mb temperature";
                x.legendStep = 5;
                x.contours = {heights(6)};
                x.barbU = "u";
                x.barbV = "v";
                p.push_back(x);
            }
            {
                auto x = make("925_temp_wnd", "925mb Temperature and Wind");
                x.wants = {want("t", "TMP", "925 mb"), want("u", "UGRD", "925 mb"), want("v", "VGRD", "925 mb")};
                x.fill = [] (const Grids& g) { return pick(g, "t"); };
                x.ramp = temperature();
                x.quantity = Quantity::Temperature;
                x.fillTitle = "925 mb temperature";
                x.legendStep = 5;
                // the model guidance site draws no height lines here: the temperature and the wind only
                x.barbU = "u";
                x.barbV = "v";
                p.push_back(x);
            }
            // the maps the other convection-allowing models have (HRRR, NAM nests, HiResW, RAP) that were not drawn yet
            for (const auto& [id, label, variable, ramp, title] : {std::tuple{"2m_temp_10m_wnd", "2m Temperature and 10m Wind", "TMP", 0, "2 m temperature"}, {"2m_dewp_10m_wnd", "2m Dew Point and 10m Wind", "DPT", 1, "2 m dew point"}}) {
                auto x = make(id, label);
                x.wants = {want("t", variable, "2 m above ground")};
                wind10(x);
                x.fill = [] (const Grids& g) { return pick(g, "t"); };
                x.ramp = ramp == 0 ? temperature() : dewpoint();
                x.quantity = Quantity::Temperature;
                x.fillTitle = title;
                x.legendStep = 5;
                p.push_back(x);
            }
            {   // 700 mb humidity with the rising air as lines: the model's vertical velocity in m/s changed to omega in Pa/s with the density of air at 700 mb (about 0.9 kg per cubic meter)
                auto x = make("700_rh_ht", "700mb Relative Humidity, Height and Omega");
                x.wants = {want("z", "HGT", "700 mb"), want("rh", "RH", "700 mb"), want("w", "DZDT", "700 mb"), want("u", "UGRD", "700 mb"), want("v", "VGRD", "700 mb")};
                x.barbU = "u";
                x.barbV = "v";
                x.derive = [] (Grids& g, const Context&) { g["o"] = GfsGrid::scaled(g["w"], -0.9 * 9.80665); };
                x.fill = [] (const Grids& g) { return pick(g, "rh"); };
                x.ramp = humidity();
                x.fillTitle = "700 mb relative humidity (%)";
                x.legendStep = 10;
                ContourSet omega;
                omega.key = "o";
                omega.scale = 10.0;
                omega.interval = 2;
                omega.title = "Omega (ubar/s, rising air)";
                omega.color = QColor{20, 110, 70};
                omega.dashed = true;
                omega.onlyBelowZero = true;
                x.contours = {heights(3), omega};
                p.push_back(x);
            }
            {   // the strongest rotation of the run so far: the hourly maxima of every hour added up by taking their largest
                auto x = make("accu_max_updraft_hlcy", "Accumulated Maximum 2-5 km Updraft Helicity");
                x.needs = [] (int hour) {
                    std::vector<GfsData::Need> out;
                    for (int k = 1; k <= hour; k++) {
                        out.push_back({k, {"u" + std::to_string(k), "MXUPHL", "5000-2000 m above ground", std::to_string(k - 1) + "-" + std::to_string(k) + " hour max fcst", ""}});
                    }
                    return out;
                };
                x.derive = [] (Grids& g, const Context& context) {
                    auto best = g["u1"];
                    for (int k = 2; k <= context.hour; k++) {
                        const auto& next = g["u" + std::to_string(k)];
                        for (size_t i = 0; i < best.values.size() && i < next.values.size(); i++) {
                            if (std::isnan(best.values[i]) || next.values[i] > best.values[i]) {
                                best.values[i] = next.values[i];
                            }
                        }
                    }
                    g["m"] = best;
                };
                x.fill = [] (const Grids& g) { return pick(g, "m"); };
                x.ramp = updraftHelicity();
                x.fillTitle = "Strongest updraft helicity since the start of the run (m2/s2)";
                x.legendStep = 0;
                p.push_back(x);
            }
            {
                auto x = make("helicity", "Helicity and 30m Wind");
                x.wants = {want("h", "HLCY", "3000-0 m above ground"), want("u", "UGRD", "30 m above ground"), want("v", "VGRD", "30 m above ground")};
                x.fill = [] (const Grids& g) { return pick(g, "h"); };
                x.ramp = helicityRamp();
                x.fillTitle = "0-3 km storm-relative helicity (m2/s2)";
                x.legendStep = 100;
                x.barbU = "u";
                x.barbV = "v";
                p.push_back(x);
            }
            // the fire weather set: what the fire weather nest of the model guidance site draws, from the 3 km run (the nest itself is a finer grid, a later refinement)
            for (const auto& [id, label, variable, ramp, title] : {std::tuple{"2m_tmpc", "Shelter (2 m) Temperature", "TMP", 0, "2 m temperature"}, {"2m_dwpc", "Shelter (2 m) Dew Point Temperature", "DPT", 1, "2 m dew point"}}) {
                auto x = make(id, label);
                x.wants = {want("t", variable, "2 m above ground")};
                x.fill = [] (const Grids& g) { return pick(g, "t"); };
                x.ramp = ramp == 0 ? temperature() : dewpoint();
                x.quantity = Quantity::Temperature;
                x.fillTitle = title;
                x.legendStep = 5;
                p.push_back(x);
            }
            {
                auto x = make("2m_rh_10m_wnd", "1-hr Minimum Relative Humidity, 10m Wind");
                x.needs = [] (int hour) {
                    return std::vector<GfsData::Need>{{hour, {"r", "MINRH", "2 m above ground", std::to_string(std::max(hour - 1, 0)) + "-" + std::to_string(hour) + " hour min fcst", ""}},
                                                      {hour, {"u", "UGRD", "10 m above ground", "", ""}}, {hour, {"v", "VGRD", "10 m above ground", "", ""}}};
                };
                x.fill = [] (const Grids& g) { return pick(g, "r"); };
                x.ramp = humidity();
                x.fillTitle = "Lowest relative humidity of the last hour (%)";
                x.legendStep = 10;
                x.barbU = "u";
                x.barbV = "v";
                p.push_back(x);
            }
            {
                auto x = make("best_cape", "Best (Most Unstable) CAPE");
                x.wants = {want("c", "CAPE", "255-0 mb above ground")};
                x.fill = [] (const Grids& g) { return pick(g, "c"); };
                x.ramp = capeRamp();
                x.fillTitle = "Most unstable CAPE (J/kg)";
                x.legendStep = 500;
                p.push_back(x);
            }
            {   // the Haines index (mid level, 850-700 mb): stability from the temperature drop between the levels (1 under 6 C, 2 up to 10, 3 above), moisture from the dew point depression at 850 mb (1 under 6 C, 2 up to 12, 3 above)
                auto x = make("haines", "Haines Index (850-700 mb)");
                x.wants = {want("t8", "TMP", "850 mb"), want("t7", "TMP", "700 mb"), want("d8", "DPT", "850 mb")};
                x.derive = [] (Grids& g, const Context&) {
                    auto out = g["t8"];
                    for (size_t i = 0; i < out.values.size(); i++) {
                        const double a = g["t8"].values[i] - g["t7"].values[i], b = g["t8"].values[i] - g["d8"].values[i];
                        out.values[i] = std::isnan(a) || std::isnan(b) ? std::nanf("") : static_cast<float>((a < 6 ? 1 : a <= 10 ? 2 : 3) + (b < 6 ? 1 : b <= 12 ? 2 : 3));
                    }
                    g["h"] = out;
                };
                x.fill = [] (const Grids& g) { return pick(g, "h"); };
                x.ramp = Ramp{{{1.5, QColor{"#a9d98a"}}, {2.5, QColor{"#a9d98a"}}, {3.0, QColor{"#e3d44a"}}, {4.0, QColor{"#f0a63a"}}, {5.0, QColor{"#e0502e"}}, {6.0, QColor{"#a8206b"}}}};
                x.fillTitle = "Haines index: 2-3 very low, 4 low, 5 moderate, 6 high";
                x.legendStep = 1;
                p.push_back(x);
            }
            for (const auto& [id, label, variable] : {std::tuple{"max_updraft", "Maximum 1-hr Updraft Vertical Velocity", "MAXUVV"}, {"max_downdraft", "Maximum 1-hr Downdraft Vertical Velocity", "MAXDVV"}}) {
                auto x = make(id, label);
                const std::string name = variable;
                x.needs = [name] (int hour) { return std::vector<GfsData::Need>{{hour, {"w", name, "100-1000 mb", std::to_string(std::max(hour - 1, 0)) + "-" + std::to_string(hour) + " hour max fcst", ""}}}; };
                x.fill = [name] (const Grids& g) { return GfsGrid::scaled(pick(g, "w"), name == "MAXDVV" ? -1.0 : 1.0); };   // the downdrafts as a positive speed
                x.ramp = lowWind();
                x.fillTitle = name == "MAXUVV" ? "Strongest updraft of the last hour (m/s, about)" : "Strongest downdraft of the last hour (m/s, about)";
                x.legendStep = 10;
                p.push_back(x);
            }
            for (const auto& [id, label] : {std::pair{"pbl_height", "PBL Height"}, {"pbl_rich_height", "PBL Height (Based on Richardson Number)"}}) {
                auto x = make(id, label);   // the model has the one boundary layer height
                x.wants = {want("h", "HPBL", "surface")};
                x.fill = [] (const Grids& g) { return pick(g, "h"); };
                x.ramp = ceilingRamp();
                x.ramp = Ramp{{{0, QColor{"#f4f8fb"}}, {500, QColor{"#cfe8f3"}}, {1000, QColor{"#8ccbe0"}}, {1500, QColor{"#55a868"}}, {2000, QColor{"#e3d44a"}}, {3000, QColor{"#f0a63a"}}, {4000, QColor{"#cc3d2c"}}}};
                x.fillTitle = "Boundary layer height (m)";
                x.legendStep = 500;
                p.push_back(x);
            }
            {
                auto x = make("precip_pwat", "Total Column Precipitable Water");
                x.wants = {want("pw", "PWAT", "entire atmosphere (considered as a single layer)")};
                x.fill = [] (const Grids& g) { return pick(g, "pw"); };
                x.ramp = precipitableWater();
                x.quantity = Quantity::Millimeters;
                x.fillTitle = "Precipitable water";
                x.legendStep = 0;
                p.push_back(x);
            }
            {   // transport wind (the mean wind through the mixed layer) is taken here as the mean of the winds at the heights the model gives through the lowest 320 m and the 925 and 850 mb winds when the boundary layer reaches them
                const auto levels = std::vector<std::pair<const char *, const char *>>{{"10 m above ground", "10"}, {"30 m above ground", "30"}, {"50 m above ground", "50"}, {"80 m above ground", "80"}, {"100 m above ground", "100"},
                                                                                       {"160 m above ground", "160"}, {"320 m above ground", "320"}};
                const auto meanWind = [levels] (Grids& g) {
                    auto u = g["u10"], v = g["v10"];
                    for (size_t i = 0; i < u.values.size(); i++) {
                        double su = 0.0, sv = 0.0;
                        int n = 0;
                        const double top = g["hp"].values[i];
                        for (const auto& [level, key] : levels) {
                            (void) level;
                            const double height = std::atof(key);
                            if (n > 0 && height > top) {
                                break;
                            }
                            su += g[std::string{"u"} + key].values[i];
                            sv += g[std::string{"v"} + key].values[i];
                            n++;
                        }
                        for (const char * level : {"925", "850"}) {
                            const double metres = std::string{level} == "925" ? 750.0 : 1500.0;
                            if (top > metres) {
                                su += g[std::string{"u"} + level].values[i];
                                sv += g[std::string{"v"} + level].values[i];
                                n++;
                            }
                        }
                        u.values[i] = n ? static_cast<float>(su / n) : std::nanf("");
                        v.values[i] = n ? static_cast<float>(sv / n) : std::nanf("");
                    }
                    g["mu"] = u;
                    g["mv"] = v;
                };
                const auto windWants = [levels] {
                    std::vector<GfsData::Want> w{want("hp", "HPBL", "surface"), want("u925", "UGRD", "925 mb"), want("v925", "VGRD", "925 mb"), want("u850", "UGRD", "850 mb"), want("v850", "VGRD", "850 mb")};
                    for (const auto& [level, key] : levels) {
                        w.push_back(want((std::string{"u"} + key).c_str(), "UGRD", level));
                        w.push_back(want((std::string{"v"} + key).c_str(), "VGRD", level));
                    }
                    return w;
                };
                auto x = make("transport_wind", "Transport Wind and Terrain Height");
                x.wants = windWants();
                x.wants.push_back(want("z", "HGT", "surface"));
                x.derive = [meanWind] (Grids& g, const Context&) { meanWind(g); };
                x.fill = speedOf("mu", "mv");
                x.ramp = windSpeed();
                x.fillTitle = "Transport wind speed (kt)";
                x.legendStep = 10;
                ContourSet terrain;
                terrain.key = "z";
                terrain.interval = 500;
                terrain.title = "Terrain height (m)";
                terrain.color = QColor{110, 80, 40};
                terrain.width = 0.9;
                x.contours = {terrain};
                x.barbU = "mu";
                x.barbV = "mv";
                p.push_back(x);
                auto y = make("vent_rate", "Ventilation Rate");   // the boundary layer height times the transport wind: under 2350 m2/s poor, 2350-4700 fair, above good for burning
                y.wants = windWants();
                y.derive = [meanWind] (Grids& g, const Context&) {
                    meanWind(g);
                    auto out = g["hp"];
                    for (size_t i = 0; i < out.values.size(); i++) {
                        out.values[i] = static_cast<float>(g["hp"].values[i] * std::hypot(g["mu"].values[i], g["mv"].values[i]));
                    }
                    g["vr"] = out;
                };
                y.fill = [] (const Grids& g) { return pick(g, "vr"); };
                y.ramp = Ramp{{{0, QColor{"#b5368f"}}, {2350, QColor{"#ee7a47"}}, {4700, QColor{"#e3d44a"}}, {9000, QColor{"#7bc47f"}}, {18000, QColor{"#2b7fc0"}}}};
                y.fillTitle = "Ventilation rate (m2/s): poor under 2350, fair to 4700, then good";
                y.legendStep = 0;
                y.barbU = "mu";
                y.barbV = "mv";
                p.push_back(y);
            }
            for (const char * level : {"250", "300"}) {
                const std::string text = std::string{level} + " mb";
                auto x = make((std::string{level} + "_wnd").c_str(), (std::string{level} + "mb Wind").c_str());
                x.wants = {want("u", "UGRD", text.c_str()), want("v", "VGRD", text.c_str())};
                x.fill = speedOf("u", "v");
                x.ramp = windSpeed();
                x.fillTitle = "Wind speed (kt)";
                x.legendStep = 20;
                x.barbU = "u";
                x.barbV = "v";
                p.push_back(x);
            }
        };

        // ---- REFS: the 5 member 3 km ensemble. The ready-made products (the ensemble mean of precipitation, the probabilities of precipitation and snow, flash flood risk) are charts of their own;
        // the members make the rest: probabilities of strong echoes and rotating updrafts (the fraction of members, neighborhood averaged), the strongest rotation, a paintball of each
        // member's strong echoes, and the mean and spread of the 2 m temperature.
        const auto refsRecipes = [&] {
            const auto make = [] (const char * id, const char * label) {
                Product x;
                x.source = "REFS";
                x.id = id;
                x.label = label;
                return x;
            };
            const auto sea = [] (int m) { return GfsData::Want{"p" + std::to_string(m), "MSLET", "mean sea level", "", "", "m00" + std::to_string(m)}; };
            (void) sea;
            // the same field from each member: keys "<prefix>1" ... "<prefix>5"
            const auto members = [] (const char * prefix, const char * variable, const char * level, const std::string& forecast) {
                std::vector<GfsData::Want> out;
                for (int m = 1; m <= 5; m++) {
                    out.push_back({std::string{prefix} + std::to_string(m), variable, level, forecast, "", "m00" + std::to_string(m)});
                }
                return out;
            };
            const auto hourly = [] (int hour) { return std::to_string(std::max(hour - 1, 0)) + "-" + std::to_string(hour) + " hour max fcst"; };
            // the fraction of the members at or above a threshold, averaged over the neighborhood
            const auto fraction = [] (const Grids& g, const char * prefix, double threshold) {
                auto out = pick(g, (std::string{prefix} + "1").c_str());
                for (size_t i = 0; i < out.values.size(); i++) {
                    int n = 0, over = 0;
                    for (int m = 1; m <= 5; m++) {
                        const float v = g.at(std::string{prefix} + std::to_string(m)).values[i];
                        if (!std::isnan(v)) {
                            n++;
                            over += v >= threshold ? 1 : 0;
                        }
                    }
                    out.values[i] = n > 0 ? static_cast<float>(100.0 * over / n) : std::nanf("");
                }
                return GfsGrid::smoothed(out, 6);
            };
            const auto reflectivityOf = [&] (const char * id, const char * label, double threshold) {
                auto x = make(id, label);
                x.wants = members("r", "REFC", "entire atmosphere (considered as a single layer)", "");
                x.fill = [fraction, threshold] (const Grids& g) { return fraction(g, "r", threshold); };
                x.ramp = probability();
                x.fillTitle = "Chance of the echo reaching the level (% of the members, nearby)";
                x.legendStep = 10;
                return x;
            };
            p.push_back(reflectivityOf("prob_refc_40", "Probability of Composite Reflectivity of 40 dBZ or more", 40.0));
            p.push_back(reflectivityOf("prob_refc_50", "Probability of Composite Reflectivity of 50 dBZ or more", 50.0));
            {
                auto x = make("prob_uphl_75", "Probability of 2-5 km Updraft Helicity of 75 or more (last hour)");
                x.needs = [members, hourly] (int hour) {
                    std::vector<GfsData::Need> out;
                    for (auto w : members("h", "MXUPHL", "5000-2000 m above ground", hourly(hour))) {
                        out.push_back({hour, w});
                    }
                    return out;
                };
                x.fill = [fraction] (const Grids& g) { return fraction(g, "h", 75.0); };
                x.ramp = probability();
                x.fillTitle = "Chance of rotating updrafts (% of the members, nearby)";
                x.legendStep = 10;
                p.push_back(x);
            }
            {
                auto x = make("max_uphl", "Strongest 2-5 km Updraft Helicity of any member (last hour)");
                x.needs = [members, hourly] (int hour) {
                    std::vector<GfsData::Need> out;
                    for (auto w : members("h", "MXUPHL", "5000-2000 m above ground", hourly(hour))) {
                        out.push_back({hour, w});
                    }
                    return out;
                };
                x.fill = [] (const Grids& g) {
                    auto out = pick(g, "h1");
                    for (size_t i = 0; i < out.values.size(); i++) {
                        float best = out.values[i];
                        for (int m = 2; m <= 5; m++) {
                            const float v = g.at("h" + std::to_string(m)).values[i];
                            best = std::isnan(best) || v > best ? v : best;
                        }
                        out.values[i] = best;
                    }
                    return out;
                };
                x.ramp = updraftHelicity();
                x.fillTitle = "Updraft helicity (m2/s2)";
                x.legendStep = 0;
                p.push_back(x);
            }
            {   // each member's 40 dBZ echoes in a color of its own, so where they agree and where they differ shows
                auto x = make("refc_paintball", "Paintball: Composite Reflectivity of 40 dBZ or more, each member");
                x.wants = members("r", "REFC", "entire atmosphere (considered as a single layer)", "");
                const char * names[5] = {"#1f77b4", "#d62728", "#2ca02c", "#9467bd", "#ff7f0e"};
                const auto above = [] (const char * color) {
                    QColor solid{color};
                    solid.setAlpha(176);
                    return Ramp{{{0, QColor{255, 255, 255, 0}}, {39.99, QColor{255, 255, 255, 0}}, {40.0, solid}, {100, solid}}};
                };
                x.fill = [] (const Grids& g) { return pick(g, "r1"); };
                x.ramp = above(names[0]);
                for (int m = 2; m <= 5; m++) {
                    x.overlays.push_back({"r" + std::to_string(m), above(names[m - 1])});
                }
                x.fillTitle = "Members: 1 blue, 2 red, 3 green, 4 purple, 5 orange (40 dBZ or more)";
                x.legendStep = 0;
                p.push_back(x);
            }
            for (const auto& [id, label, spread] : {std::tuple{"mean_2m_temp", "Ensemble Mean 2 m Temperature", false}, {"spread_2m_temp", "2 m Temperature Spread among the members", true}}) {
                auto x = make(id, label);
                x.wants = members("t", "TMP", "2 m above ground", "");
                x.fill = [spread] (const Grids& g) {
                    auto out = pick(g, "t1");
                    for (size_t i = 0; i < out.values.size(); i++) {
                        double sum = 0.0, square = 0.0;
                        int n = 0;
                        for (int m = 1; m <= 5; m++) {
                            const float v = g.at("t" + std::to_string(m)).values[i];
                            if (!std::isnan(v)) {
                                sum += v;
                                square += static_cast<double>(v) * v;
                                n++;
                            }
                        }
                        out.values[i] = n == 0 ? std::nanf("") : spread ? static_cast<float>(std::sqrt(std::max(0.0, square / n - (sum / n) * (sum / n)))) : static_cast<float>(sum / n);
                    }
                    return out;
                };
                x.ramp = spread ? spreadRamp(5.0) : temperature();
                x.quantity = spread ? Quantity::Other : Quantity::Temperature;
                x.fillTitle = spread ? "Spread of the 2 m temperature (C)" : "Mean 2 m temperature";
                x.legendStep = spread ? 1 : 5;
                p.push_back(x);
            }

            // the combinations: the deterministic RRFS in color, and what the members say about it drawn over it as lines (the model's own run is one thing, the ensemble's chance another)
            const auto rrfsWant = [] (const char * key, const char * variable, const char * level, const std::string& forecast) {
                return GfsData::Want{key, variable, level, forecast, "", "rrfs"};
            };
            const auto withChance = [] (const char * key, const char * title, QColor color) {
                ContourSet c;
                c.key = key;
                c.base = 30.0;
                c.minimum = 30.0;
                c.interval = 30.0;   // the 30, 60 and 90 per cent lines
                c.title = title;
                c.color = color;
                c.width = 2.2;
                return c;
            };
            {
                auto x = make("combo_refc_chance", "RRFS Radar with the Members' Chance of 40 dBZ or more");
                x.wants = members("r", "REFC", "entire atmosphere (considered as a single layer)", "");
                x.wants.push_back(rrfsWant("d", "REFC", "entire atmosphere (considered as a single layer)", ""));
                x.derive = [fraction] (Grids& g, const Context&) { g["chance"] = fraction(g, "r", 40.0); };
                x.fill = [] (const Grids& g) { return pick(g, "d"); };
                x.ramp = reflectivity();
                x.fillTitle = "RRFS composite reflectivity (dBZ)";
                x.legendStep = 10;
                x.contours = {withChance("chance", "Chance of 40 dBZ or more in the 5 members (%)", QColor{20, 20, 20})};
                p.push_back(x);
            }
            {
                auto x = make("combo_uphl_chance", "RRFS Updraft Helicity with the Members' Chance of 75 or more");
                x.needs = [members, rrfsWant, hourly] (int hour) {
                    std::vector<GfsData::Need> out;
                    for (auto w : members("h", "MXUPHL", "5000-2000 m above ground", hourly(hour))) {
                        out.push_back({hour, w});
                    }
                    out.push_back({hour, rrfsWant("d", "MXUPHL", "5000-2000 m above ground", hourly(hour))});
                    return out;
                };
                x.derive = [fraction] (Grids& g, const Context&) { g["chance"] = fraction(g, "h", 75.0); };
                x.fill = [] (const Grids& g) { return pick(g, "d"); };
                x.ramp = updraftHelicity();
                x.fillTitle = "RRFS 2-5 km updraft helicity in the last hour (m2/s2)";
                x.legendStep = 0;
                x.contours = {withChance("chance", "Chance of 75 or more in the 5 members (%)", QColor{20, 20, 20})};
                p.push_back(x);
            }
            {
                auto x = make("combo_temp_spread", "RRFS 2 m Temperature with the Members' Spread");
                x.wants = members("t", "TMP", "2 m above ground", "");
                x.wants.push_back(rrfsWant("d", "TMP", "2 m above ground", ""));
                x.derive = [] (Grids& g, const Context&) {
                    auto out = g["t1"];
                    for (size_t i = 0; i < out.values.size(); i++) {
                        double sum = 0.0, square = 0.0;
                        int n = 0;
                        for (int m = 1; m <= 5; m++) {
                            const float v = g.at("t" + std::to_string(m)).values[i];
                            if (!std::isnan(v)) {
                                sum += v;
                                square += static_cast<double>(v) * v;
                                n++;
                            }
                        }
                        out.values[i] = n == 0 ? std::nanf("") : static_cast<float>(std::sqrt(std::max(0.0, square / n - (sum / n) * (sum / n))));
                    }
                    g["spread"] = GfsGrid::smoothed(out, 2);
                };
                x.fill = [] (const Grids& g) { return pick(g, "d"); };
                x.ramp = temperature();
                x.quantity = Quantity::Temperature;
                x.fillTitle = "RRFS 2 m temperature";
                x.legendStep = 5;
                ContourSet spread;
                spread.key = "spread";
                spread.base = 1.0;
                spread.minimum = 1.0;
                spread.interval = 1.0;
                spread.title = "Spread of the members (C)";
                spread.color = QColor{20, 20, 20};
                spread.width = 1.6;
                x.contours = {spread};
                p.push_back(x);
            }
            // the ready-made products: the window is (hour - period) to the hour, as the file writes it
            const auto window = [] (int hour, int period) { return std::to_string(std::max(hour - period, 0)) + "-" + std::to_string(hour) + " hour acc fcst"; };
            for (const auto period : {1, 3}) {
                auto x = make(period == 1 ? "mean_precip_p01" : "mean_precip_p03", period == 1 ? "Ensemble Mean Precipitation, 1 hour" : "Ensemble Mean Precipitation, 3 hours");
                x.needs = [window, period] (int hour) { return std::vector<GfsData::Need>{{hour, {"a", "APCP", "surface", window(hour, period), "wt ens mean", "ens:avrg"}}}; };
                x.fill = [] (const Grids& g) { return pick(g, "a"); };
                x.ramp = precipitation();
                x.quantity = Quantity::Millimeters;
                x.fillTitle = period == 1 ? "Mean precipitation in the last hour" : "Mean precipitation in the last 3 hours";
                x.legendStep = 0;
                p.push_back(x);
            }
            struct Prob {
                const char * id;
                const char * variable;
                int period;
                const char * detail;
                const char * title;
            };
            for (const auto& q : {Prob{"prob_precip_1h_0.25in", "APCP", 1, "prob >6.35", "Probability of 0.25 in or more of rain in an hour"}, Prob{"prob_precip_1h_0.5in", "APCP", 1, "prob >12.7", "Probability of 0.5 in or more of rain in an hour"},
                                  Prob{"prob_precip_1h_1in", "APCP", 1, "prob >25.4", "Probability of 1 in or more of rain in an hour"}, Prob{"prob_precip_3h_0.5in", "APCP", 3, "prob >12.7", "Probability of 0.5 in or more of rain in 3 hours"},
                                  Prob{"prob_precip_3h_1in", "APCP", 3, "prob >25.4", "Probability of 1 in or more of rain in 3 hours"}, Prob{"prob_precip_3h_2in", "APCP", 3, "prob >50.8", "Probability of 2 in or more of rain in 3 hours"},
                                  Prob{"prob_snow_3h_1in", "ASNOW", 3, "prob >0.025", "Probability of 1 in or more of snow in 3 hours"}, Prob{"prob_snow_3h_3in", "ASNOW", 3, "prob >0.076", "Probability of 3 in or more of snow in 3 hours"}}) {
                auto x = make(q.id, q.title);
                const auto variable = std::string{q.variable};
                const auto detail = std::string{q.detail};
                const int period = q.period;
                x.needs = [window, variable, detail, period] (int hour) { return std::vector<GfsData::Need>{{hour, {"a", variable, "surface", window(hour, period), detail, "ens:eas"}}}; };
                x.fill = [] (const Grids& g) { return GfsGrid::scaled(pick(g, "a"), 100.0); };   // a fraction -> percent
                x.ramp = probability();
                x.fillTitle = "Chance (%, within the neighborhood)";
                x.legendStep = 10;
                p.push_back(x);
            }
        };

        // ---- the wave models (GFS-Wave and the GEFS-Wave ensemble mean): each wave component (all of the sea, the wind sea, the three swells) as a height map with the wind, and as the period
        // with the direction the waves come from drawn as streamlines
        const auto waveRecipes = [&] (const char * model) {
            struct Component {
                const char * name;
                const char * label;
                const char * heightId;
                const char * directionId;
                const char * height;
                const char * period;
                const char * direction;
                const char * level;
            };
            for (const auto& c : {Component{"Significant Wave Height", "Significant Wave Height and Wind", "sig_wv_ht", "peak_dir_per", "HTSGW", "PERPW", "DIRPW", "surface"},
                                  Component{"Wind Sea", "Wind Sea Wave Height and Wind", "wsea_wv_ht", "wsea_dir_per", "WVHGT", "WVPER", "WVDIR", "surface"},
                                  Component{"Primary Swell", "Primary Swell Wave Height and Wind", "swell1_wv_ht", "swell1_dir_per", "SWELL", "SWPER", "SWDIR", "1 in sequence"},
                                  Component{"Secondary Swell", "Secondary Swell Wave Height and Wind", "swell2_wv_ht", "swell2_dir_per", "SWELL", "SWPER", "SWDIR", "2 in sequence"},
                                  Component{"Tertiary Swell", "Tertiary Swell Wave Height and Wind", "swell3_wv_ht", "swell3_dir_per", "SWELL", "SWPER", "SWDIR", "3 in sequence"}}) {
                {
                    Product x;
                    x.source = model;
                    x.id = c.heightId;
                    x.label = c.label;
                    x.wants = {want("h", c.height, c.level), want("u", "UGRD", "surface"), want("v", "VGRD", "surface")};
                    x.fill = [] (const Grids& g) { return pick(g, "h"); };
                    x.ramp = waveHeight();
                    x.quantity = Quantity::Meters;
                    x.fillTitle = std::string{c.name} + (std::string{c.heightId} == "sig_wv_ht" ? "" : " height");
                    x.legendStep = 0;
                    x.barbU = "u";
                    x.barbV = "v";
                    p.push_back(x);
                }
                {
                    Product x;
                    x.source = model;
                    x.id = c.directionId;
                    x.label = std::string{c.name} + " Direction and Period (sec)";
                    x.wants = {want("t", c.period, c.level), want("d", c.direction, c.level)};
                    x.derive = [] (Grids& g, const Context&) {   // the direction the waves come from, as a flow toward where they go
                        auto u = g["d"], v = g["d"];
                        for (size_t i = 0; i < u.values.size(); i++) {
                            const double from = g["d"].values[i] * pi / 180.0;
                            u.values[i] = std::isnan(g["d"].values[i]) ? std::nanf("") : static_cast<float>(-std::sin(from));
                            v.values[i] = std::isnan(g["d"].values[i]) ? std::nanf("") : static_cast<float>(-std::cos(from));
                        }
                        g["su"] = u;
                        g["sv"] = v;
                    };
                    x.fill = [] (const Grids& g) { return pick(g, "t"); };
                    x.ramp = wavePeriod();
                    x.fillTitle = std::string{c.name} + " period (s); lines: direction the waves travel";
                    x.legendStep = 2;
                    x.streamU = "su";
                    x.streamV = "sv";
                    p.push_back(x);
                }
            }
        };

        // ---- HAFS: the hurricane model for one storm (the screen picks the storm): wind, simulated radar and satellite, rain, sea surface temperature, shear, waves
        const auto hafsRecipes = [&] (const char * model) {
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
            {   // the swaths: the strongest wind (and gust) of the run so far, and the strongest rotation in each 3 hours (from the file of the whole parent domain)
                const auto window = [] (int hour) { return hour % 24 == 0 ? "0-" + std::to_string(hour / 24) + " day max fcst" : "0-" + std::to_string(hour) + " hour max fcst"; };
                const auto swath = [&] (const char * id, const char * label, const char * variable, const char * title) {
                    auto x = make(id, label);
                    x.needs = [window, variable] (int hour) {
                        return std::vector<GfsData::Need>{{hour, {"w", variable, "10-10 m above ground", window(hour), "", "swath"}}};
                    };
                    x.fill = [] (const Grids& g) { return GfsGrid::scaled(pick(g, "w"), msToKnots); };
                    x.ramp = tropicalWind();
                    x.fillTitle = title;
                    x.legendStep = 0;
                    return x;
                };
                p.push_back(swath("swath_wind", "Swath: Strongest 10m Wind so far", "WIND", "Strongest 10 m wind since the start of the run (kt)"));
                p.push_back(swath("swath_gust", "Swath: Strongest 10m Gust so far", "GUST", "Strongest 10 m gust since the start of the run (kt)"));
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
        };

        // The models in the registry's order: each one's clone of the GFS charts, then the charts of its own
        const std::map<std::string, std::function<void()>> own{{"GEFS", gefsRecipes}, {"RRFS", rrfsRecipes}, {"REFS", refsRecipes}, {"GFS-WAVE", [&] { waveRecipes("GFS-WAVE"); }}, {"GEFS-WAVE", [&] { waveRecipes("GEFS-WAVE"); }}, {"HAFSA", [&] { hafsRecipes("HAFSA"); }}, {"HAFSB", [&] { hafsRecipes("HAFSB"); }}};
        for (const auto& def : GfsModels::all()) {
            if (def.clone.enabled) {
                cloneFrom(def);
            }
            const auto found = own.find(def.id);
            if (found != own.end()) {
                found->second();
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
    const auto * def = GfsModels::find(source);
    return def ? def->label : "NOAA/NCEP GFS 0.25 degree";
}

std::vector<std::string> sectorIds(const std::string& source) {
    const auto * def = GfsModels::find(source);
    if (def && def->sectors == GfsModels::Sectors::Conus) {   // the contiguous United States and its regions
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
                o.sources = GfsModels::overlayModels();
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
                o.sources = GfsModels::overlayModels();
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
                o.sources = GfsModels::overlayModels();
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
                o.sources = GfsModels::overlayModels();
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
                o.sources = GfsModels::overlayModels();
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

std::string category(const Product& product) {
    const auto& id = product.id;
    const auto has = [&id] (const char * text) { return id.find(text) != std::string::npos; };
    if (id.compare(0, 7, "spread_") == 0) {
        return "Ensemble spread";
    }
    if (id.compare(0, 5, "swath") == 0) {
        return "Swaths";
    }
    if (has("trend") || has("chng")) {
        return "Changes and trends";
    }
    if (has("anom")) {
        return "Anomalies";
    }
    if (id.compare(0, 4, "sat_") == 0) {
        return "Satellite";
    }
    if (id == "sst" || id == "waves" || has("wave")) {
        return "Ocean and waves";
    }
    if (has("shear") || has("steering") || has("div") || has("stream") || has("thetae") || has("vor") || has("trop") || has("850vor")) {
        return "Tropical and dynamics";
    }
    if (has("cape") || has("helicity") || has("hlcy") || has("radar") || has("reflectivity") || has("echo") || has("lightning") || has("updraft") || has("srh") || has("stp") || has("scp")) {
        return "Storms and severe";
    }
    if (has("vis") || has("ceil") || has("fog") || has("flight") || has("haines") || has("mix")) {
        return "Aviation and visibility";
    }
    if (has("precip") || has("snow") || has("rain") || has("qpf") || has("ptype") || has("thick") || has("pwat") || has("ice")) {
        return "Precipitation and moisture";
    }
    if (id.size() > 3 && std::isdigit(static_cast<unsigned char>(id[0])) && (has("_wnd_ht") || has("_temp") || has("_rh") || has("rh_700") || has("_ht") || id.compare(1, 2, "00") == 0 || id.compare(0, 3, "250") == 0 || id.compare(0, 3, "925") == 0) && id.compare(0, 3, "10m") != 0 && id.compare(0, 2, "2m") != 0) {
        return "Upper air";
    }
    return "Surface";
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
    const bool seaLevelAsMslet = base.source == "RRFS";   // the Rapid Refresh calls its sea level pressure MSLET
    const auto add = [used, seaLevelAsMslet] (std::vector<GfsData::Need> needs, int hour) {
        for (const auto * o : used) {
            for (auto need : o->needs(hour)) {
                need.want.key = overlayPrefix(*o) + need.want.key;
                if (seaLevelAsMslet && need.want.variable == "PRMSL") {
                    need.want.variable = "MSLET";
                }
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
            case Quantity::Meters: return value * 3.28084;   // wave heights in feet
            default: return value;
        }
    }
    QString unitText(Quantity quantity, bool us) {
        switch (quantity) {
            case Quantity::Temperature: return us ? "°F" : "°C";
            case Quantity::Millimeters: return us ? "in" : "mm";
            case Quantity::Centimeters: return us ? "in" : "cm";
            case Quantity::Meters: return us ? "ft" : "m";
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
        for (double level = std::max(std::ceil((lo - set.base) / interval) * interval + set.base, std::ceil((set.minimum - set.base) / interval) * interval + set.base); level <= hi; level += interval) {
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
                        halo(mid, interval < 1.0 ? QString::number(level, 'f', interval < 0.1 ? 2 : 1) : QString::number(static_cast<int>(std::lround(level))), color.darker(130));   // a fractional interval keeps its decimals
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
    // the other model's track under it, dashed
    if (!options.trackOther.empty()) {
        QPolygonF other;
        for (const auto& t : options.trackOther) {
            other << view.toPixel(t.lon, t.lat);
        }
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen{QColor{255, 255, 255, 220}, 4.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin});
        p.drawPolyline(other);
        p.setPen(QPen{QColor{20, 90, 220}, 2.0, Qt::DashLine, Qt::RoundCap, Qt::RoundJoin});
        p.drawPolyline(other);
        for (const auto& t : options.trackOther) {
            if (t.hour % 24 == 0) {
                const auto at = view.toPixel(t.lon, t.lat);
                p.setPen(QPen{QColor{20, 90, 220}, 1.5});
                p.setBrush(QColor{20, 90, 220});
                p.drawEllipse(at, 3.0, 3.0);
            }
        }
    }
    // a storm track with the wind radii of the hour shown
    if (!options.track.empty()) {
        const auto destination = [] (double lat, double lon, double bearing, double nm) {
            const double d = nm * 1.852 / 6371.0, b = bearing * pi / 180.0, la = lat * pi / 180.0;
            const double lat2 = std::asin(std::sin(la) * std::cos(d) + std::cos(la) * std::sin(d) * std::cos(b));
            const double lon2 = lon * pi / 180.0 + std::atan2(std::sin(b) * std::sin(d) * std::cos(la), std::cos(d) - std::sin(la) * std::sin(lat2));
            return std::pair{lon2 * 180.0 / pi, lat2 * 180.0 / pi};
        };
        const TrackPoint * now = nullptr;
        for (const auto& t : options.track) {
            if (t.hour == forecastHour) {
                now = &t;
            }
        }
        if (now) {   // 64 kt over 50 over 34, each quadrant's radius out along its quarter of the circle
            static const QColor colors[3] = {QColor{255, 235, 0, 70}, QColor{255, 140, 0, 90}, QColor{230, 20, 20, 110}};
            for (int row = 0; row < 3; row++) {
                QPolygonF poly;
                bool any = false;
                for (int q = 0; q < 4; q++) {
                    const double r = now->radii[row][q];
                    any = any || r > 0.0;
                    for (int step = 0; step <= 18; step++) {
                        const double bearing = q * 90.0 + step * 5.0;
                        const auto [lon, lat] = destination(now->lat, now->lon, bearing, r);
                        poly << (r > 0.0 ? view.toPixel(lon, lat) : view.toPixel(now->lon, now->lat));
                    }
                }
                if (any) {
                    p.setPen(QPen{colors[row].darker(160), 1.5});
                    p.setBrush(colors[row]);
                    p.drawPolygon(poly);
                }
            }
        }
        QPolygonF line;
        for (const auto& t : options.track) {
            line << view.toPixel(t.lon, t.lat);
        }
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen{QColor{255, 255, 255, 230}, 4.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin});
        p.drawPolyline(line);
        p.setPen(QPen{QColor{20, 20, 20}, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin});
        p.drawPolyline(line);
        QFont small = p.font();
        small.setPixelSize(11);
        small.setBold(true);
        p.setFont(small);
        for (const auto& t : options.track) {
            if (t.hour % 12 != 0 && &t != now) {
                continue;
            }
            const auto at = view.toPixel(t.lon, t.lat);
            const bool current = &t == now;
            p.setPen(QPen{QColor{20, 20, 20}, 1.5});
            p.setBrush(current ? QColor{255, 255, 255} : QColor{20, 20, 20});
            p.drawEllipse(at, current ? 6.0 : 3.5, current ? 6.0 : 3.5);
            if (current || t.hour % 24 == 0) {
                halo(at + QPointF{9.0, -7.0}, QString{"+%1 h  %2 kt  %3 mb"}.arg(t.hour).arg(t.wind).arg(t.pressure), QColor{20, 20, 20});
            }
        }
    }
    if (!options.track.empty() && !options.trackName.empty()) {   // the key of the tracks
        QFont keyFont = p.font();
        keyFont.setPixelSize(12);
        keyFont.setBold(true);
        p.setFont(keyFont);
        const QString first = QString::fromStdString(options.trackName), second = QString::fromStdString(options.trackOtherName);
        const double width = 60.0 + QFontMetricsF{keyFont}.horizontalAdvance(first) + (options.trackOther.empty() ? 0.0 : 40.0 + QFontMetricsF{keyFont}.horizontalAdvance(second));
        const QRectF box{area.right() - width - 8.0, area.top() + 8.0, width, 24.0};
        p.setPen(Qt::NoPen);
        p.setBrush(QColor{255, 255, 255, 215});
        p.drawRoundedRect(box, 4.0, 4.0);
        double x = box.left() + 8.0;
        const double y = box.center().y();
        p.setPen(QPen{QColor{20, 20, 20}, 2.0});
        p.drawLine(QPointF{x, y}, QPointF{x + 22.0, y});
        p.drawText(QPointF{x + 28.0, y + 4.0}, first);
        x += 28.0 + QFontMetricsF{keyFont}.horizontalAdvance(first) + 14.0;
        if (!options.trackOther.empty()) {
            p.setPen(QPen{QColor{20, 90, 220}, 2.0, Qt::DashLine});
            p.drawLine(QPointF{x, y}, QPointF{x + 22.0, y});
            p.setPen(QColor{20, 20, 20});
            p.drawText(QPointF{x + 28.0, y + 4.0}, second);
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
