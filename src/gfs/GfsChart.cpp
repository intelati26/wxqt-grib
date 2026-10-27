// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "gfs/GfsChart.h"
#include <algorithm>
#include <regex>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <QDateTime>
#include <QFontMetricsF>
#include <QPainter>
#include <QRegularExpression>
#include <QPainterPath>
#include <QPolygonF>
#include <QStringList>
#include <set>
#include <tuple>
#include "cfs/UtilityCfs.h"
#include "gfs/GfsClimate.h"
#include "gfs/GfsModels.h"
#include "spcrefs/UtilitySpcRefs.h"
#include "gfs/BarbGrid.h"
#include "ui/WindBarb.h"

namespace GfsChart {
namespace {

// the number without trailing zeros: 2, 0.25
std::string trimmed(double value) {
    auto text = QString::number(value, 'f', 2);
    while (text.contains('.') && (text.endsWith('0') || text.endsWith('.'))) {
        text.chop(1);
    }
    return text.toStdString();
}

// template id -> a function making the product for the chosen values (filled when the registry is built)
std::map<std::string, std::function<Product(const std::vector<double>&)>>& generators() {
    static std::map<std::string, std::function<Product(const std::vector<double>&)>> table;
    return table;
}

std::string generatedId(const std::string& templateId, const std::vector<double>& values) {
    std::string id = "gen~" + templateId;
    for (const double v : values) {
        id += "~" + trimmed(v);
    }
    return id;
}

// ---- ingredients: the conditions of a pattern (CAPE over so much, shear over so much ...) as parts, for the "ingredients" charts: on a single run, where each point meets how many of them; on the
// ensemble, the share of the members that meet all of them together
struct IngredientReq {
    const char * key;
    const char * variable;
    const char * level;
    int kind;   // 0: the field at the hour, 1: the 6 hour amount ending at it
};
struct Ingredient {
    const char * name;
    const char * unit;
    std::vector<double> limits;
    std::vector<IngredientReq> reqs;
    bool single, ensemble;
    std::vector<std::string> only;   // the single run models that have it (empty: every one)
    std::function<GfsGrid::Grid(const std::function<const GfsGrid::Grid&(const char *)>&)> value;   // the field in the unit of the limits, from the records by their keys
};
GfsGrid::Grid combine2(const GfsGrid::Grid& a, const GfsGrid::Grid& b, const std::function<float(float, float)>& f) {
    auto out = a;
    for (size_t i = 0; i < out.values.size(); i++) {
        out.values[i] = i < b.values.size() ? f(a.values[i], b.values[i]) : std::nanf("");
    }
    return out;
}
GfsGrid::Grid mapped(const GfsGrid::Grid& a, const std::function<float(float)>& f) {
    auto out = a;
    for (auto& v : out.values) {
        v = std::isnan(v) ? v : f(v);
    }
    return out;
}
const std::vector<Ingredient>& ingredients() {
    static const std::vector<Ingredient> all{
        {"MUCAPE", "J/kg", {250, 500, 1000, 1500, 2000, 3000}, {{"c", "CAPE", "255-0 mb above ground", 0}}, true, true, {}, [] (const auto& get) { return get("c"); }},
        {"SBCAPE", "J/kg", {250, 500, 1000, 1500, 2000, 3000}, {{"c", "CAPE", "surface", 0}}, true, true, {}, [] (const auto& get) { return get("c"); }},
        {"Sfc-500 mb shear", "kt", {20, 30, 40, 50, 60}, {{"u0", "UGRD", "10 m above ground", 0}, {"v0", "VGRD", "10 m above ground", 0}, {"u5", "UGRD", "500 mb", 0}, {"v5", "VGRD", "500 mb", 0}},
         true, true, {}, [] (const auto& get) {
             const auto du = combine2(get("u5"), get("u0"), [] (float a, float b) { return a - b; });
             const auto dv = combine2(get("v5"), get("v0"), [] (float a, float b) { return a - b; });
             return combine2(du, dv, [] (float a, float b) { return static_cast<float>(std::hypot(a, b) * 1.943844); });
         }},
        {"0-6 km shear", "kt", {20, 30, 40, 50}, {{"u", "VUCSH", "0-6000 m above ground", 0}, {"v", "VVCSH", "0-6000 m above ground", 0}}, true, false, {"RRFS", "HRRR"},
         [] (const auto& get) { return combine2(get("u"), get("v"), [] (float a, float b) { return static_cast<float>(std::hypot(a, b) * 1.943844); }); }},
        {"0-1 km SRH", "m2/s2", {100, 150, 250, 400}, {{"h", "HLCY", "1000-0 m above ground", 0}}, true, false, {"RRFS", "HRRR"}, [] (const auto& get) { return get("h"); }},
        {"Precip. water", "in", {1.0, 1.25, 1.5, 1.75, 2.0}, {{"w", "PWAT", "entire atmosphere (considered as a single layer)", 0}}, true, true, {}, [] (const auto& get) { return mapped(get("w"), [] (float v) { return v / 25.4f; }); }},
        {"2 m dew point", "F", {50, 55, 60, 65, 70}, {{"d", "DPT", "2 m above ground", 0}}, true, true, {}, [] (const auto& get) { return mapped(get("d"), [] (float v) { return v * 1.8f + 32.0f; }); }},
        {"Rain rate", "in/h", {0.01, 0.05, 0.1, 0.25, 0.5}, {{"r", "PRATE", "surface", 0}}, true, false, {}, [] (const auto& get) { return mapped(get("r"), [] (float v) { return v * 3600.0f / 25.4f; }); }},
        {"6-h rain", "in", {0.01, 0.05, 0.1, 0.25, 0.5, 1.0}, {{"a", "APCP", "surface", 1}}, false, true, {}, [] (const auto& get) { return mapped(get("a"), [] (float v) { return v / 25.4f; }); }},
    };
    return all;
}
struct IngredientCondition {
    size_t ingredient;
    double limit;
};
// the conditions a chart of this kind can be made of, in the order they are listed
std::vector<IngredientCondition> ingredientConditions(bool ensemble, const std::string& source) {
    std::vector<IngredientCondition> out;
    for (size_t i = 0; i < ingredients().size(); i++) {
        const auto& ing = ingredients()[i];
        if (ensemble ? !ing.ensemble : !(ing.single && (ing.only.empty() || std::find(ing.only.begin(), ing.only.end(), source) != ing.only.end()))) {
            continue;
        }
        for (const double limit : ing.limits) {
            out.push_back({i, limit});
        }
    }
    return out;
}
// the ingredients' names alone, for a title (the limits are in the line under it): "MUCAPE + 0-6 km shear + rain rate"
std::string ingredientNames(const std::vector<IngredientCondition>& chosen) {
    std::string out;
    for (const auto& c : chosen) {
        out += (out.empty() ? "" : " + ") + std::string{ingredients()[c.ingredient].name};
    }
    return out;
}
std::string conditionText(const IngredientCondition& c) {
    const auto& ing = ingredients()[c.ingredient];
    return std::string{ing.name} + " > " + trimmed(c.limit) + " " + ing.unit;
}
// the sources that get the single run version
const std::vector<std::string>& ingredientSources() {
    static const std::vector<std::string> all{"GFS", "HRRR", "RRFS"};
    return all;
}

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
    Ramp standardAnomaly() {   // standard deviations: within a quarter of one clear
        return {{{-3, QColor{"#2b4a9a"}}, {-2, QColor{"#3f74b8"}}, {-1, QColor{"#86b4dc"}}, {-0.5, QColor{"#d3e5f2"}}, {-0.25, QColor{211, 229, 242, 0}}, {0.25, QColor{246, 224, 184, 0}},
                 {0.5, QColor{"#f6e0b8"}}, {1, QColor{"#f0b070"}}, {2, QColor{"#d9633a"}}, {3, QColor{"#a02a2a"}}}};
    }
    Ramp temperatureDeparture() {   // degrees C: below the climate in blues, above in warm colors, within half a degree clear
        return {{{-10, QColor{"#2b4a9a"}}, {-6, QColor{"#3f74b8"}}, {-3, QColor{"#86b4dc"}}, {-1, QColor{"#d3e5f2"}}, {-0.5, QColor{211, 229, 242, 0}}, {0.5, QColor{246, 224, 184, 0}},
                 {1, QColor{"#f6e0b8"}}, {3, QColor{"#f0b070"}}, {6, QColor{"#d9633a"}}, {10, QColor{"#a02a2a"}}}};
    }
    Ramp seaLevelPressure() {   // millibars: lows in purples and blues, highs in yellows to reds
        return {{{980, QColor{"#6e3aa0"}}, {995, QColor{"#3a5ac8"}}, {1005, QColor{"#78bee6"}}, {1013, QColor{"#ebebe1"}}, {1020, QColor{"#fad25a"}}, {1030, QColor{"#f08232"}}, {1040, QColor{"#c82828"}}}};
    }
    Ramp precipitationDeparture() {   // millimeters in the period: drier than the climate in browns, wetter in greens, within a few clear
        return {{{-60, QColor{"#7a4a1c"}}, {-30, QColor{"#b07a3a"}}, {-12, QColor{"#dcc08a"}}, {-4, QColor{"#efe3c4"}}, {-2, QColor{239, 227, 196, 0}}, {2, QColor{200, 230, 200, 0}},
                 {4, QColor{"#c8e6c8"}}, {12, QColor{"#8cc99a"}}, {30, QColor{"#3fa06a"}}, {60, QColor{"#1b6e48"}}}};
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
    Ramp lowLevelVorticity() {   // 1e-3 per second: the strongest vertical vorticity of the last hour in the lowest 1-2 km; the weak values clear, a mesocyclone from about 10, a tornadic circulation from 20
        return {{{0, QColor{240, 244, 250, 0}}, {3, QColor{240, 244, 250, 0}}, {5, QColor{"#c4e3a4"}}, {8, QColor{"#7bc47f"}}, {10, QColor{"#f3e04a"}}, {15, QColor{"#f0a63a"}}, {20, QColor{"#e0702e"}},
                 {30, QColor{"#c4262c"}}, {40, QColor{"#a8206b"}}, {60, QColor{"#6a2a9a"}}}};
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

    // ---- the chance of an amount of precipitation, for every ensemble: the share of the members whose total (or amount in a period) is at or over it. The bands are those of the commercial ensemble
    // maps: white under 10 %, up to a deep red over 90 %.
    const Ramp& exceedRamp() {
        static const Ramp chance{{{0.0, QColor{255, 255, 255}}, {10.0, QColor{93, 74, 168}}, {20.0, QColor{45, 120, 184}}, {30.0, QColor{70, 176, 160}}, {40.0, QColor{154, 212, 160}}, {50.0, QColor{244, 244, 176}},
                                  {60.0, QColor{246, 216, 138}}, {70.0, QColor{248, 168, 90}}, {80.0, QColor{240, 105, 60}}, {90.0, QColor{140, 10, 58}}}, true};
        return chance;
    }

    // An ensemble whose members each have a running total of precipitation since the start of the run (the ECMWF's, the GEPS's): its source, its members' names, and the forecast text of the total at an hour
    struct CumulativeEnsemble {
        std::string source;
        std::vector<std::string> members;
        std::function<std::string(int hour)> forecast;
    };

    // The chart for an amount (inches) and a period (hours; 0 the total since the start of the run): each member's total at the hour, less its total `period` hours before, and the share at or over the amount
    Product exceedProduct(const CumulativeEnsemble& ensemble, const std::string& id, const std::string& label, double inches, int period) {
        const double mm = inches * 25.4 - 1e-6;
        Product x;
        x.source = ensemble.source;
        x.id = id;
        x.label = label;
        x.needs = [ensemble, period] (int hour) {
            std::vector<GfsData::Need> out;
            if (hour < 3 || (period > 0 && hour < period)) {
                return out;
            }
            for (const auto& name : ensemble.members) {
                out.push_back({hour, GfsData::Want{"q" + name, "APCP", "surface", ensemble.forecast(hour), "", name}});
                if (period > 0 && hour - period > 0) {   // the totals are from the start: the period's amount is the difference of two of them
                    out.push_back({hour - period, GfsData::Want{"r" + name, "APCP", "surface", ensemble.forecast(hour - period), "", name}});
                }
            }
            return out;
        };
        x.derive = [ensemble, mm, period] (Grids& g, const Context& context) {
            auto out = g.at("q" + ensemble.members.front());
            const bool difference = period > 0 && context.hour - period > 0;
            for (size_t i = 0; i < out.values.size(); i++) {
                int n = 0, hits = 0;
                for (const auto& name : ensemble.members) {
                    float v = g.at("q" + name).values[i];
                    if (difference) {
                        v -= g.at("r" + name).values[i];
                    }
                    if (std::isfinite(v)) {
                        n++;
                        hits += v >= mm ? 1 : 0;
                    }
                }
                out.values[i] = n > 0 ? static_cast<float>(100.0 * hits / n) : std::nanf("");
            }
            g["qp"] = std::move(out);
            for (const auto& name : ensemble.members) {
                g.erase("q" + name);
                g.erase("r" + name);
            }
        };
        x.fill = [] (const Grids& g) { return pick(g, "qp"); };
        x.ramp = exceedRamp();
        x.legendStep = 10;
        return x;
    }

    // the same for the members pooled from several ensembles (ENS-ALL): GfsRender reads each member's total, or its amount in the period, onto one grid ("q" + the member's tag)
    Product exceedPooledProduct(const std::string& id, const std::string& label, double inches, int period) {
        const double mm = inches * 25.4 - 1e-6;
        Product x;
        x.source = "ENS-ALL";
        x.id = id;
        x.label = label;
        x.pool = {"APCP", "surface", "q"};
        x.pool.periodHours = period;
        x.derive = [mm] (Grids& g, const Context&) {
            std::vector<const GfsGrid::Grid *> members;
            for (const auto& member : poolMembers()) {
                const auto found = g.find("q" + member.tag);
                if (found != g.end() && !found->second.values.empty()) {
                    members.push_back(&found->second);
                }
            }
            if (members.empty()) {
                throw std::runtime_error{"no member could be read"};
            }
            auto out = *members.front();
            for (size_t i = 0; i < out.values.size(); i++) {
                int n = 0, hits = 0;
                for (const auto * m : members) {
                    const float v = m->values[i];
                    if (std::isfinite(v)) {
                        n++;
                        hits += v >= mm ? 1 : 0;
                    }
                }
                out.values[i] = n > 0 ? static_cast<float>(100.0 * hits / n) : std::nanf("");
            }
            g["qp"] = std::move(out);
            for (const auto& member : poolMembers()) {
                g.erase("q" + member.tag);
            }
        };
        x.fill = [] (const Grids& g) { return pick(g, "qp"); };
        x.ramp = exceedRamp();
        x.legendStep = 10;
        return x;
    }
}

QRgb Ramp::at(double value) const {
    if (stops.empty() || std::isnan(value)) {
        return qRgba(0, 0, 0, 0);
    }
    if (banded) {
        if (value < stops.front().first) {
            return qRgba(0, 0, 0, 0);
        }
        size_t at = 0;
        for (size_t i = 1; i < stops.size() && stops[i].first <= value; i++) {
            at = i;
        }
        return stops[at].second.rgba();
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
        {"C-PLAINS", -106, 34, -90, 44},
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

std::string localId(double lat, double lon, double km, const std::string& name) {
    std::string clean = name;
    for (auto& c : clean) {
        if (c == ',' || c == '|' || c == ':') {
            c = ' ';
        }
    }
    char text[64];
    std::snprintf(text, sizeof text, "%.3f,%.3f,%d,", lat, lon, static_cast<int>(std::lround(km)));
    return std::string{"LOCAL:"} + text + clean;
}

bool parseLocalId(const std::string& id, double& lat, double& lon, double& km, std::string& name) {
    if (id.compare(0, 6, "LOCAL:") != 0) {
        return false;
    }
    const auto parts = QString::fromStdString(id.substr(6)).split(',');
    if (parts.size() < 3) {
        return false;
    }
    bool a = false, b = false, c = false;
    lat = parts[0].toDouble(&a);
    lon = parts[1].toDouble(&b);
    km = parts[2].toDouble(&c);
    name = parts.size() > 3 ? parts.mid(3).join(',').toStdString() : std::string{};
    return a && b && c && km > 10.0 && std::fabs(lat) <= 90.0 && std::fabs(lon) <= 360.0;
}

std::string boxId(double west, double south, double east, double north) {
    char text[96];
    std::snprintf(text, sizeof text, "BOX:%.3f,%.3f,%.3f,%.3f", west, south, east, north);
    return text;
}

bool parseBoxId(const std::string& id, Sector& out) {
    if (id.compare(0, 4, "BOX:") != 0) {
        return false;
    }
    const auto parts = QString::fromStdString(id.substr(4)).split(',');
    if (parts.size() != 4) {
        return false;
    }
    bool ok[4];
    const double west = parts[0].toDouble(&ok[0]), south = parts[1].toDouble(&ok[1]), east = parts[2].toDouble(&ok[2]), north = parts[3].toDouble(&ok[3]);
    if (!ok[0] || !ok[1] || !ok[2] || !ok[3] || !(east - west > 0.2) || !(north - south > 0.2) || west < -180.0 || east > 180.0 || south < -85.0 || north > 85.0) {
        return false;
    }
    out = Sector{id, west, south, east, north};
    return true;
}

Sector localSector(const std::string& id, double west, double south, double east, double north) {
    double lat = 0, lon = 0, km = 0;
    std::string name;
    Sector out{id, west, south, east, north};
    if (!parseLocalId(id, lat, lon, km, name)) {
        return out;
    }
    const double halfLat = km / 111.32 / 2.0;
    const double halfLon = km / (111.32 * std::max(std::cos(lat * pi / 180.0), 0.2)) / 2.0;
    // a window of that size round the place, moved inside the coverage (not narrowed: the same kilometers wherever it is)
    const auto place = [] (double center, double half, double low, double high, double& from, double& to) {
        if (2.0 * half >= high - low) {
            from = low;
            to = high;
            return;
        }
        from = center - half;
        to = center + half;
        if (from < low) {
            to += low - from;
            from = low;
        }
        if (to > high) {
            from -= to - high;
            to = high;
        }
    };
    place(lat, halfLat, std::max(south, -85.0), std::min(north, 85.0), out.south, out.north);
    place(lon, halfLon, west, east, out.west, out.east);
    return out;
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
            x.palette = "isotach";
            x.barbU.clear();   // the model guidance site draws the isotachs and the heights only
            x.barbV.clear();
            p.push_back(x);
        }
        {
            auto x = upper("250_wnd_ht", "250mb Wind and Height", "250 mb", 12);
            x.palette = "isotach";
            x.barbU.clear();   // the model guidance site draws the isotachs and the heights only
            x.barbV.clear();
            p.push_back(x);
        }
        {
            auto x = upper("300_wnd_ht", "300mb Wind and Height", "300 mb", 12);
            x.palette = "isotach";
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
            x.palette = "rh";
            x.fillTitle = "Relative humidity (%)";
            x.legendStep = 10;
            p.push_back(x);
            {   // the lower level humidity: the mean of the 850 and 700 mb relative humidity, for the severe weather forecaster's "moist enough in the lower levels"; the winds and heights come from the stacked overlays
                Product m;
                m.id = "850_700_rh";
                m.label = "850-700mb Mean Relative Humidity";
                m.wants = {want("r8", "RH", "850 mb"), want("r7", "RH", "700 mb")};
                m.derive = [] (Grids& g, const Context&) {
                    auto mean = g.at("r8");
                    const auto& upper = g.at("r7");
                    for (size_t i = 0; i < mean.values.size() && i < upper.values.size(); i++) {
                        mean.values[i] = 0.5f * (mean.values[i] + upper.values[i]);
                    }
                    g["rh"] = std::move(mean);
                };
                m.fill = [] (const Grids& g) { return pick(g, "rh"); };
                m.ramp = humidity();   // (our own scale, dry brown to moist teal: the site's two green bands would be lost under the green 700 mb lines)
                m.fillTitle = "Mean relative humidity of 850 and 700 mb (%)";
                m.legendStep = 10;
                p.push_back(m);
            }
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
            omega.smoothKm = 30.0;
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
        // the CFS's own normal (NOAA's calibration climatologies of its reforecasts, the 1999-2010 or 1982-2010 months blended by the day) in place of the reanalysis' for the CFS models: the anomaly of
        // a model is best taken from its own climate. `toUnit` takes the shown unit to the grid's (hPa to Pa). false: another model, or the files are not there (the reanalysis' normal is then used)
        const auto cfsNormal = [] (const Context& context, const char * key, double toUnit, GfsGrid::Grid& out) {
            if (context.source != "CFS" && context.source != "CFS-ANL") {
                return false;
            }
            const auto * variable = UtilityCfs::find(key);
            if (variable == nullptr) {
                return false;
            }
            auto t = QDateTime::fromString(QString::fromStdString(context.run.id()), "yyyyMMddHH");
            t.setTimeSpec(Qt::UTC);
            t = t.addSecs(static_cast<qint64>(context.hour) * 3600);
            std::string error;
            const auto run = context.run;
            if (!UtilityCfs::normalAt(*variable, t, context.source == "CFS" ? &run : nullptr, Utility::readPrefInt("MM_CFS_BASE", 1999) == 1982 ? 1982 : 1999, out, error)) {
                return false;
            }
            GfsGrid::closeWrapGap(out);   // (the file's grid warped onto -180..180 has no value in its first and last columns)
            for (auto& v : out.values) {
                v = static_cast<float>(v * toUnit);
            }
            return true;
        };
        // anomalies: the forecast less the 1991-2020 daily mean for the valid day (GfsClimate), where the heights of the level are real
        const auto validDay = [] (const Context& context) {
            auto t = QDateTime::fromString(QString::fromStdString(context.run.id()), "yyyyMMddHH");
            t.setTimeSpec(Qt::UTC);
            t = t.addSecs(context.hour * 3600);
            return GfsClimate::dayIndex(t.date().year(), t.date().month(), t.date().day());
        };
        // the anomaly over the day's standard deviation, on the forecast's grid: "anom" in deviations, "sd_size" and "anom_unit" in the chart's own unit (scale: meters to dam, pascals to mb)
        const auto sigmaOf = [validDay, cfsNormal] (const GfsClimate::Field& field, double scale, bool maskUnderground, const char * cfsKey, double cfsToUnit) {
            return [validDay, cfsNormal, field, scale, maskUnderground, cfsKey, cfsToUnit] (Grids& g, const Context& context) {
                if (!context.climate) {
                    throw std::runtime_error{"no climatology"};
                }
                GfsGrid::Grid normal, deviation;
                std::string error;
                const int day = validDay(context);
                if (!context.climate->at(field, day, normal, error) || !context.climate->deviation(field, day, deviation, error)) {
                    throw std::runtime_error{error};
                }
                if (cfsKey != nullptr) {   // the anomaly from the CFS's own normal; the spread stays the reanalysis' daily one (the CFS file's is the spread of monthly means)
                    GfsGrid::Grid own;
                    if (cfsNormal(context, cfsKey, cfsToUnit, own)) {
                        normal = std::move(own);
                    }
                }
                auto anom = GfsGrid::anomaly(g["z"], normal);
                auto zero = g["z"];
                std::fill(zero.values.begin(), zero.values.end(), 0.0f);
                auto size = GfsGrid::scaled(GfsGrid::anomaly(zero, deviation), -1.0);   // one standard deviation on the forecast's grid
                auto inUnit = GfsGrid::scaled(anom, scale);
                for (size_t i = 0; i < anom.values.size(); i++) {
                    const bool under = maskUnderground && g.find("ps") != g.end() && g["ps"].values[i] < 70000.0f;
                    const float sd = size.values[i];
                    anom.values[i] = (sd > 1e-6f && !under) ? anom.values[i] / sd : std::nanf("");
                    if (under) {
                        inUnit.values[i] = std::nanf("");
                    }
                }
                size = GfsGrid::scaled(size, scale);
                g["anom"] = std::move(anom);
                g["sd_size"] = std::move(size);
                g["anom_unit"] = std::move(inUnit);
            };
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
            x.derive = [validDay, cfsNormal, level] (Grids& g, const Context& context) {
                GfsGrid::Grid normal;
                std::string error;
                if (!(level == 500 && cfsNormal(context, "z500", 1.0, normal)) && (!context.climate || !context.climate->at(GfsClimate::height(level), validDay(context), normal, error))) {
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
            x.sigma = sigmaOf(GfsClimate::height(level), 0.1, level == 700, level == 500 ? "z500" : nullptr, 1.0);
            x.sigmaUnit = "dam";
            x.sigmaSizeKey = "sd_size";
            x.sigmaReadKey = "anom_unit";
            x.sigmaInterval = interval;
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
            x.derive = [validDay, cfsNormal] (Grids& g, const Context& context) {
                GfsGrid::Grid normal;
                std::string error;
                if (!cfsNormal(context, "mslp", 100.0, normal) && (!context.climate || !context.climate->at(GfsClimate::seaLevelPressure(), validDay(context), normal, error))) {
                    return;
                }
                g["anom"] = GfsGrid::scaled(GfsGrid::anomaly(g["p"], normal), 0.01);   // Pa -> mb
            };
            x.fill = [] (const Grids& g) { return pick(g, "anom"); };
            x.ramp = pressureAnomaly();
            x.fillTitle = "Pressure anomaly from the 1991-2020 mean (mb)";
            x.legendStep = 5;
            x.sigma = [sigma = sigmaOf(GfsClimate::seaLevelPressure(), 0.01, false, "mslp", 100.0)] (Grids& g, const Context& context) {
                g["z"] = g.at("p");   // the sigma helper reads the field as "z"
                sigma(g, context);
                g.erase("z");
            };
            x.sigmaUnit = "mb";
            x.sigmaSizeKey = "sd_size";
            x.sigmaReadKey = "anom_unit";
            x.sigmaInterval = 4.0;
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
            x.palette = "precip";
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
            x.palette = "precip";
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
            x.palette = "precip";
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
            x.palette = "precip";
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
        // ---- the RTMA and URMA surface analyses (2.5 km, the hour of the analysis only): the blend's surface charts made from the analysis's own fields
        for (const char * source : {"RTMA", "URMA"}) {
            const auto analysisChart = [&, source] (const char * id, const char * label, const char * variable, const char * level, const Ramp& ramp, const char * title, Quantity quantity, double step, bool barbs) {
                Product x;
                x.source = source;
                x.id = id;
                x.label = label;
                x.needs = [=] (int hour) {
                    std::vector<GfsData::Need> needs{{hour, nbmWant("f", variable, level, "anl")}};
                    if (barbs) {
                        needs.push_back({hour, nbmWant("ws", "WIND", "10 m above ground", "anl")});
                        needs.push_back({hour, nbmWant("wd", "WDIR", "10 m above ground", "anl")});
                    }
                    return needs;
                };
                if (barbs) {
                    x.derive = windComponents;
                    x.barbU = "u";
                    x.barbV = "v";
                }
                x.fill = [] (const Grids& g) { return pick(g, "f"); };
                x.ramp = ramp;
                x.fillTitle = title;
                x.quantity = quantity;
                x.legendStep = step;
                return x;
            };
            p.push_back(analysisChart("2m_temp_10m_wnd", "2 m Temperature and 10 m Wind", "TMP", "2 m above ground", temperature(), "2 m temperature", Quantity::Temperature, 5, true));
            p.push_back(analysisChart("2m_dewp_10m_wnd", "2 m Dewpoint and 10 m Wind", "DPT", "2 m above ground", dewpoint(), "2 m dewpoint", Quantity::Temperature, 5, true));
            p.push_back(analysisChart("total_cloud_cover", "Total Cloud Cover and 10 m Wind", "TCDC", "entire atmosphere (considered as a single layer)", cloudCover(), "Total cloud cover (%)", Quantity::Other, 10, true));
            {   // the gust is the fill; the barbs are the sustained wind
                auto x = analysisChart("10m_wnd_gust", "10 m Wind and Gust", "GUST", "10 m above ground", lowWind(), "10 m wind gust (kt)", Quantity::Other, 10, true);
                x.fill = [] (const Grids& g) { return GfsGrid::scaled(pick(g, "f"), msToKnots); };
                p.push_back(x);
            }
            {
                auto x = analysisChart("visibility", "2-meter Visibility", "VIS", "surface", visibilityRamp(), "Visibility (miles)", Quantity::Other, 0, false);
                x.fill = [] (const Grids& g) { return GfsGrid::scaled(pick(g, "f"), 1.0 / 1609.344); };
                x.palette = "vis";
                p.push_back(x);
            }
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
                x.palette = "prob";
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
                x.palette = "prob";
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
                x.palette = "vis";
                x.fillTitle = "Visibility (miles)";
                x.legendStep = 0;
                p.push_back(x);
            }
            {
                auto x = nbmChart("ceiling", "Ceiling");
                x.needs = [=] (int hour) { return std::vector<GfsData::Need>{{hour, nbmWant("f", "CEIL", "cloud ceiling", atHour(hour))}}; };
                x.fill = [] (const Grids& g) { return GfsGrid::scaled(pick(g, "f"), 3.28084); };
                x.ramp = ceilingRamp();
                x.palette = "ceiling";
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
                x.palette = "echo_top";
                x.fillTitle = "Echo top (thousands of feet)";
                x.legendStep = 10;
                p.push_back(x);
            }
            {   // the number of hours with precipitation in the last 12: the hourly amounts counted where there is at least 0.01 inch (the blend has the hourly amounts to hour 36)
                auto x = nbmChart("precip_duration", "Precipitation duration");
                x.needs = [=] (int hour) {
                    std::vector<GfsData::Need> out;
                    for (int k = std::max(hour - 11, 1); k <= hour; k++) {
                        out.push_back({k, nbmWant("d" + std::to_string(k), "APCP", "surface", window(k, 1, "acc"))});
                    }
                    return out;
                };
                x.derive = [] (Grids& g, const Context&) {
                    GfsGrid::Grid count;
                    for (const auto& [key, grid] : g) {
                        if (key.size() >= 2 && key[0] == 'd' && std::isdigit(static_cast<unsigned char>(key[1]))) {
                            if (count.empty()) {
                                count = grid;
                                std::fill(count.values.begin(), count.values.end(), 0.0f);
                            }
                            for (size_t i = 0; i < count.values.size(); i++) {
                                count.values[i] = std::isnan(grid.values[i]) || std::isnan(count.values[i]) ? std::nanf("") : count.values[i] + (grid.values[i] >= 0.254f ? 1.0f : 0.0f);
                            }
                        }
                    }
                    g["count"] = std::move(count);
                };
                x.fill = [] (const Grids& g) { return pick(g, "count"); };
                if (const auto * bands = magPalette("duration")) {
                    x.ramp = *bands;
                }
                x.palette = "duration";
                x.fillTitle = "Hours with precipitation in the last 12 (0.01 in or more in the hour)";
                x.legendStep = 0;
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
            if (x.source == "CFS" || x.source == "CFS-ANL") {   // the CFS charts' anomalies are from its own normal (cfsNormal)
                const std::string was = "from the 1991-2020 mean";
                if (const auto at = x.fillTitle.find(was); at != std::string::npos) {
                    x.fillTitle.replace(at, was.size(), "from the CFS normal (1999-2010)");
                }
            }
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
                    if (pressureLevel && !rule.fields.empty() && !rule.fields.count(w.variable + "|" + w.level)) {
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

            // ---- the maps that count the members: the share of the 31 (the control and the 30) that pass a limit, and the spread of a change. Each is 31 fetches of one field (the cache keeps them).
            static const std::vector<std::string> memberNames = [] {
                std::vector<std::string> names{"c00"};
                for (int i = 1; i <= 30; i++) {
                    names.push_back(std::string{"p"} + (i < 10 ? "0" : "") + std::to_string(i));
                }
                return names;
            }();
            // the same record from every member: the grids are named "<prefix><member>"
            const auto memberNeeds = [] (const char * prefix, const char * variable, const char * level, int hour, const std::string& forecast) {
                std::vector<GfsData::Need> out;
                for (const auto& name : memberNames) {
                    out.push_back({hour, GfsData::Want{std::string{prefix} + name, variable, level, forecast, "", name}});
                }
                return out;
            };
            // The percentage of the members that pass a test at each point (1 passes, 0 does not, -1 no data), into `out`; the members' grids are dropped then, 31 of them are a lot to carry.
            const auto share = [] (Grids& g, const std::function<int(const Grids&, const std::string&, size_t)>& test, const char * out, std::initializer_list<const char *> prefixes) {
                auto result = g.at(std::string{*prefixes.begin()} + memberNames.front());
                for (size_t i = 0; i < result.values.size(); i++) {
                    int n = 0, passed = 0;
                    for (const auto& name : memberNames) {
                        const int r = test(g, name, i);
                        if (r >= 0) {
                            n++;
                            passed += r;
                        }
                    }
                    result.values[i] = n > 0 ? 100.0f * static_cast<float>(passed) / static_cast<float>(n) : std::nanf("");
                }
                g[out] = std::move(result);
                for (const auto * prefix : prefixes) {
                    for (const auto& name : memberNames) {
                        g.erase(std::string{prefix} + name);
                    }
                }
            };
            const auto shareChart = [&] (const char * id, const std::string& label, const std::string& title, std::function<std::vector<GfsData::Need>(int)> needs, std::function<void(Grids&)> derive) {
                Product x;
                x.source = "GEFS";
                x.id = id;
                x.label = label;
                x.needs = std::move(needs);
                x.derive = [derive] (Grids& g, const Context&) { derive(g); };
                x.fill = [] (const Grids& g) { return pick(g, "share"); };
                x.ramp = probability();
                x.palette = "prob";
                x.fillTitle = title;
                x.legendStep = 10;
                return x;
            };
            const auto sixHourAmount = [] (int hour) { return std::to_string(hour - 6) + "-" + std::to_string(hour) + " hour acc fcst"; };
            const auto sixHourAverage = [] (int hour) { return std::to_string(hour - 6) + "-" + std::to_string(hour) + " hour ave fcst"; };
            // ---- made on request: the same maps with a limit and a period of the user's choosing ("rain over 2 in in 36 hours"). The model screen's "Build a chart" lists the templates; the chart's
            // id carries the choice ("gen~rain_over~2~36"), so a view that is saved or restored draws it again, and product() makes it when it is asked for.
            {
                auto& made = generators();
                const auto makeRain = [memberNeeds, share, sixHourAmount] (bool over) { return [memberNeeds, share, sixHourAmount, over] (const std::vector<double>& v) {   // inches, hours
                    const double inches = v.at(0);
                    const int window = std::max(6, static_cast<int>(std::lround(v.at(1) / 6.0)) * 6);
                    Product x;
                    x.source = "GEFS";
                    x.id = generatedId(over ? "rain_over" : "rain_under", {inches, static_cast<double>(window)});
                    x.label = std::string{"Chance of "} + (over ? "more" : "less") + " than " + trimmed(inches) + " in of rain in " + std::to_string(window) + " hours";
                    x.needs = [memberNeeds, sixHourAmount, window] (int hour) {
                        std::vector<GfsData::Need> out;
                        if (hour < 6 || hour % 6 != 0) {   // the members' precipitation is in pieces of 6 hours
                            return out;
                        }
                        for (int end = hour, k = 0; end >= 6 && end > hour - window; end -= 6, k++) {
                            for (auto& need : memberNeeds(("a" + std::to_string(k) + "_").c_str(), "APCP", "surface", end, sixHourAmount(end))) {
                                out.push_back(std::move(need));
                            }
                        }
                        return out;
                    };
                    x.derive = [share, inches, window, over] (Grids& g, const Context& context) {
                        const int pieces = std::min(window, context.hour) / 6;
                        for (const auto& name : memberNames) {   // each member's total over the period
                            auto total = g.at("a0_" + name);
                            for (int k = 1; k < pieces; k++) {
                                const auto& more = g.at("a" + std::to_string(k) + "_" + name);
                                for (size_t i = 0; i < total.values.size(); i++) {
                                    total.values[i] += more.values[i];
                                }
                            }
                            g["t" + name] = std::move(total);
                        }
                        for (auto it = g.begin(); it != g.end();) {   // the pieces are not needed again: 31 members of 10 of them are a lot to carry
                            it = it->first.size() > 1 && it->first[0] == 'a' && std::isdigit(static_cast<unsigned char>(it->first[1])) ? g.erase(it) : std::next(it);
                        }
                        const double mm = inches * 25.4;
                        share(g, [mm, over] (const Grids& grids, const std::string& name, size_t i) {
                            const float a = grids.at("t" + name).values[i];
                            return std::isnan(a) ? -1 : (over ? a > mm : a < mm) ? 1 : 0;
                        }, "share", {"t"});
                    };
                    x.fill = [] (const Grids& g) { return pick(g, "share"); };
                    x.ramp = probability();
                    x.palette = "prob";
                    x.fillTitleFor = [inches, window, over] (int hour) {
                        const int used = std::min(window, hour);
                        return std::string{"Chance of "} + (over ? "more" : "less") + " than " + trimmed(inches) + " in of precipitation in the " + std::to_string(used) + " hours ending at hour " + std::to_string(hour) + " (% of the 31 members)";
                    };
                    x.legendStep = 10;
                    return x;
                }; };
                made["rain_over"] = makeRain(true);
                made["rain_under"] = makeRain(false);
                // The precipitation of a type: the share of the members with a 6 hour amount over a limit that fell mostly as rain, snow, ice pellets or freezing rain
                made["ptype"] = [memberNeeds, share, sixHourAmount] (const std::vector<double>& v) {   // type (1 rain, 2 snow, 3 ice pellets, 4 freezing rain), inches
                    const int type = std::clamp(static_cast<int>(std::lround(v.at(0))), 1, 4);
                    const double inches = v.at(1);
                    static const char * names[] = {"", "rain", "snow", "ice pellets", "freezing rain"};
                    static const char * variables[] = {"", "CRAIN", "CSNOW", "CICEP", "CFRZR"};
                    Product x;
                    x.source = "GEFS";
                    x.id = generatedId("ptype", {static_cast<double>(type), inches});
                    x.label = std::string{"Chance of "} + names[type] + " (6 hours, over " + trimmed(inches) + " in)";
                    x.needs = [memberNeeds, sixHourAmount, type] (int hour) {
                        std::vector<GfsData::Need> out;
                        if (hour < 6 || hour % 6 != 0) {
                            return out;
                        }
                        const auto average = std::to_string(hour - 6) + "-" + std::to_string(hour) + " hour ave fcst";
                        for (auto& need : memberNeeds("a", "APCP", "surface", hour, sixHourAmount(hour))) {
                            out.push_back(std::move(need));
                        }
                        for (auto& need : memberNeeds("k", variables[type], "surface", hour, average)) {
                            out.push_back(std::move(need));
                        }
                        return out;
                    };
                    x.derive = [share, inches] (Grids& g, const Context&) {
                        const float mm = static_cast<float>(inches * 25.4);
                        share(g, [mm] (const Grids& grids, const std::string& name, size_t i) {
                            const float a = grids.at("a" + name).values[i], k = grids.at("k" + name).values[i];
                            return std::isnan(a) || std::isnan(k) ? -1 : (a >= mm && k >= 0.5f) ? 1 : 0;   // the type held most of the 6 hours
                        }, "share", {"a", "k"});
                    };
                    x.fill = [] (const Grids& g) { return pick(g, "share"); };
                    x.ramp = probability();
                    x.palette = "prob";
                    x.fillTitle = std::string{"Chance of "} + names[type] + " in the 6 hours, with at least " + trimmed(inches) + " in of precipitation (% of the 31 members)";
                    x.legendStep = 10;
                    return x;
                };
                // The chance of an anomaly of so many standard deviations: each member's value less the day's mean, over the day's standard deviation (both from the 1991-2020 reanalysis; the
                // standard deviation is worked out on first use, in the background)
                made["stdanom"] = [memberNeeds, share] (const std::vector<double>& v) {   // what (1-5), direction (1 under, 2 over), deviations
                    const int what = std::clamp(static_cast<int>(std::lround(v.at(0))), 1, 5);
                    const bool over = std::lround(v.at(1)) == 2;
                    const double sigmas = v.at(2);
                    static const char * names[] = {"", "a 500 mb height", "a 700 mb height", "a 700 mb temperature", "an 850 mb temperature", "precipitable water"};
                    static const char * variables[] = {"", "HGT", "HGT", "TMP", "TMP", "PWAT"};
                    static const char * levels[] = {"", "500 mb", "700 mb", "700 mb", "850 mb", "entire atmosphere (considered as a single layer)"};
                    Product x;
                    x.source = "GEFS";
                    x.id = generatedId("stdanom", {static_cast<double>(what), over ? 2.0 : 1.0, sigmas});
                    const std::string words = std::string{"Chance of "} + names[what] + " more than " + trimmed(sigmas) + " standard deviation" + (sigmas == 1.0 ? "" : "s") + " " + (over ? "above" : "below") + " normal";
                    x.label = words;
                    x.needs = [memberNeeds, what] (int hour) {
                        if (hour < 3) {
                            return std::vector<GfsData::Need>{};
                        }
                        auto out = memberNeeds("v", variables[what], levels[what], hour, std::to_string(hour) + " hour fcst");
                        if (what == 2 || what == 3 || what == 4) {   // where the surface is above the level, the level is underground: the control run's surface pressure marks it
                            out.push_back({hour, GfsData::Want{"ps", "PRES", "surface", std::to_string(hour) + " hour fcst", "", "c00"}});
                        }
                        return out;
                    };
                    x.derive = [share, what, over, sigmas] (Grids& g, const Context& context) {
                        if (!context.climate) {
                            throw std::runtime_error{"no climatology"};
                        }
                        auto t = QDateTime::fromString(QString::fromStdString(context.run.id()), "yyyyMMddHH");
                        t.setTimeSpec(Qt::UTC);
                        t = t.addSecs(context.hour * 3600LL);
                        const int day = GfsClimate::dayIndex(t.date().year(), t.date().month(), t.date().day());
                        const auto field = what == 1 ? GfsClimate::height(500) : what == 2 ? GfsClimate::height(700) : what == 3 ? GfsClimate::temperature(700) : what == 4 ? GfsClimate::temperature(850) : GfsClimate::precipitableWater();
                        GfsGrid::Grid normal, deviation;
                        std::string error;
                        if (!context.climate->at(field, day, normal, error) || !context.climate->deviation(field, day, deviation, error)) {
                            throw std::runtime_error{error};
                        }
                        if (what == 3 || what == 4) {
                            normal = GfsGrid::scaled(normal, 1.0, -273.15);   // the reanalysis' kelvins to the degrees C of the forecast fields
                        }
                        const auto& first = g.at("v" + memberNames.front());
                        auto zero = first;
                        std::fill(zero.values.begin(), zero.values.end(), 0.0f);
                        const auto spreadHere = GfsGrid::scaled(GfsGrid::anomaly(zero, deviation), -1.0);   // the standard deviation on the members' grid
                        const float level = what == 2 ? 70000.0f : what == 3 ? 70000.0f : what == 4 ? 85000.0f : 0.0f;
                        for (const auto& name : memberNames) {
                            auto z = GfsGrid::anomaly(g.at("v" + name), normal);
                            for (size_t i = 0; i < z.values.size(); i++) {
                                const float sd = spreadHere.values[i];
                                const bool under = level > 0.0f && g.at("ps").values[i] < level;
                                z.values[i] = (sd > 1e-6f && !under) ? z.values[i] / sd : std::nanf("");
                            }
                            g["v" + name] = std::move(z);
                        }
                        g.erase("ps");
                        share(g, [over, sigmas] (const Grids& grids, const std::string& name, size_t i) {
                            const float a = grids.at("v" + name).values[i];
                            return std::isnan(a) ? -1 : (over ? a > sigmas : a < -sigmas) ? 1 : 0;
                        }, "share", {"v"});
                    };
                    x.fill = [] (const Grids& g) { return pick(g, "share"); };
                    x.ramp = probability();
                    x.palette = "prob";
                    x.fillTitle = words + " (% of the 31 members)";
                    x.legendStep = 10;
                    return x;
                };
                // WPC's "heavy rain potential": the share of the members that have, together at a point, precipitable water over so many standard deviations above normal, CAPE over a limit and a
                // 6 hour amount over a limit ("PROB PW > 2 SIGMA + MUCAPE > 500 + QPF > 0.10 IN"). CAPE is that of the most unstable parcel of the lowest 255 mb (the second 0.5 degree file).
                made["heavy_rain"] = [memberNeeds, share, sixHourAmount] (const std::vector<double>& v) {   // deviations, CAPE, inches
                    const double sigmas = v.at(0), cape = v.at(1), inches = v.at(2);
                    Product x;
                    x.source = "GEFS";
                    x.id = generatedId("heavy_rain", {sigmas, cape, inches});
                    const std::string words = "Chance of precipitable water over " + trimmed(sigmas) + " standard deviation" + (sigmas == 1.0 ? "" : "s") + " above normal, MUCAPE over " + trimmed(cape) +
                        " and over " + trimmed(inches) + " in of rain in 6 hours";
                    x.label = "Heavy rain potential: PW > " + trimmed(sigmas) + " sigma, MUCAPE > " + trimmed(cape) + ", rain > " + trimmed(inches) + " in";
                    x.needs = [memberNeeds, sixHourAmount] (int hour) {
                        std::vector<GfsData::Need> out;
                        if (hour < 6 || hour % 6 != 0) {
                            return out;
                        }
                        for (auto& need : memberNeeds("w", "PWAT", "entire atmosphere (considered as a single layer)", hour, std::to_string(hour) + " hour fcst")) {
                            out.push_back(std::move(need));
                        }
                        for (auto& need : memberNeeds("e", "CAPE", "255-0 mb above ground", hour, std::to_string(hour) + " hour fcst")) {
                            out.push_back(std::move(need));
                        }
                        for (auto& need : memberNeeds("a", "APCP", "surface", hour, sixHourAmount(hour))) {
                            out.push_back(std::move(need));
                        }
                        return out;
                    };
                    x.derive = [share, sigmas, cape, inches] (Grids& g, const Context& context) {
                        if (!context.climate) {
                            throw std::runtime_error{"no climatology"};
                        }
                        auto t = QDateTime::fromString(QString::fromStdString(context.run.id()), "yyyyMMddHH");
                        t.setTimeSpec(Qt::UTC);
                        t = t.addSecs(context.hour * 3600LL);
                        const int day = GfsClimate::dayIndex(t.date().year(), t.date().month(), t.date().day());
                        GfsGrid::Grid normal, deviation;
                        std::string error;
                        if (!context.climate->at(GfsClimate::precipitableWater(), day, normal, error) || !context.climate->deviation(GfsClimate::precipitableWater(), day, deviation, error)) {
                            throw std::runtime_error{error};
                        }
                        auto zero = g.at("w" + memberNames.front());
                        std::fill(zero.values.begin(), zero.values.end(), 0.0f);
                        const auto spreadHere = GfsGrid::scaled(GfsGrid::anomaly(zero, deviation), -1.0);
                        for (const auto& name : memberNames) {   // the member's precipitable water in standard deviations
                            auto z = GfsGrid::anomaly(g.at("w" + name), normal);
                            for (size_t i = 0; i < z.values.size(); i++) {
                                z.values[i] = spreadHere.values[i] > 1e-6f ? z.values[i] / spreadHere.values[i] : std::nanf("");
                            }
                            g["w" + name] = std::move(z);
                        }
                        const float mm = static_cast<float>(inches * 25.4);
                        share(g, [sigmas, cape, mm] (const Grids& grids, const std::string& name, size_t i) {
                            const float w = grids.at("w" + name).values[i], e = grids.at("e" + name).values[i], a = grids.at("a" + name).values[i];
                            return std::isnan(w) || std::isnan(e) || std::isnan(a) ? -1 : (w > sigmas && e > cape && a > mm) ? 1 : 0;
                        }, "share", {"w", "e", "a"});
                    };
                    x.fill = [] (const Grids& g) { return pick(g, "share"); };
                    x.ramp = probability();
                    x.palette = "prob";
                    x.fillTitle = words + " (% of the 31 members)";
                    x.legendStep = 10;
                    return x;
                };
                // ---- ingredients together: the share of the 31 members that meet every condition chosen at a point (up to three: CAPE over a limit, shear over a limit, rain over a limit ...)
                made["ingredients"] = [memberNeeds, sixHourAmount] (const std::vector<double>& v) {
                    const auto conditions = ingredientConditions(true, "GEFS");
                    std::vector<IngredientCondition> chosen;
                    for (const double index : v) {
                        if (index >= 0 && static_cast<size_t>(index) < conditions.size()) {
                            chosen.push_back(conditions[static_cast<size_t>(index)]);
                        }
                    }
                    if (chosen.empty()) {
                        throw std::runtime_error{"no condition chosen"};
                    }
                    Product x;
                    x.source = "GEFS";
                    x.id = generatedId("ingredients", v);
                    std::string words;
                    for (const auto& c : chosen) {
                        words += (words.empty() ? "" : " and ") + conditionText(c);
                    }
                    x.label = "Chance of " + ingredientNames(chosen);   // (the limits are in the line under the title)
                    const bool sixHour = std::any_of(chosen.begin(), chosen.end(), [] (const IngredientCondition& c) { return ingredients()[c.ingredient].reqs.front().kind == 1; });
                    x.needs = [memberNeeds, sixHourAmount, chosen, sixHour] (int hour) {
                        std::vector<GfsData::Need> out;
                        if (hour < 3 || (sixHour && (hour < 6 || hour % 6 != 0))) {
                            return out;
                        }
                        for (size_t slot = 0; slot < chosen.size(); slot++) {
                            for (const auto& req : ingredients()[chosen[slot].ingredient].reqs) {
                                const auto prefix = "s" + std::to_string(slot) + "_" + req.key;
                                for (auto& need : memberNeeds(prefix.c_str(), req.variable, req.level, hour, req.kind == 1 ? sixHourAmount(hour) : std::to_string(hour) + " hour fcst")) {
                                    out.push_back(std::move(need));
                                }
                            }
                        }
                        return out;
                    };
                    x.derive = [chosen] (Grids& g, const Context&) {
                        auto result = g.at("s0_" + std::string{ingredients()[chosen[0].ingredient].reqs.front().key} + memberNames.front());
                        std::vector<float> passed(result.values.size(), 0.0f), counted(result.values.size(), 0.0f);
                        for (const auto& name : memberNames) {
                            std::vector<char> all(result.values.size(), 1), known(result.values.size(), 1);
                            for (size_t slot = 0; slot < chosen.size(); slot++) {
                                const auto prefix = "s" + std::to_string(slot) + "_";
                                const auto value = ingredients()[chosen[slot].ingredient].value([&g, &prefix, &name] (const char * key) -> const GfsGrid::Grid& { return g.at(prefix + key + name); });
                                for (size_t i = 0; i < all.size(); i++) {
                                    if (std::isnan(value.values[i])) {
                                        known[i] = 0;
                                    } else if (!(value.values[i] > chosen[slot].limit)) {
                                        all[i] = 0;
                                    }
                                }
                            }
                            for (size_t i = 0; i < all.size(); i++) {
                                if (known[i]) {
                                    counted[i] += 1.0f;
                                    passed[i] += all[i] ? 1.0f : 0.0f;
                                }
                            }
                        }
                        for (size_t i = 0; i < result.values.size(); i++) {
                            result.values[i] = counted[i] > 0.0f ? 100.0f * passed[i] / counted[i] : std::nanf("");
                        }
                        for (auto it = g.begin(); it != g.end();) {   // the members' grids are dropped: 31 of them are a lot to carry
                            it = it->first.compare(0, 1, "s") == 0 && it->first.find('_') != std::string::npos ? g.erase(it) : std::next(it);
                        }
                        g["share"] = std::move(result);
                    };
                    x.fill = [] (const Grids& g) { return pick(g, "share"); };
                    x.ramp = probability();
                    x.palette = "prob";
                    x.fillTitle = "Chance of " + words + ", together (% of the 31 members)";
                    x.legendStep = 10;
                    return x;
                };
                // ---- ingredients met on one run: where each condition holds, and how many of them hold at a point; each condition's own line (its limit) is drawn over the picture, so the one that
                // limits the pattern can be seen
                for (const auto& sourceName : ingredientSources()) {
                    made["ingr_" + sourceName] = [sourceName] (const std::vector<double>& v) {
                        const auto conditions = ingredientConditions(false, sourceName);
                        std::vector<IngredientCondition> chosen;
                        for (const double index : v) {
                            if (index >= 0 && static_cast<size_t>(index) < conditions.size()) {
                                chosen.push_back(conditions[static_cast<size_t>(index)]);
                            }
                        }
                        if (chosen.empty()) {
                            throw std::runtime_error{"no condition chosen"};
                        }
                        Product x;
                        x.source = sourceName;
                        x.id = generatedId("ingr_" + sourceName, v);
                        std::string words;
                        for (const auto& c : chosen) {
                            words += (words.empty() ? "" : ", ") + conditionText(c);
                        }
                        x.label = "Ingredients met: " + ingredientNames(chosen);   // (the limits are in the line under the title)
                        for (size_t slot = 0; slot < chosen.size(); slot++) {
                            for (const auto& req : ingredients()[chosen[slot].ingredient].reqs) {
                                x.wants.push_back(GfsData::Want{"s" + std::to_string(slot) + "_" + req.key, req.variable, req.level, "", ""});
                            }
                        }
                        x.derive = [chosen] (Grids& g, const Context&) {
                            std::vector<GfsGrid::Grid> values;
                            for (size_t slot = 0; slot < chosen.size(); slot++) {
                                const auto prefix = "s" + std::to_string(slot) + "_";
                                values.push_back(ingredients()[chosen[slot].ingredient].value([&g, &prefix] (const char * key) -> const GfsGrid::Grid& { return g.at(prefix + key); }));
                            }
                            auto count = values.front();
                            for (size_t i = 0; i < count.values.size(); i++) {
                                int met = 0;
                                bool known = false;
                                for (size_t slot = 0; slot < chosen.size(); slot++) {
                                    const float value = values[slot].values[i];
                                    if (!std::isnan(value)) {
                                        known = true;
                                        met += value > chosen[slot].limit ? 1 : 0;
                                    }
                                }
                                count.values[i] = known ? static_cast<float>(met) : std::nanf("");
                            }
                            for (size_t slot = 0; slot < values.size(); slot++) {
                                g["v" + std::to_string(slot)] = std::move(values[slot]);
                            }
                            g["count"] = std::move(count);
                        };
                        x.fill = [] (const Grids& g) { return pick(g, "count"); };
                        // one condition met: grey; all of them: red (and amber between, when there are three)
                        Ramp ramp;
                        ramp.banded = true;
                        if (chosen.size() == 1) {
                            ramp.stops = {{1.0, QColor{"#d02a1e"}}};
                        } else if (chosen.size() == 2) {
                            ramp.stops = {{1.0, QColor{"#c9ced6"}}, {2.0, QColor{"#d02a1e"}}};
                        } else {
                            ramp.stops = {{1.0, QColor{"#c9ced6"}}, {2.0, QColor{"#f2b134"}}, {3.0, QColor{"#d02a1e"}}};
                        }
                        x.ramp = ramp;
                        x.fillTitle = "Conditions met at the point (all of them: red): " + words;
                        x.legendStep = 0;
                        static const QColor lineColors[] = {QColor{20, 70, 190}, QColor{20, 130, 60}, QColor{130, 40, 160}};
                        for (size_t slot = 0; slot < chosen.size(); slot++) {
                            ContourSet c;
                            c.key = "v" + std::to_string(slot);
                            c.base = chosen[slot].limit;
                            c.interval = 1.0e9;   // one line, at the limit
                            c.minimum = chosen[slot].limit - 1.0e-6;
                            c.title = conditionText(chosen[slot]);
                            c.color = lineColors[slot % 3];
                            c.uniform = true;
                            c.width = 1.6;
                            c.dashed = slot > 0;
                            c.labelled = false;
                            x.contours.push_back(c);
                        }
                        return x;
                    };
                }
                // ---- the maps NAEFS has, from GEFS's 31 members alone (NAEFS adds the Canadian members and corrects the bias; neither is open): the 10th, 50th and 90th percentile of the members'
                // 10 m wind and 2 m temperature, and the extreme forecast index
                {
                    const auto windNeeds = [memberNeeds] (int hour) {
                        if (hour < 3) {
                            return std::vector<GfsData::Need>{};
                        }
                        auto out = memberNeeds("u", "UGRD", "10 m above ground", hour, std::to_string(hour) + " hour fcst");
                        for (auto& need : memberNeeds("w", "VGRD", "10 m above ground", hour, std::to_string(hour) + " hour fcst")) {
                            out.push_back(std::move(need));
                        }
                        return out;
                    };
                    const auto speedOfMembers = [] (Grids& g) {   // "v<member>": the 10 m wind speed (m/s)
                        for (const auto& name : memberNames) {
                            auto u = std::move(g.at("u" + name));
                            const auto& w = g.at("w" + name);
                            for (size_t i = 0; i < u.values.size(); i++) {
                                u.values[i] = std::hypot(u.values[i], w.values[i]);
                            }
                            g["v" + name] = std::move(u);
                            g.erase("u" + name);
                            g.erase("w" + name);
                        }
                    };
                    const auto temperatureNeeds = [memberNeeds] (int hour) { return hour < 3 ? std::vector<GfsData::Need>{} : memberNeeds("v", "TMP", "2 m above ground", hour, std::to_string(hour) + " hour fcst"); };
                    const auto pressureNeeds = [memberNeeds] (int hour) { return hour < 3 ? std::vector<GfsData::Need>{} : memberNeeds("v", "PRMSL", "mean sea level", hour, std::to_string(hour) + " hour fcst"); };
                    for (const int percent : {10, 50, 90}) {
                        for (const bool wind : {true, false}) {
                            Product x;
                            x.source = "GEFS";
                            x.id = std::string{"pct"} + std::to_string(percent) + (wind ? "_10m_wnd" : "_2m_temp");
                            x.label = std::to_string(percent) + (percent == 10 ? "th" : percent == 50 ? "th" : "th") + " percentile " + (wind ? "10m Winds" : "2m Temperature");
                            x.needs = wind ? std::function<std::vector<GfsData::Need>(int)>{windNeeds} : std::function<std::vector<GfsData::Need>(int)>{temperatureNeeds};
                            x.derive = [wind, percent, speedOfMembers] (Grids& g, const Context&) {
                                if (wind) {
                                    speedOfMembers(g);
                                }
                                auto out = g.at("v" + memberNames.front());
                                std::vector<float> values;
                                for (size_t i = 0; i < out.values.size(); i++) {
                                    values.clear();
                                    for (const auto& name : memberNames) {
                                        const float v = g.at("v" + name).values[i];
                                        if (!std::isnan(v)) {
                                            values.push_back(v);
                                        }
                                    }
                                    if (values.empty()) {
                                        out.values[i] = std::nanf("");
                                        continue;
                                    }
                                    std::sort(values.begin(), values.end());
                                    const double rank = percent / 100.0 * static_cast<double>(values.size() - 1);
                                    const auto low = static_cast<size_t>(std::floor(rank)), high = static_cast<size_t>(std::ceil(rank));
                                    out.values[i] = static_cast<float>(values[low] + (values[high] - values[low]) * (rank - static_cast<double>(low)));
                                }
                                g["pct"] = wind ? GfsGrid::scaled(out, 1.943844) : out;   // kt
                                for (const auto& name : memberNames) {
                                    g.erase("v" + name);
                                }
                            };
                            x.fill = [] (const Grids& g) { return pick(g, "pct"); };
                            x.ramp = wind ? lowWind() : temperature();
                            x.quantity = wind ? Quantity::Other : Quantity::Temperature;
                            x.fillTitle = std::to_string(percent) + "th percentile of the 31 members: " + (wind ? "10 m wind speed (kt)" : "2 m temperature");
                            x.legendStep = wind ? 10 : 5;
                            p.push_back(x);
                        }
                    }
                    // the members' 2 m temperature in one picture: the median as the shading, the 10th and 90th percentile as numbers at cities (and evenly elsewhere) - the range the
                    // ensemble gives at each place. One sort per point makes all three.
                    {
                        Product x;
                        x.source = "GEFS";
                        x.id = "pct50_2m_temp_range";
                        x.label = "Median 2m Temperature with 10th / 90th percentiles";
                        x.needs = temperatureNeeds;
                        x.derive = [] (Grids& g, const Context&) {
                            auto p10 = g.at("v" + memberNames.front());
                            auto p50 = p10;
                            auto p90 = p10;
                            std::vector<float> values;
                            const auto at = [&values] (int percent) {
                                const double rank = percent / 100.0 * static_cast<double>(values.size() - 1);
                                const auto low = static_cast<size_t>(std::floor(rank)), high = static_cast<size_t>(std::ceil(rank));
                                return static_cast<float>(values[low] + (values[high] - values[low]) * (rank - static_cast<double>(low)));
                            };
                            for (size_t i = 0; i < p50.values.size(); i++) {
                                values.clear();
                                for (const auto& name : memberNames) {
                                    const float v = g.at("v" + name).values[i];
                                    if (!std::isnan(v)) {
                                        values.push_back(v);
                                    }
                                }
                                if (values.empty()) {
                                    p10.values[i] = p50.values[i] = p90.values[i] = std::nanf("");
                                    continue;
                                }
                                std::sort(values.begin(), values.end());
                                p10.values[i] = at(10);
                                p50.values[i] = at(50);
                                p90.values[i] = at(90);
                            }
                            for (const auto& name : memberNames) {
                                g.erase("v" + name);
                            }
                            g["p10"] = std::move(p10);
                            g["p50"] = std::move(p50);
                            g["p90"] = std::move(p90);
                        };
                        x.fill = [] (const Grids& g) { return pick(g, "p50"); };
                        x.ramp = temperature();
                        x.quantity = Quantity::Temperature;
                        x.fillTitle = "Median of the 31 members: 2 m temperature. Numbers: 10th percentile (blue), 90th percentile (red)";
                        x.legendStep = 5;
                        x.pointValues = {{"p10", QColor{30, 80, 200}, "10th percentile 2 m temperature"},
                                         {"p90", QColor{200, 40, 30}, "90th percentile 2 m temperature"}};
                        p.push_back(x);
                    }
                    // the extreme forecast index: where the members' values stand in the climate of the day (the 1991-2020 daily mean and spread of the reanalysis, taken as a normal distribution): +1 when every
                    // member is above everything the climate has, -1 below. (ECMWF's index, made with a model climate; this one is a stand-in made with the reanalysis')
                    struct Efi {
                        const char * id;
                        const char * label;
                        const char * what;
                        int kind;   // 0 temperature, 1 wind, 2 pressure
                    };
                    for (const auto& e : {Efi{"efi_2m_temp", "Extreme Forecast Index 2m Temperature", "2 m temperature", 0}, Efi{"efi_10m_wnd", "Extreme Forecast Index 10m Winds", "10 m wind speed", 1},
                                          Efi{"efi_mslp", "Extreme Forecast Index mslp", "sea level pressure", 2}}) {
                        Product x;
                        x.source = "GEFS";
                        x.id = e.id;
                        x.label = e.label;
                        const int kind = e.kind;
                        x.needs = kind == 0 ? std::function<std::vector<GfsData::Need>(int)>{temperatureNeeds} : kind == 1 ? std::function<std::vector<GfsData::Need>(int)>{windNeeds} : std::function<std::vector<GfsData::Need>(int)>{pressureNeeds};
                        x.derive = [kind, speedOfMembers] (Grids& g, const Context& context) {
                            if (!context.climate) {
                                throw std::runtime_error{"no climatology"};
                            }
                            if (kind == 1) {
                                speedOfMembers(g);
                            }
                            auto t = QDateTime::fromString(QString::fromStdString(context.run.id()), "yyyyMMddHH");
                            t.setTimeSpec(Qt::UTC);
                            t = t.addSecs(context.hour * 3600LL);
                            const int day = GfsClimate::dayIndex(t.date().year(), t.date().month(), t.date().day());
                            GfsGrid::Grid mean, deviation;
                            std::string error;
                            const auto stat = kind == 0 ? GfsClimate::nearSurfaceTemperature() : kind == 1 ? GfsClimate::nearSurfaceWind() : GfsClimate::seaLevelPressureStat();
                            if (!context.climate->statistics(stat, day, mean, deviation, error)) {
                                throw std::runtime_error{error};
                            }
                            if (kind == 0) {
                                mean = GfsGrid::scaled(mean, 1.0, -273.15);   // kelvins -> the degrees C of the forecast
                            }
                            auto out = g.at("v" + memberNames.front());
                            auto zero = out;
                            std::fill(zero.values.begin(), zero.values.end(), 0.0f);
                            const auto mu = GfsGrid::scaled(GfsGrid::anomaly(zero, mean), -1.0);       // the climate's mean and spread on the members' grid
                            const auto sigma = GfsGrid::scaled(GfsGrid::anomaly(zero, deviation), -1.0);
                            constexpr double pi = 3.14159265358979323846;
                            std::vector<double> ps;
                            for (size_t i = 0; i < out.values.size(); i++) {
                                ps.clear();
                                const double sd = sigma.values[i];
                                for (const auto& name : memberNames) {
                                    const float v = g.at("v" + name).values[i];
                                    if (!std::isnan(v) && sd > 1e-6) {
                                        ps.push_back(0.5 * std::erfc(-(v - mu.values[i]) / sd / std::sqrt(2.0)));   // the member's place in the climate (0 to 1)
                                    }
                                }
                                if (ps.empty()) {
                                    out.values[i] = std::nanf("");
                                    continue;
                                }
                                std::sort(ps.begin(), ps.end());
                                const double n = static_cast<double>(ps.size());
                                // EFI = 2/pi * integral over p of (p - F(p)) / sqrt(p (1 - p)), F the members' share at or under the climate's p: exact for the steps of F
                                const auto up = [] (double p) { return std::asin(std::sqrt(std::clamp(p, 0.0, 1.0))); };
                                const auto lin = [&up] (double p) { return up(p) - std::sqrt(std::clamp(p * (1.0 - p), 0.0, 1.0)); };   // the integral of p / sqrt(p (1 - p))
                                double total = 0.0, from = 0.0;
                                for (size_t j = 0; j <= ps.size(); j++) {
                                    const double to = j < ps.size() ? ps[j] : 1.0;
                                    total += (lin(to) - lin(from)) - (static_cast<double>(j) / n) * 2.0 * (up(to) - up(from));
                                    from = to;
                                }
                                out.values[i] = static_cast<float>(2.0 / pi * total);
                            }
                            g["efi"] = std::move(out);
                            for (const auto& name : memberNames) {
                                g.erase("v" + name);
                            }
                        };
                        x.fill = [] (const Grids& g) { return pick(g, "efi"); };
                        x.ramp = Ramp{{{-1.0, QColor{"#2b4fb0"}}, {-0.8, QColor{"#2b6cb0"}}, {-0.5, QColor{"#7fb6e0"}}, {-0.2, QColor{"#dbeaf5"}}, {0.0, QColor{245, 245, 245, 0}}, {0.2, QColor{"#fbe1c6"}}, {0.5, QColor{"#f0a060"}}, {0.8, QColor{"#d0402a"}}, {1.0, QColor{"#8b1a1a"}}}};
                        x.fillTitle = std::string{"Extreme forecast index of the "} + e.what + " (-1 well below to +1 well above the day's climate)";
                        x.legendStep = 0.2;
                        p.push_back(x);
                    }
                }
                // The chance that a member's value passes a limit: the template says what to fetch (`needs`, the members' grids named "<prefix><member>"), how to make one value of them (`collapse`, into
                // "v<member>": a wind speed from its components, a heat index) and how the limit given in the user's unit is in the file's (`toFile`).
                struct Limit {
                    const char * id;
                    const char * what;     // "a 2 m temperature"
                    const char * unit;     // "F"
                    bool above;
                    std::function<std::vector<GfsData::Need>(int hour)> needs;
                    std::function<void(Grids&)> collapse;
                    std::function<float(double)> toFile;
                };
                const auto instant = [memberNeeds] (const char * prefix, const char * variable, const char * level) {
                    return [=] (int hour) { return hour < 3 ? std::vector<GfsData::Need>{} : memberNeeds(prefix, variable, level, hour, std::to_string(hour) + " hour fcst"); };
                };
                const auto sixHour = [memberNeeds] (const char * prefix, const char * variable, const char * level, const char * kind) {   // the 6 hours ending at the hour: their largest, smallest or average
                    return [=] (int hour) {
                        return hour < 6 || hour % 6 != 0 ? std::vector<GfsData::Need>{} :
                            memberNeeds(prefix, variable, level, hour, std::to_string(hour - 6) + "-" + std::to_string(hour) + " hour " + kind + " fcst");
                    };
                };
                const auto rename = [] (const char * from) {   // the one grid of each member, under the name the comparison reads
                    return [from] (Grids& g) {
                        for (const auto& name : memberNames) {
                            g["v" + name] = std::move(g.at(std::string{from} + name));
                            g.erase(std::string{from} + name);
                        }
                    };
                };
                const auto fahrenheit = [] (double f) { return static_cast<float>((f - 32.0) * 5.0 / 9.0); };   // the grids are in degrees C (GDAL turns the GRIB temperatures from K)
                const auto scaled = [] (double factor) { return [factor] (double v) { return static_cast<float>(v * factor); }; };
                const auto together = [memberNeeds] (std::vector<std::function<std::vector<GfsData::Need>(int)>> parts) {
                    return [parts] (int hour) {
                        std::vector<GfsData::Need> out;
                        for (const auto& part : parts) {
                            for (auto& need : part(hour)) {
                                out.push_back(std::move(need));
                            }
                        }
                        return out;
                    };
                };
                const auto speed = [] (Grids& g) {   // 10 m wind speed (m/s) from the components
                    for (const auto& name : memberNames) {
                        auto u = std::move(g.at("u" + name));
                        const auto& w = g.at("w" + name);
                        for (size_t i = 0; i < u.values.size(); i++) {
                            u.values[i] = std::hypot(u.values[i], w.values[i]);
                        }
                        g["v" + name] = std::move(u);
                        g.erase("u" + name);
                        g.erase("w" + name);
                    }
                };
                const auto heat = [] (Grids& g) {   // the heat index (K) from the 2 m temperature and humidity: the National Weather Service's regression, with its adjustments
                    for (const auto& name : memberNames) {
                        auto t = std::move(g.at("t" + name));
                        const auto& rh = g.at("h" + name);
                        for (size_t i = 0; i < t.values.size(); i++) {
                            const double f = t.values[i] * 9.0 / 5.0 + 32.0, r = rh.values[i];
                            double hi = f;
                            if (f >= 80.0) {
                                hi = -42.379 + 2.04901523 * f + 10.14333127 * r - 0.22475541 * f * r - 0.00683783 * f * f - 0.05481717 * r * r + 0.00122874 * f * f * r + 0.00085282 * f * r * r - 0.00000199 * f * f * r * r;
                                if (r < 13.0 && f <= 112.0) {
                                    hi -= (13.0 - r) / 4.0 * std::sqrt((17.0 - std::abs(f - 95.0)) / 17.0);
                                } else if (r > 85.0 && f <= 87.0) {
                                    hi += (r - 85.0) / 10.0 * (87.0 - f) / 5.0;
                                }
                            }
                            t.values[i] = static_cast<float>((hi - 32.0) * 5.0 / 9.0);
                        }
                        g["v" + name] = std::move(t);
                        g.erase("t" + name);
                        g.erase("h" + name);
                    }
                };
                const std::vector<Limit> limits{
                    {"temp_under", "a 2 m temperature", "F", false, instant("v", "TMP", "2 m above ground"), nullptr, fahrenheit},
                    {"temp_over", "a 2 m temperature", "F", true, instant("v", "TMP", "2 m above ground"), nullptr, fahrenheit},
                    {"tmin_under", "a 6 hour minimum temperature", "F", false, sixHour("v", "TMIN", "2 m above ground", "min"), nullptr, fahrenheit},
                    {"tmax_over", "a 6 hour maximum temperature", "F", true, sixHour("v", "TMAX", "2 m above ground", "max"), nullptr, fahrenheit},
                    {"tmax_under", "a 6 hour maximum temperature", "F", false, sixHour("v", "TMAX", "2 m above ground", "max"), nullptr, fahrenheit},
                    {"dewpoint_over", "a 2 m dewpoint", "F", true, instant("v", "DPT", "2 m above ground"), nullptr, fahrenheit},
                    {"heat_over", "a heat index", "F", true, together({instant("t", "TMP", "2 m above ground"), instant("h", "RH", "2 m above ground")}), heat, fahrenheit},
                    {"rh_under", "a 2 m relative humidity", "%", false, instant("v", "RH", "2 m above ground"), nullptr, scaled(1.0)},
                    {"wind_over", "a 10 m wind", "kt", true, together({instant("u", "UGRD", "10 m above ground"), instant("w", "VGRD", "10 m above ground")}), speed, scaled(0.514444)},
                    {"gust_over", "wind gusts", "kt", true, instant("v", "GUST", "surface"), nullptr, scaled(0.514444)},
                    {"mslp_under", "a sea level pressure", "mb", false, instant("v", "PRMSL", "mean sea level"), nullptr, scaled(100.0)},
                    {"mslp_over", "a sea level pressure", "mb", true, instant("v", "PRMSL", "mean sea level"), nullptr, scaled(100.0)},
                    {"pw_over", "precipitable water", "in", true, instant("v", "PWAT", "entire atmosphere (considered as a single layer)"), nullptr, scaled(25.4)},
                    {"cloud_under", "a 6 hour mean cloud cover", "%", false, sixHour("v", "TCDC", "entire atmosphere", "ave"), nullptr, scaled(1.0)},
                    {"cloud_over", "a 6 hour mean cloud cover", "%", true, sixHour("v", "TCDC", "entire atmosphere", "ave"), nullptr, scaled(1.0)},
                    {"cape_over", "surface-based CAPE", "J/kg", true, instant("v", "CAPE", "surface"), nullptr, scaled(1.0)},
                    {"mucape_over", "most unstable CAPE", "J/kg", true, instant("v", "CAPE", "255-0 mb above ground"), nullptr, scaled(1.0)},
                    {"z500_over", "a 500 mb height", "m", true, instant("v", "HGT", "500 mb"), nullptr, scaled(1.0)},
                };
                for (const auto& limit : limits) {
                    made[limit.id] = [limit, share] (const std::vector<double>& v) {
                        const double value = v.at(0);
                        Product x;
                        x.source = "GEFS";
                        x.id = generatedId(limit.id, {value});
                        const std::string words = std::string{"Chance of "} + limit.what + (limit.above ? " over " : " under ") + trimmed(value) + " " + limit.unit;
                        x.label = words;
                        x.needs = limit.needs;
                        x.derive = [limit, share, value] (Grids& g, const Context&) {
                            if (limit.collapse) {
                                limit.collapse(g);
                            }
                            const float edge = limit.toFile(value);
                            const bool above = limit.above;
                            share(g, [edge, above] (const Grids& grids, const std::string& name, size_t i) {
                                const float a = grids.at("v" + name).values[i];
                                return std::isnan(a) ? -1 : (above ? a > edge : a < edge) ? 1 : 0;
                            }, "share", {"v"});
                        };
                        x.fill = [] (const Grids& g) { return pick(g, "share"); };
                        x.ramp = probability();
                        x.palette = "prob";
                        x.fillTitle = words + " (% of the 31 members)";
                        x.legendStep = 10;
                        return x;
                    };
                }
            }
            for (const auto& [id, inches, text] : {std::tuple{"prob_precip_0.25in", 0.25, "0.25"}, {"prob_precip_0.5in", 0.5, "0.50"}, {"prob_precip_1in", 1.0, "1.00"}}) {
                const double mm = inches * 25.4;
                p.push_back(shareChart(id, std::string{"Probability of 6-hour Precipitation over "} + text + " in", std::string{"Chance of more than "} + text + " in of precipitation in 6 hours (% of the 31 members)",
                                       [memberNeeds, sixHourAmount] (int hour) { return hour < 6 ? std::vector<GfsData::Need>{} : memberNeeds("a", "APCP", "surface", hour, sixHourAmount(hour)); },
                                       [share, mm] (Grids& g) {
                                           share(g, [mm] (const Grids& grids, const std::string& name, size_t i) {
                                               const float a = grids.at("a" + name).values[i];
                                               return std::isnan(a) ? -1 : a > mm ? 1 : 0;
                                           }, "share", {"a"});
                                       }));
            }
            p.push_back(shareChart("prob_ice_0.25in", "Probability of Ice Pellets over 0.25 in", "Chance of 0.25 in or more of precipitation falling as ice pellets in 6 hours (% of the 31 members)",
                                   [memberNeeds, sixHourAmount, sixHourAverage] (int hour) {
                                       if (hour < 6) {
                                           return std::vector<GfsData::Need>{};
                                       }
                                       auto out = memberNeeds("a", "APCP", "surface", hour, sixHourAmount(hour));
                                       for (auto& need : memberNeeds("i", "CICEP", "surface", hour, sixHourAverage(hour))) {   // the share of the 6 hours with ice pellets
                                           out.push_back(std::move(need));
                                       }
                                       return out;
                                   },
                                   [share] (Grids& g) {   // a member counts when it makes the amount and most of that time is ice pellets (an approximation: the amount of the ice pellets alone is not in the files)
                                       share(g, [] (const Grids& grids, const std::string& name, size_t i) {
                                           const float a = grids.at("a" + name).values[i], ice = grids.at("i" + name).values[i];
                                           return std::isnan(a) || std::isnan(ice) ? -1 : (a >= 6.35f && ice >= 0.5f) ? 1 : 0;
                                       }, "share", {"a", "i"});
                                   }));
            for (const int joules : {250, 500, 2000, 4000}) {
                p.push_back(shareChart(("prob_cape_" + std::to_string(joules)).c_str(), "Probability of CAPE over " + std::to_string(joules), "Chance of surface-based CAPE over " + std::to_string(joules) + " J/kg (% of the 31 members)",
                                       [memberNeeds] (int hour) { return memberNeeds("c", "CAPE", "surface", hour, std::to_string(hour) + " hour fcst"); },
                                       [share, joules] (Grids& g) {
                                           share(g, [joules] (const Grids& grids, const std::string& name, size_t i) {
                                               const float c = grids.at("c" + name).values[i];
                                               return std::isnan(c) ? -1 : c > static_cast<float>(joules) ? 1 : 0;
                                           }, "share", {"c"});
                                       }));
            }
            {   // the precipitation type that most of the members' 6 hours had (the shares of the time with rain, snow, freezing rain and ice pellets are in the mean files): 1 rain, 2 snow, 3 freezing rain, 4 ice pellets
                Product x;
                x.source = "GEFS";
                x.id = "dom_precip_type";
                x.label = "Dominant Precipitation Type";
                x.needs = [sixHourAmount, sixHourAverage] (int hour) {
                    if (hour < 6) {
                        return std::vector<GfsData::Need>{};
                    }
                    return std::vector<GfsData::Need>{{hour, {"a", "APCP", "surface", sixHourAmount(hour), "", ""}}, {hour, {"r", "CRAIN", "surface", sixHourAverage(hour), "", ""}}, {hour, {"s", "CSNOW", "surface", sixHourAverage(hour), "", ""}},
                                                      {hour, {"f", "CFRZR", "surface", sixHourAverage(hour), "", ""}}, {hour, {"i", "CICEP", "surface", sixHourAverage(hour), "", ""}}};
                };
                x.derive = [] (Grids& g, const Context&) {
                    auto type = g["a"];
                    for (size_t i = 0; i < type.values.size(); i++) {
                        if (std::isnan(g["a"].values[i]) || g["a"].values[i] < 0.254f) {   // under 0.01 inch: no precipitation to name
                            type.values[i] = std::nanf("");
                            continue;
                        }
                        const float shares[4] = {g["r"].values[i], g["s"].values[i], g["f"].values[i], g["i"].values[i]};
                        int best = 0;
                        for (int k = 1; k < 4; k++) {
                            best = shares[k] > shares[best] ? k : best;
                        }
                        type.values[i] = static_cast<float>(best + 1);
                    }
                    g["type"] = std::move(type);
                };
                x.fill = [] (const Grids& g) { return pick(g, "type"); };
                x.ramp = Ramp{{{1, QColor{0, 200, 0}}, {2, QColor{30, 100, 255}}, {3, QColor{230, 20, 20}}, {4, QColor{150, 40, 200}}}, true};
                x.fillTitle = "Dominant precipitation type: 1 rain (green), 2 snow (blue), 3 freezing rain (red), 4 ice pellets (purple)";
                x.legendStep = 0;
                p.push_back(x);
            }
            {   // the change of the snow depth over the 24 hours before: the mean of the members, and how much the members differ in it
                const auto depthNeeds = [memberNeeds] (int hour, bool members) {
                    const int start = std::max(hour - 24, 0);
                    std::vector<GfsData::Need> out;
                    if (members) {
                        out = memberNeeds("n", "SNOD", "surface", hour, std::to_string(hour) + " hour fcst");
                        for (auto& need : memberNeeds("o", "SNOD", "surface", start, std::to_string(start) + " hour fcst")) {
                            out.push_back(std::move(need));
                        }
                    } else {
                        out = {{hour, {"n", "SNOD", "surface", std::to_string(hour) + " hour fcst", "", ""}}, {start, {"o", "SNOD", "surface", std::to_string(start) + " hour fcst", "", ""}}};
                    }
                    return out;
                };
                Product mean;
                mean.source = "GEFS";
                mean.id = "snodpth_chng_mean";
                mean.label = "Mean Snow Depth Change from the previous 24 hours";
                mean.needs = [depthNeeds] (int hour) { return depthNeeds(hour, false); };
                mean.fill = [] (const Grids& g) { return GfsGrid::scaled(GfsGrid::difference(pick(g, "n"), pick(g, "o")), 100.0); };   // meters -> centimeters
                mean.ramp = snowChange();
                mean.palette = "snowdepth";
                mean.quantity = Quantity::Centimeters;
                mean.fillTitle = "Mean change of the snow depth in the previous 24 hours";
                mean.legendStep = 0;
                p.push_back(mean);
                Product spreadChart = mean;
                spreadChart.id = "snodpth_chng_sprd";
                spreadChart.label = "Spread of the Snow Depth Change from the previous 24 hours";
                spreadChart.needs = [depthNeeds] (int hour) { return depthNeeds(hour, true); };
                spreadChart.derive = [] (Grids& g, const Context&) {   // the spread (standard deviation) among the members of each one's change
                    auto result = g.at("n" + memberNames.front());
                    for (size_t i = 0; i < result.values.size(); i++) {
                        double sum = 0.0, square = 0.0;
                        int n = 0;
                        for (const auto& name : memberNames) {
                            const float change = g.at("n" + name).values[i] - g.at("o" + name).values[i];
                            if (!std::isnan(change)) {
                                sum += change;
                                square += static_cast<double>(change) * change;
                                n++;
                            }
                        }
                        result.values[i] = n > 1 ? static_cast<float>(100.0 * std::sqrt(std::max(0.0, square / n - (sum / n) * (sum / n)))) : std::nanf("");   // centimeters
                    }
                    g["spread"] = std::move(result);
                    for (const auto& name : memberNames) {
                        g.erase("n" + name);
                        g.erase("o" + name);
                    }
                };
                spreadChart.fill = [] (const Grids& g) { return pick(g, "spread"); };
                spreadChart.ramp = spreadRamp(10.0);
                spreadChart.fillTitle = "Spread among the members of the snow depth change";
                p.push_back(spreadChart);
            }

            // ---- the spaghetti maps: one contour (or two) of a field from each of the 31 members, each member a color of its own, and the ensemble mean as the heavy black line
            const auto spaghetti = [&] (const std::string& id, const std::string& label, const char * variable, const char * level, double scale, std::vector<double> levels, const std::string& what) {
                Product x;
                x.source = "GEFS";
                x.id = id;
                x.label = label;
                x.linesOnly = true;
                x.needs = [memberNeeds, variable, level] (int hour) {
                    if (hour < 3) {
                        return std::vector<GfsData::Need>{};   // the members have no forecast record at hour 0
                    }
                    const auto when = std::to_string(hour) + " hour fcst";
                    auto out = memberNeeds("z", variable, level, hour, when);
                    out.push_back({hour, GfsData::Want{"zm", variable, level, when, "", ""}});
                    return out;
                };
                x.fillTitle = what + ": one thin line for each of the 31 members, the heavy black line the ensemble mean";
                for (const double value : levels) {
                    for (size_t m = 0; m < memberNames.size(); m++) {
                        ContourSet c;
                        c.key = "z" + memberNames[m];
                        c.scale = scale;
                        c.base = value;
                        c.interval = 100000.0;   // one level: the value
                        c.color = QColor::fromHsv(static_cast<int>(m * 360 / memberNames.size()), 230, 190);
                        c.width = 1.0;
                        c.labelled = false;
                        c.uniform = true;
                        x.contours.push_back(c);
                    }
                    ContourSet mean;
                    mean.key = "zm";
                    mean.scale = scale;
                    mean.base = value;
                    mean.interval = 100000.0;
                    mean.color = QColor{10, 10, 10};
                    mean.width = 2.6;
                    mean.uniform = true;
                    x.contours.push_back(mean);
                }
                return x;
            };
            for (const int value : {1176, 1188, 1200, 1212, 1224, 1230}) {
                p.push_back(spaghetti("200_" + std::to_string(value) + "_ht", "200mb " + std::to_string(value) + " Height Contours", "HGT", "200 mb", 0.1, {static_cast<double>(value)}, "200 mb height of " + std::to_string(value) + " dam"));
            }
            for (const auto& [low, high] : {std::pair{510, 552}, {516, 558}, {522, 564}, {528, 570}, {534, 576}, {540, 582}}) {
                p.push_back(spaghetti("500_" + std::to_string(low) + "_" + std::to_string(high) + "_ht", "500mb " + std::to_string(low) + "/" + std::to_string(high) + " Height Contours", "HGT", "500 mb", 0.1,
                                      {static_cast<double>(low), static_cast<double>(high)}, "500 mb heights of " + std::to_string(low) + " and " + std::to_string(high) + " dam"));
            }
            for (const auto& [low, high] : {std::pair{984, 1024}, {996, 1036}, {1000, 1040}, {1004, 1044}, {1008, 1048}, {1012, 1052}}) {
                p.push_back(spaghetti("mslp_" + std::to_string(low) + "_" + std::to_string(high) + "_iso", "MSLP " + std::to_string(low) + "/" + std::to_string(high) + " Isobar Contours", "PRMSL", "mean sea level", 0.01,
                                      {static_cast<double>(low), static_cast<double>(high)}, "sea level pressure of " + std::to_string(low) + " and " + std::to_string(high) + " mb"));
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
                x.palette = "spread_wind";
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
                x.palette = "spread_mslp";
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
                x.palette = "spread_wind";
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
            x.palette = "precip";
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
        const auto rrfsRecipes = [&] (const std::string& model) {   // (the HRRR has the same fields, a few of them under other names)
            const bool hrrr = model.compare(0, 4, "HRRR") == 0;
            const auto make = [&model] (const char * id, const char * label) {
                Product x;
                x.source = model;
                x.id = id;
                x.label = label;
                return x;
            };
            const auto scaledOf = [] (const char * key, double factor) { return [key, factor] (const Grids& g) { return GfsGrid::scaled(pick(g, key), factor); }; };
            const auto sea = [hrrr] { return want("p", hrrr ? "MSLMA" : "MSLET", "mean sea level"); };
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
            {   // the files' "best 4-layer lifted index" of the lowest 180 mb (their surface-based record, LFTX 500-1000 mb, holds temperatures of 260-290 K, not an index: it is not used)
                auto x = make("lifted_index_ml", "Best Lifted Index (lowest 180 mb)");
                x.wants = {want("l", "4LFTX", "180-0 mb above ground")};
                x.fill = [] (const Grids& g) { return pick(g, "l"); };
                x.ramp = Ramp{{{-14, QColor{"#6a2c9a"}}, {-10, QColor{"#d02fb0"}}, {-8, QColor{"#e0301e"}}, {-6, QColor{"#f07a1a"}}, {-4, QColor{"#f5c842"}}, {-2, QColor{"#f6f0a0"}}, {0, QColor{"#e8f0e8"}}, {2, QColor{232, 240, 232, 0}}}};
                x.fillTitle = "Best (4-layer) lifted index of the lowest 180 mb (negative: unstable)";
                x.legendStep = 2;
                p.push_back(x);
            }
            for (const auto& [id, label, layer] : {std::tuple{"helicity_1km", "0-1 km Storm-Relative Helicity", "1000-0 m above ground"}, {"helicity_3km", "0-3 km Storm-Relative Helicity", "3000-0 m above ground"}}) {
                auto x = make(id, label);
                x.wants = {want("h", "HLCY", layer)};
                x.fill = [] (const Grids& g) { return pick(g, "h"); };
                x.ramp = helicityRamp();
                x.palette = "helicity";
                x.fillTitle = "Storm-relative helicity (m2/s2)";
                x.legendStep = 100;
                p.push_back(x);
            }
            {
                auto x = make("max_updraft_hlcy", "Maximum 2-5 km Updraft Helicity in the last hour");
                x.needs = [hourly] (int hour) { return std::vector<GfsData::Need>{{hour, {"h", "MXUPHL", "5000-2000 m above ground", hourly(hour), ""}}}; };
                x.fill = [] (const Grids& g) { return pick(g, "h"); };
                x.ramp = updraftHelicity();
                x.palette = "uh";
                x.fillTitle = "Updraft helicity (m2/s2)";
                x.legendStep = 0;
                p.push_back(x);
            }
            // the strongest relative vorticity of the last hour in the lowest 1 km and 2 km: the low-level rotation (the helicity above is the 2-5 km mid-level one), in 1e-3 per second
            for (const auto& [id, label, layer] : {std::tuple{"max_relv_0_1km", "Maximum 0-1 km Relative Vorticity in the last hour", "1000-0 m above ground"},
                                                    {"max_relv_0_2km", "Maximum 0-2 km Relative Vorticity in the last hour", "2000-0 m above ground"}}) {
                auto x = make(id, label);
                x.needs = [hourly, layer = std::string{layer}] (int hour) { return std::vector<GfsData::Need>{{hour, {"v", "RELV", layer, hourly(hour), ""}}}; };
                x.derive = [] (Grids& g, const Context&) {
                    for (auto& value : g.at("v").values) {
                        value *= 1000.0f;
                    }
                };
                x.fill = [] (const Grids& g) { return pick(g, "v"); };
                x.ramp = lowLevelVorticity();
                x.fillTitle = "Relative vorticity (1e-3 /s)";
                x.legendStep = 0;
                p.push_back(x);
            }
            for (auto& chart : p) {   // the composite reflectivity (the GFS chart cloned above) with the rotating storms outlined: the last hour's strongest 2-5 km updraft helicity, 40 and over
                if (chart.source != model || chart.id != "sim_radar_comp") {
                    continue;
                }
                chart.wants.clear();
                chart.needs = [hourly, hrrr] (int hour) {   // the RRFS files call the whole-column level by its long name
                    std::vector<GfsData::Need> out;
                    out.push_back({hour, {"r", "REFC", hrrr ? "entire atmosphere" : "entire atmosphere (considered as a single layer)", "", ""}});
                    out.push_back({hour, {"uh", "MXUPHL", "5000-2000 m above ground", hourly(hour), ""}});
                    return out;
                };
                chart.derive = [] (Grids& g, const Context&) {
                    if (g.find("uh") == g.end()) {   // the start of the run has no hour's maximum yet
                        g["uh"] = g.at("r");
                        std::fill(g["uh"].values.begin(), g["uh"].values.end(), 0.0f);
                    }
                };
                ContourSet uh;
                uh.key = "uh";
                uh.interval = 40;
                uh.base = 40;
                uh.minimum = 39;
                uh.title = "2-5 km updraft helicity of the last hour, 40 and over (m2/s2)";
                uh.color = QColor{10, 10, 10};
                uh.width = 2.5;
                uh.uniform = true;
                chart.contours = {uh};
                chart.label = "Composite Reflectivity with Updraft Helicity of 40 or more";
                break;
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
                x.palette = "radar_rain";
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
                x.wants = {hrrr ? want("t", "RETOP", "cloud top") : want("t", "RETOP", "entire atmosphere (considered as a single layer)")};
                x.fill = scaledOf("t", 3.28084 / 1000.0);
                x.ramp = echoTopRamp();
                x.palette = "echo_top";
                x.fillTitle = "Echo top (thousands of feet)";
                x.legendStep = 10;
                p.push_back(x);
            }
            {
                auto x = make("ceiling", "Cloud Ceiling");
                x.wants = {hrrr ? want("c", "HGT", "cloud ceiling") : want("c", "CEIL", "cloud ceiling")};
                x.fill = scaledOf("c", 3.28084);
                x.ramp = ceilingRamp();
                x.palette = "ceiling";
                x.fillTitle = "Ceiling (feet)";
                x.legendStep = 0;
                p.push_back(x);
            }
            {
                auto x = make("vis", "Surface Visibility");
                x.wants = {want("v", "VIS", "surface")};
                x.fill = scaledOf("v", 1.0 / 1609.344);
                x.ramp = visibilityRamp();
                x.palette = "vis";
                x.fillTitle = "Visibility (miles)";
                x.legendStep = 0;
                p.push_back(x);
            }
            {
                auto x = make("lightning", "Lightning (flashes in the last hour)");
                x.needs = [hourly, hrrr] (int hour) { return std::vector<GfsData::Need>{{hour, {"l", "LTNG", "entire atmosphere", hrrr ? std::string{} : hourly(hour), ""}}}; };
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
            x.palette = "precip";
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
                x.wants = {want("z", "HGT", "700 mb"), want("rh", "RH", "700 mb"), want("w", hrrr ? "VVEL" : "DZDT", "700 mb"), want("u", "UGRD", "700 mb"), want("v", "VGRD", "700 mb")};
                x.barbU = "u";
                x.barbV = "v";
                x.derive = [hrrr] (Grids& g, const Context&) { g["o"] = GfsGrid::scaled(g["w"], hrrr ? 1.0 : -0.9 * 9.80665); };   // (the HRRR gives omega itself, in Pa/s)
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
                omega.smoothKm = 30.0;
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
                x.palette = "uh";
                x.fillTitle = "Strongest updraft helicity since the start of the run (m2/s2)";
                x.legendStep = 0;
                p.push_back(x);
            }
            {
                auto x = make("helicity", "Helicity and 30m Wind");
                x.wants = {want("h", "HLCY", "3000-0 m above ground"), want("u", "UGRD", "30 m above ground"), want("v", "VGRD", "30 m above ground")};
                x.fill = [] (const Grids& g) { return pick(g, "h"); };
                x.ramp = helicityRamp();
                x.palette = "helicity";
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
            // ---- the "Parametric" indices of the old SPC-style screen (IndexViewer), now charts like the rest (so the maximum over a day, the change since an earlier run, the hover and the compare tiles all
            // work on them). Formulas as that screen had them (docs/derived-severe-indices-plan.md).
            {   // SHIP: the significant hail parameter, with the model's own hail size as lines (inches)
                auto x = make("ship", "SHIP (Significant Hail Parameter)");
                x.needs = [=] (int hour) {
                    std::vector<GfsData::Need> out;
                    if (hour < 1) {
                        return out;
                    }
                    out.push_back({hour, {"hail", "HAIL", "surface", std::to_string(hour - 1) + "-" + std::to_string(hour) + " hour max fcst", ""}});
                    for (const auto& w : {want("cape", "CAPE", "180-0 mb above ground"), want("d2", "DPT", "2 m above ground"), want("sp", "PRES", "surface"), want("us", "VUCSH", "0-6000 m above ground"),
                                          want("vs", "VVCSH", "0-6000 m above ground"), want("fz", "HGT", "0C isotherm"), want("terr", "HGT", "surface"), want("t7", "TMP", "700 mb"), want("t5", "TMP", "500 mb"),
                                          want("z7", "HGT", "700 mb"), want("z5", "HGT", "500 mb")}) {
                        out.push_back({hour, w});
                    }
                    return out;
                };
                x.derive = [] (Grids& g, const Context&) {
                    auto out = g["cape"];
                    for (size_t i = 0; i < out.values.size(); i++) {
                        const double cape = g["cape"].values[i], d2 = g["d2"].values[i], sp = g["sp"].values[i], t5 = g["t5"].values[i];
                        const double e = 6.112 * std::exp(17.67 * d2 / (d2 + 243.5));
                        const double mixr = 622.0 * e / (sp / 100.0 - e);                                                        // g/kg
                        const double lr75 = (g["t7"].values[i] - t5) / ((g["z5"].values[i] - g["z7"].values[i]) / 1000.0);      // C/km
                        const double shear = std::hypot(g["us"].values[i], g["vs"].values[i]);                                   // m/s
                        const double frz = g["fz"].values[i] - g["terr"].values[i];                                             // m above the ground
                        double ship = cape * std::clamp(mixr, 11.0, 13.6) * lr75 * (-std::min(t5, -5.5)) * std::clamp(shear, 7.0, 27.0) / 42000000.0;
                        ship *= (cape < 1300.0 ? cape / 1300.0 : 1.0) * (lr75 < 5.8 ? lr75 / 5.8 : 1.0) * (frz < 2400.0 ? frz / 2400.0 : 1.0);
                        out.values[i] = std::isfinite(ship) ? static_cast<float>(std::max(0.0, ship)) : std::nanf("");
                    }
                    g["ship"] = std::move(out);
                };
                x.fill = [] (const Grids& g) { return pick(g, "ship"); };
                x.ramp = Ramp{{{0.0, QColor{0, 0, 40, 0}}, {0.25, QColor{0, 0, 40, 0}}, {0.5, QColor{40, 60, 200}}, {1.0, QColor{60, 170, 90}}, {1.5, QColor{230, 220, 60}}, {2.0, QColor{240, 150, 40}},
                               {3.0, QColor{220, 40, 40}}, {4.0, QColor{170, 30, 150}}, {6.0, QColor{255, 255, 255}}}};
                x.fillTitle = "SHIP: under 1 not significant, 1-4 favorable for hail of 2 in or more, over 4 very high (lines: the model's hail size, in)";
                x.legendStep = 1;
                ContourSet hail;
                hail.key = "hail";
                hail.scale = 39.3701;
                hail.base = 0.5;
                hail.interval = 0.5;
                hail.minimum = 0.75;
                hail.title = "Hail size (in)";
                hail.color = QColor{20, 20, 20};
                hail.width = 1.6;
                x.contours = {hail};
                p.push_back(x);
            }
            {   // STP: the significant tornado parameter, fixed layer (Thompson et al. 2003)
                auto x = make("stp", "STP (Significant Tornado Parameter, fixed layer)");
                x.wants = {want("sbcape", "CAPE", "surface"), want("sbcin", "CIN", "surface"), want("srh1", "HLCY", "1000-0 m above ground"), want("us", "VUCSH", "0-6000 m above ground"),
                           want("vs", "VVCSH", "0-6000 m above ground"), want("lcl", "HGT", "level of adiabatic condensation from sfc"), want("terr", "HGT", "surface")};
                x.derive = [] (Grids& g, const Context&) {
                    auto out = g["sbcape"];
                    for (size_t i = 0; i < out.values.size(); i++) {
                        const double lcl = g["lcl"].values[i] - g["terr"].values[i], shear = std::hypot(g["us"].values[i], g["vs"].values[i]), cin = g["sbcin"].values[i];
                        const double stp = (g["sbcape"].values[i] / 1500.0) * (lcl < 1000.0 ? 1.0 : (lcl > 2000.0 ? 0.0 : (2000.0 - lcl) / 1000.0)) * (g["srh1"].values[i] / 150.0) *
                            (shear > 30.0 ? 1.5 : (shear < 12.5 ? 0.0 : shear / 20.0)) * (cin > -50.0 ? 1.0 : (cin < -200.0 ? 0.0 : (200.0 + cin) / 150.0));
                        out.values[i] = std::isfinite(stp) ? static_cast<float>(std::max(0.0, stp)) : std::nanf("");
                    }
                    g["stp"] = std::move(out);
                };
                x.fill = [] (const Grids& g) { return pick(g, "stp"); };
                x.ramp = Ramp{{{0.0, QColor{0, 0, 40, 0}}, {0.25, QColor{0, 0, 40, 0}}, {0.5, QColor{40, 60, 200}}, {1.0, QColor{60, 170, 90}}, {2.0, QColor{230, 220, 60}}, {3.0, QColor{240, 150, 40}},
                               {5.0, QColor{220, 40, 40}}, {8.0, QColor{170, 30, 150}}, {12.0, QColor{255, 255, 255}}}};
                x.fillTitle = "STP: over 1, most significant tornadoes with a supercell; under 1, most non-tornadic ones";
                x.legendStep = 1;
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
                x.palette = "prob";
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
                x.palette = "prob";
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
                x.palette = "uh";
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
            // one member of the field on its own (the old REFS screen's "RRFS Ensemble Member" rows): member 1 here, the screen's member picker swaps it for 2 to 5 (withMember)
            {
                const auto one = [&] (const char * key, const char * label, const char * variable, const char * level, bool hourlyMax, const Ramp& ramp, const char * title, int legendStep,
                                      Quantity quantity, float scale) {
                    auto x = make((std::string{"member_"} + key).c_str(), (std::string{"Member: "} + label).c_str());
                    const GfsData::Want want{"v", variable, level, "", "", "m001"};
                    if (!hourlyMax) {
                        x.wants = {want};
                    } else {
                        x.needs = [want] (int hour) {
                            auto w = want;
                            w.forecast = std::to_string(std::max(hour - 1, 0)) + "-" + std::to_string(hour) + " hour max fcst";
                            return std::vector<GfsData::Need>{{hour, w}};
                        };
                    }
                    x.fill = [scale] (const Grids& g) {
                        auto out = pick(g, "v");
                        if (scale != 1.0f) {
                            for (auto& v : out.values) {
                                v *= scale;
                            }
                        }
                        return out;
                    };
                    x.ramp = ramp;
                    if (hourlyMax) {
                        x.palette = "uh";
                    }
                    x.quantity = quantity;
                    x.fillTitle = title;
                    x.legendStep = legendStep;
                    p.push_back(x);
                };
                one("refc", "Composite Reflectivity", "REFC", "entire atmosphere (considered as a single layer)", false, reflectivity(), "Composite reflectivity (dBZ)", 10, Quantity::Other, 1.0f);
                one("tmp2m", "2 m Temperature", "TMP", "2 m above ground", false, temperature(), "2 m temperature", 5, Quantity::Temperature, 1.0f);
                one("cape", "Surface CAPE", "CAPE", "surface", false, capeRamp(), "Surface CAPE (J/kg)", 500, Quantity::Other, 1.0f);
                one("gust", "Surface Wind Gust", "GUST", "surface", false, windSpeed(), "Surface wind gust (kt)", 20, Quantity::Other, 1.943844f);
                one("uphl", "2-5 km Updraft Helicity (last hour)", "MXUPHL", "5000-2000 m above ground", true, updraftHelicity(), "Updraft helicity (m2/s2)", 0, Quantity::Other, 1.0f);
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
                x.palette = "uh";
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
            x.palette = "precip";
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
                x.palette = "prob";
                x.fillTitle = "Chance (%, within the neighborhood)";
                x.legendStep = 10;
                p.push_back(x);
            }
            // ---- the maps SREF had (REFS replaces it): the ensemble mean, spread and probability files of REFS itself (kind "mean", "sprd", "prob", "eas": the 5 members, on the 3 km grid) and,
            // where those hold no such field, the mean of the members' own fields. Five members: the chances come in steps of 20 per cent (the neighborhood ones, "eas", are smoother).
            const auto ens = [] (const std::string& key, const char * variable, const char * level, const std::string& forecast, const char * kind, const char * detail) {
                return GfsData::Want{key, variable, level, forecast, detail, std::string{"ens:"} + kind};
            };
            const auto atHour = [] (int hour) { return std::to_string(hour) + " hour fcst"; };
            const auto accum = [] (int from, int to) { return std::to_string(from) + "-" + std::to_string(to) + " hour acc fcst"; };
            const auto meanOfMembers = [] (Grids& g, const char * prefix, const char * out) {
                auto mean = g.at(std::string{prefix} + "1");
                for (size_t i = 0; i < mean.values.size(); i++) {
                    double sum = 0.0;
                    int n = 0;
                    for (int m = 1; m <= 5; m++) {
                        const float v = g.at(std::string{prefix} + std::to_string(m)).values[i];
                        if (!std::isnan(v)) {
                            sum += v;
                            n++;
                        }
                    }
                    mean.values[i] = n > 0 ? static_cast<float>(sum / n) : std::nanf("");
                }
                g[out] = std::move(mean);
            };
            for (const int period : {6, 12, 24}) {   // the mean precipitation of 6, 12 and 24 hours: the mean's own 6 hour amounts added up
                auto x = make(("ens_precip_p" + std::string{period < 10 ? "0" : ""} + std::to_string(period)).c_str(), ("Mean " + std::to_string(period) + "-hour Precipitation").c_str());
                x.needs = [ens, accum, period] (int hour) {
                    std::vector<GfsData::Need> out;
                    if (hour < period) {
                        return out;
                    }
                    int n = 0;
                    for (int end = hour; end > hour - period; end -= 6) {
                        out.push_back({end, ens("a" + std::to_string(n++), "APCP", "surface", accum(end - 6, end), "mean", "wt ens mean")});
                    }
                    return out;
                };
                x.derive = [] (Grids& g, const Context&) {
                    auto sum = g["a0"];
                    for (int k = 1; g.count("a" + std::to_string(k)); k++) {
                        for (size_t i = 0; i < sum.values.size(); i++) {
                            sum.values[i] += g["a" + std::to_string(k)].values[i];
                        }
                    }
                    g["f"] = std::move(sum);
                };
                x.fill = [] (const Grids& g) { return pick(g, "f"); };
                x.ramp = precipitation();
                x.palette = "precip";
                x.quantity = Quantity::Millimeters;
                x.fillTitle = "Mean precipitation in the last " + std::to_string(period) + " hours";
                x.legendStep = 0;
                p.push_back(x);
            }
            for (const auto& [id, inches, millimeters] : {std::tuple{"ens_prob_precip_6h_0.25in", "0.25", "prob >6.35"}, {"ens_prob_precip_6h_0.5in", "0.5", "prob >12.7"}, {"ens_prob_precip_6h_1in", "1", "prob >25.4"}}) {
                auto x = make(id, (std::string{"Probability of 6-hour Precipitation over "} + inches + " in").c_str());
                const std::string detail = millimeters;
                x.needs = [ens, accum, detail] (int hour) { return hour < 6 ? std::vector<GfsData::Need>{} : std::vector<GfsData::Need>{{hour, ens("a", "APCP", "surface", accum(hour - 6, hour), "eas", detail.c_str())}}; };
                x.fill = [] (const Grids& g) { return GfsGrid::scaled(pick(g, "a"), 100.0); };
                x.ramp = probability();
                x.palette = "prob";
                x.fillTitle = std::string{"Chance of more than "} + inches + " in of rain in 6 hours (%, within the neighborhood)";
                x.legendStep = 10;
                p.push_back(x);
            }
            for (const bool spread : {false, true}) {   // snow since the start of the run: the mean and the spread of the members' own totals (the mean file has the total only for the first 12 hours)
                auto x = make(spread ? "ens_snow_total_sprd" : "ens_snow_total_mean", spread ? "Snow Total Spread" : "Snow Total Mean");
                x.needs = [members, accum] (int hour) {
                    std::vector<GfsData::Need> out;
                    if (hour < 1) {
                        return out;
                    }
                    for (auto w : members("s", "ASNOW", "surface", accum(0, hour))) {
                        out.push_back({hour, w});
                    }
                    return out;
                };
                x.derive = [spread, meanOfMembers] (Grids& g, const Context&) {
                    meanOfMembers(g, "s", "mean");
                    auto out = g["mean"];
                    if (spread) {
                        for (size_t i = 0; i < out.values.size(); i++) {
                            double square = 0.0;
                            int n = 0;
                            for (int m = 1; m <= 5; m++) {
                                const float v = g.at("s" + std::to_string(m)).values[i];
                                if (!std::isnan(v)) {
                                    square += (v - g["mean"].values[i]) * (v - g["mean"].values[i]);
                                    n++;
                                }
                            }
                            out.values[i] = n > 0 ? static_cast<float>(std::sqrt(square / n)) : std::nanf("");
                        }
                    }
                    g["f"] = GfsGrid::scaled(out, 100.0);   // the file's meters, as centimeters
                };
                x.fill = [] (const Grids& g) { return pick(g, "f"); };
                x.ramp = snowfall();
                x.quantity = Quantity::Centimeters;
                x.fillTitle = spread ? "Spread of the snow since the start of the run (the 5 members)" : "Mean snow since the start of the run (the 5 members)";
                x.legendStep = 0;
                p.push_back(x);
            }
            // the thickness of a layer: the mean of the high level less the mean of the members' low one (the files have no 1000 mb mean)
            for (const auto& [id, label, low, high, interval, split] : {std::tuple{"ens_1000_500_thick", "Mean 1000-500mb Thickness", "1000 mb", "500 mb", 6.0, 540.0}, {"ens_1000_850_thick", "Mean 1000-850mb Thickness", "1000 mb", "850 mb", 3.0, 130.0},
                                                                         {"ens_850_700_thick", "Mean 850-700mb Thickness", "850 mb", "700 mb", 3.0, 154.0}}) {
                auto x = make(id, label);
                const std::string lowLevel = low, highLevel = high;
                x.needs = [ens, members, atHour, lowLevel, highLevel] (int hour) {
                    std::vector<GfsData::Need> out;
                    for (auto w : members("l", "HGT", lowLevel.c_str(), "")) {
                        out.push_back({hour, w});
                    }
                    out.push_back({hour, ens("h", "HGT", highLevel.c_str(), atHour(hour), "mean", "wt ens mean")});
                    return out;
                };
                x.derive = [meanOfMembers] (Grids& g, const Context&) {
                    meanOfMembers(g, "l", "lm");
                    g["thick"] = GfsGrid::scaled(GfsGrid::difference(g["h"], g["lm"]), 0.1);   // meters -> decameters
                    g.erase("l1"); g.erase("l2"); g.erase("l3"); g.erase("l4"); g.erase("l5");
                };
                x.fill = [] (const Grids& g) { return pick(g, "thick"); };
                const double lo = split - 8 * interval, hi = split + 8 * interval;
                x.ramp = Ramp{{{lo, QColor{"#2b4fb0"}}, {lo + (hi - lo) * 0.25, QColor{"#5aa0d8"}}, {lo + (hi - lo) * 0.45, QColor{"#bfe3ee"}}, {split, QColor{"#f4f1c0"}}, {lo + (hi - lo) * 0.65, QColor{"#f5c06a"}},
                               {lo + (hi - lo) * 0.85, QColor{"#e0642c"}}, {hi, QColor{"#a01c1c"}}}};
                x.fillTitle = std::string{label} + " (dam)";
                x.legendStep = static_cast<double>(interval) * 2;
                ContourSet t;
                t.key = "thick";
                t.interval = interval;
                t.title = std::string{label} + " (dam)";
                t.color = QColor{190, 50, 40};
                t.colorBelow = QColor{40, 90, 190};
                t.split = split;
                t.width = 1.3;
                x.contours = {t};
                p.push_back(x);
            }
            {   // the mean 10 m wind (kt) with barbs, and the chance of 25 kt or more from the members
                auto x = make("ens_10m_wind", "Mean 10m Winds");
                x.needs = [ens, atHour] (int hour) {
                    return std::vector<GfsData::Need>{{hour, ens("w", "WIND", "10 m above ground", atHour(hour), "mean", "wt ens mean")}, {hour, ens("u", "UGRD", "10 m above ground", atHour(hour), "mean", "wt ens mean")},
                                                      {hour, ens("v", "VGRD", "10 m above ground", atHour(hour), "mean", "wt ens mean")}};
                };
                x.fill = [] (const Grids& g) { return GfsGrid::scaled(pick(g, "w"), 1.943844); };
                x.ramp = lowWind();
                x.fillTitle = "Mean 10 m wind speed (kt)";
                x.legendStep = 10;
                x.barbU = "u";
                x.barbV = "v";
                p.push_back(x);
            }
            {
                auto x = make("ens_prob_10m_wind_25kt", "Probability of 10m Wind Speeds over 25 kt");
                x.wants = members("u", "UGRD", "10 m above ground", "");
                for (auto& w : members("v", "VGRD", "10 m above ground", "")) {
                    x.wants.push_back(w);
                }
                x.derive = [] (Grids& g, const Context&) {
                    for (int m = 1; m <= 5; m++) {
                        g["s" + std::to_string(m)] = GfsGrid::speed(g.at("u" + std::to_string(m)), g.at("v" + std::to_string(m)));
                    }
                };
                x.fill = [fraction] (const Grids& g) { return fraction(g, "s", 25.0 * 0.514444); };
                x.ramp = probability();
                x.palette = "prob";
                x.fillTitle = "Chance of a 10 m wind over 25 kt (% of the members, nearby)";
                x.legendStep = 10;
                p.push_back(x);
            }
            {
                auto x = make("ens_2m_temp", "Mean 2m Temperature");
                x.needs = [ens, atHour] (int hour) { return std::vector<GfsData::Need>{{hour, ens("t", "TMP", "2 m above ground", atHour(hour), "mean", "wt ens mean")}}; };
                x.fill = [] (const Grids& g) { return pick(g, "t"); };
                x.ramp = temperature();
                x.quantity = Quantity::Temperature;
                x.fillTitle = "Mean 2 m temperature";
                x.legendStep = 5;
                p.push_back(x);
            }
            {
                auto x = make("ens_prob_2m_temp_0C", "Probability of 2m Temperature under 0 C");
                x.needs = [ens, atHour] (int hour) { return std::vector<GfsData::Need>{{hour, ens("t", "TMP", "2 m above ground", atHour(hour), "prob", "prob <273.15")}}; };
                x.fill = [] (const Grids& g) { return GfsGrid::scaled(pick(g, "t"), 100.0); };
                x.ramp = probability();
                x.palette = "prob";
                x.fillTitle = "Chance of a 2 m temperature under 0 C (% of the members)";
                x.legendStep = 10;
                p.push_back(x);
            }
            {
                auto x = make("ens_cape", "Mean Convective Available Potential Energy");
                x.needs = [ens, atHour] (int hour) { return std::vector<GfsData::Need>{{hour, ens("c", "CAPE", "surface", atHour(hour), "mean", "wt ens mean")}}; };
                x.fill = [] (const Grids& g) { return pick(g, "c"); };
                x.ramp = capeRamp();
                x.fillTitle = "Mean surface-based CAPE (J/kg)";
                x.legendStep = 500;
                p.push_back(x);
            }
            {
                auto x = make("ens_cin", "Mean Convective Inhibition");
                x.needs = [ens, atHour] (int hour) { return std::vector<GfsData::Need>{{hour, ens("c", "CIN", "surface", atHour(hour), "mean", "wt ens mean")}}; };
                x.fill = [] (const Grids& g) { return pick(g, "c"); };
                x.ramp = Ramp{{{-300, QColor{"#5b2a8a"}}, {-200, QColor{"#3a55b8"}}, {-100, QColor{"#5aa0d8"}}, {-25, QColor{"#dff3f8"}}, {0, QColor{223, 243, 248, 0}}}};
                x.fillTitle = "Mean surface-based CIN (J/kg)";
                x.legendStep = 50;
                p.push_back(x);
            }
            for (const int joules : {500, 1000, 2000}) {   // the ready-made chances are of the mixed-layer parcel
                auto x = make(("ens_prob_cape_" + std::to_string(joules)).c_str(), ("Probability of CAPE over " + std::to_string(joules)).c_str());
                const std::string detail = "prob >" + std::to_string(joules);
                x.needs = [ens, atHour, detail] (int hour) { return std::vector<GfsData::Need>{{hour, ens("c", "CAPE", "90-0 mb above ground", atHour(hour), "prob", detail.c_str())}}; };
                x.fill = [] (const Grids& g) { return GfsGrid::scaled(pick(g, "c"), 100.0); };
                x.ramp = probability();
                x.palette = "prob";
                x.fillTitle = "Chance of mixed-layer CAPE over " + std::to_string(joules) + " J/kg (% of the members)";
                x.legendStep = 10;
                p.push_back(x);
            }
            {   // the sea level pressure: the mean as lines, the spread as the fill
                auto x = make("ens_mslp", "Mean Sea Level Pressure and Spread");
                x.needs = [ens, atHour] (int hour) {
                    return std::vector<GfsData::Need>{{hour, ens("p", "MSLET", "mean sea level", atHour(hour), "mean", "wt ens mean")}, {hour, ens("s", "MSLET", "mean sea level", atHour(hour), "sprd", "ens spread")}};
                };
                x.fill = [] (const Grids& g) { return GfsGrid::scaled(pick(g, "s"), 0.01); };
                x.ramp = spreadRamp(6.0);
                x.fillTitle = "Spread of the sea level pressure (mb)";
                x.legendStep = 1;
                x.contours = {pressure()};
                p.push_back(x);
            }
            // the mean of the upper-air fields at a level (height, wind, temperature, humidity): filled with what the map is about, heights as lines
            const auto meanUpper = [make, ens, atHour, heights, speedOf] (const char * id, const char * label, const char * level, double interval, bool temperatureToo, bool humidityToo) {
                auto x = make(id, label);
                const std::string where = level;
                x.needs = [ens, atHour, where, temperatureToo, humidityToo] (int hour) {
                    std::vector<GfsData::Need> out{{hour, ens("z", "HGT", where.c_str(), atHour(hour), "mean", "wt ens mean")}, {hour, ens("u", "UGRD", where.c_str(), atHour(hour), "mean", "wt ens mean")},
                                                   {hour, ens("v", "VGRD", where.c_str(), atHour(hour), "mean", "wt ens mean")}};
                    if (temperatureToo) {
                        out.push_back({hour, ens("t", "TMP", where.c_str(), atHour(hour), "mean", "wt ens mean")});
                    }
                    if (humidityToo) {
                        out.push_back({hour, ens("rh", "RH", where.c_str(), atHour(hour), "mean", "wt ens mean")});
                    }
                    return out;
                };
                x.fill = speedOf("u", "v");
                x.ramp = windSpeed();
                x.fillTitle = "Mean wind speed (kt)";
                x.legendStep = 20;
                x.contours = {heights(interval)};
                x.barbU = "u";
                x.barbV = "v";
                return x;
            };
            for (const auto& [id, label, level, interval] : {std::tuple{"ens_250_vort_ht", "250mb Vorticity and Height", "250 mb", 12.0}, {"ens_500_vort_ht", "500mb Vorticity and Height", "500 mb", 6.0}}) {
                auto x = meanUpper(id, label, level, interval, false, false);
                x.fill = vorticityOf;
                x.ramp = vorticityRamp();
                x.fillTitle = "Vorticity of the mean wind (1e-5 /s)";
                x.legendStep = 10;
                x.barbU.clear();
                x.barbV.clear();
                p.push_back(x);
            }
            for (const auto& [id, label, level, interval] : {std::tuple{"ens_250_wnd", "250mb Wind", "250 mb", 12.0}, {"ens_850_wnd", "850mb Wind", "850 mb", 3.0}}) {
                p.push_back(meanUpper(id, label, level, interval, false, false));
            }
            for (const auto& [id, label, level, interval] : {std::tuple{"ens_700_temp", "700mb Temperature", "700 mb", 3.0}, {"ens_850_temp", "850mb Temperature", "850 mb", 3.0}}) {
                auto x = meanUpper(id, label, level, interval, true, false);
                x.fill = [] (const Grids& g) { return pick(g, "t"); };
                x.ramp = temperature();
                x.quantity = Quantity::Temperature;
                x.fillTitle = "Mean temperature";
                x.legendStep = 5;
                p.push_back(x);
            }
            {
                auto x = meanUpper("ens_700_rh", "700mb Relative Humidity", "700 mb", 3.0, false, true);
                x.fill = [] (const Grids& g) { return pick(g, "rh"); };
                x.ramp = humidity();
                x.palette = "rh";
                x.fillTitle = "Mean relative humidity (%)";
                x.legendStep = 10;
                p.push_back(x);
            }
            {   // 850 mb: the file has no mean humidity there, so the mean of the members'
                auto x = meanUpper("ens_850_rh", "850mb Relative Humidity", "850 mb", 3.0, false, false);
                const auto inner = x.needs;
                x.needs = [inner, members] (int hour) {
                    auto out = inner(hour);
                    for (auto w : members("r", "RH", "850 mb", "")) {
                        out.push_back({hour, w});
                    }
                    return out;
                };
                x.derive = [meanOfMembers] (Grids& g, const Context&) {
                    meanOfMembers(g, "r", "rh");
                    g.erase("r1"); g.erase("r2"); g.erase("r3"); g.erase("r4"); g.erase("r5");
                };
                x.fill = [] (const Grids& g) { return pick(g, "rh"); };
                x.ramp = humidity();
                x.palette = "rh";
                x.fillTitle = "Mean relative humidity (%)";
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

        // ---- HREF: the 10 member ensemble's own ready-made files (the mean, the probability-matched mean, the localized one, the blend, the probabilities and the agreement-scale probabilities).
        // Every one of the model guidance site's 71 HREF maps is a row below: which file, which field, over what time, which threshold, and how it is colored.
        const auto hrefRecipes = [&] {
            enum class Time { Instant, OneHour, ThreeHour, Total };
            enum class Look { Precip, Snow, Radar, Prob, Temp, Dew, Pwat, Cape, Vis, Shear, Plain };
            struct Row {
                const char * id;
                const char * label;
                const char * kind;
                const char * variable;
                const char * level;
                Time time;
                const char * detail;
                double scale;
                Look look;
                const char * title;
                bool pressure;
            };
            const char * const surface = "surface";
            const char * const column = "entire atmosphere (considered as a single layer)";
            const char * const ground = "1000 m above ground";
            const std::vector<Row> rows{
                {"mean_precip_p01", "Mean 1-hour Precipitation and MSLP", "mean", "APCP", surface, Time::OneHour, "", 1, Look::Precip, "Mean precipitation in the hour", true},
                {"mean_precip_p03", "Mean 3-hour Precipitation and MSLP", "mean", "APCP", surface, Time::ThreeHour, "", 1, Look::Precip, "Mean precipitation in 3 hours", true},
                {"mean_precip_ptot", "Mean Total Precipitation and MSLP", "mean", "APCP", surface, Time::Total, "", 1, Look::Precip, "Mean precipitation since the start of the run", true},
                {"blend_mean_precip_p01", "Blend Mean 1-hour Precipitation", "avrg", "APCP", surface, Time::OneHour, "", 1, Look::Precip, "Blended mean precipitation in the hour", false},
                {"blend_mean_precip_p03", "Blend Mean 3-hour Precipitation", "avrg", "APCP", surface, Time::ThreeHour, "", 1, Look::Precip, "Blended mean precipitation in 3 hours", false},
                {"blend_mean_precip_ptot", "Blend Mean Total Precipitation", "avrg", "APCP", surface, Time::Total, "", 1, Look::Precip, "Blended mean precipitation since the start of the run", false},
                {"lpmm_mean_precip_p01", "Localized Probability Matched Mean 1-hour Precipitation", "lpmm", "APCP", surface, Time::OneHour, "", 1, Look::Precip, "Localized probability-matched mean, the hour", false},
                {"lpmm_mean_precip_p03", "Localized Probability Matched Mean 3-hour Precipitation", "lpmm", "APCP", surface, Time::ThreeHour, "", 1, Look::Precip, "Localized probability-matched mean, 3 hours", false},
                {"lpmm_mean_precip_ptot", "Localized Probability Matched Mean Total Precipitation", "lpmm", "APCP", surface, Time::Total, "", 1, Look::Precip, "Localized probability-matched mean since the start of the run", false},
                {"pmm_refd_1km", "Probability Matched Mean of 1 km Reflectivity", "pmmn", "REFD", ground, Time::Instant, "", 1, Look::Radar, "Probability-matched mean of the 1 km reflectivity (dBZ)", false},
                {"pmm_refd_1km_emsl", "Probability Matched Mean of 1 km Reflectivity and MSLP", "pmmn", "REFD", ground, Time::Instant, "", 1, Look::Radar, "Probability-matched mean of the 1 km reflectivity (dBZ)", true},
                {"pmm_refd_max", "Probability Matched Mean of Maximum Reflectivity", "pmmn", "MAXREF", ground, Time::Instant, "", 1, Look::Radar, "Probability-matched mean of the hourly maximum reflectivity (dBZ)", false},
                {"pmm_refd_max_emsl", "Probability Matched Mean of Maximum Reflectivity and MSLP", "pmmn", "MAXREF", ground, Time::Instant, "", 1, Look::Radar, "Probability-matched mean of the hourly maximum reflectivity (dBZ)", true},
                {"prob_refd_40dbz", "Probability of 1 km Reflectivity over 40 dBZ", "prob", "REFD", ground, Time::Instant, "prob >40", 1, Look::Prob, "Chance of 1 km reflectivity over 40 dBZ (%)", false},
                {"prob_refd_max_40dbz", "Probability of 1 km Maximum Reflectivity over 40 dBZ", "prob", "MAXREF", ground, Time::Instant, "prob >40", 1, Look::Prob, "Chance of the hourly maximum reflectivity over 40 dBZ (%)", false},
                {"prob_cref_40dbz", "Probability of Composite Reflectivity over 40 dBZ", "prob", "REFC", column, Time::Instant, "prob >40", 1, Look::Prob, "Chance of composite reflectivity over 40 dBZ (%)", false},
                {"prob_cref_50dbz", "Probability of Composite Reflectivity over 50 dBZ", "prob", "REFC", column, Time::Instant, "prob >50", 1, Look::Prob, "Chance of composite reflectivity over 50 dBZ (%)", false},
                {"prob_rain", "Probability of Rain", "prob", "CRAIN", surface, Time::Instant, "prob >=1 <0", 1, Look::Prob, "Chance of rain (%)", false},
                {"prob_snow", "Probability of Snow", "prob", "CSNOW", surface, Time::Instant, "prob >=1 <0", 1, Look::Prob, "Chance of snow (%)", false},
                {"prob_sleet", "Probability of Sleet", "prob", "CICEP", surface, Time::Instant, "prob >=1 <0", 1, Look::Prob, "Chance of sleet (%)", false},
                {"prob_freezing_rain", "Probability of Freezing Rain", "prob", "CFRZR", surface, Time::Instant, "prob >=1 <0", 1, Look::Prob, "Chance of freezing rain (%)", false},
                {"prob_3h_rain_0.5in", "Probability of 3-hour Rain over 0.5 in", "prob", "APCP", surface, Time::ThreeHour, "prob >12.7", 1, Look::Prob, "Chance of more than 0.5 in of rain in 3 hours (%)", false},
                {"prob_3h_rain_1in", "Probability of 3-hour Rain over 1 in", "prob", "APCP", surface, Time::ThreeHour, "prob >25.4", 1, Look::Prob, "Chance of more than 1 in of rain in 3 hours (%)", false},
                {"eas_prob_1h_rain_0.01in", "Ensemble Agreement Scale Probability of 1-hour Rain over 0.01 in", "eas", "APCP", surface, Time::OneHour, "prob >0.254", 1, Look::Prob, "Neighborhood chance of more than 0.01 in of rain in the hour (%)", false},
                {"eas_prob_1h_rain_0.25in", "Ensemble Agreement Scale Probability of 1-hour Rain over 0.25 in", "eas", "APCP", surface, Time::OneHour, "prob >6.35", 1, Look::Prob, "Neighborhood chance of more than 0.25 in of rain in the hour (%)", false},
                {"eas_prob_1h_rain_0.5in", "Ensemble Agreement Scale Probability of 1-hour Rain over 0.5 in", "eas", "APCP", surface, Time::OneHour, "prob >12.7", 1, Look::Prob, "Neighborhood chance of more than 0.5 in of rain in the hour (%)", false},
                {"eas_prob_3h_rain_0.01in", "Ensemble Agreement Scale Probability of 3-hour Rain over 0.01 in", "eas", "APCP", surface, Time::ThreeHour, "prob >0.254", 1, Look::Prob, "Neighborhood chance of more than 0.01 in of rain in 3 hours (%)", false},
                {"eas_prob_3h_rain_0.25in", "Ensemble Agreement Scale Probability of 3-hour Rain over 0.25 in", "eas", "APCP", surface, Time::ThreeHour, "prob >6.35", 1, Look::Prob, "Neighborhood chance of more than 0.25 in of rain in 3 hours (%)", false},
                {"eas_prob_3h_rain_0.5in", "Ensemble Agreement Scale Probability of 3-hour Rain over 0.5 in", "eas", "APCP", surface, Time::ThreeHour, "prob >12.7", 1, Look::Prob, "Neighborhood chance of more than 0.5 in of rain in 3 hours (%)", false},
                {"mean_snow_1h", "Mean 1-hour Snow (10 to 1 ratio)", "mean", "WEASD", surface, Time::OneHour, "", 1, Look::Snow, "Mean snowfall in the hour, from the water at 10 to 1", false},
                {"mean_snow_3h", "Mean 3-hour Snow (10 to 1 ratio)", "mean", "WEASD", surface, Time::ThreeHour, "", 1, Look::Snow, "Mean snowfall in 3 hours, from the water at 10 to 1", false},
                {"mean_snow_total", "Mean Total Snow (10 to 1 ratio)", "mean", "WEASD", surface, Time::Total, "", 1, Look::Snow, "Mean snowfall since the start of the run, from the water at 10 to 1", false},
                {"prob_1h_snow_1in", "Probability of 1-hour Snow over 1 in", "prob", "WEASD", surface, Time::OneHour, "prob >2.54", 1, Look::Prob, "Chance of more than 1 in of snow in the hour (%)", false},
                {"prob_3h_snow_1in", "Probability of 3-hour Snow over 1 in", "prob", "WEASD", surface, Time::ThreeHour, "prob >2.54", 1, Look::Prob, "Chance of more than 1 in of snow in 3 hours (%)", false},
                {"prob_3h_snow_3in", "Probability of 3-hour Snow over 3 in", "prob", "WEASD", surface, Time::ThreeHour, "prob >7.62", 1, Look::Prob, "Chance of more than 3 in of snow in 3 hours (%)", false},
                {"eas_prob_1h_snow_0.1in", "Ensemble Agreement Scale Probability of 1-hour Snow over 0.1 in", "eas", "WEASD", surface, Time::OneHour, "prob >2.54", 1, Look::Prob, "Neighborhood chance of more than 0.1 in of snow in the hour (%)", false},
                {"eas_prob_1h_snow_0.3in", "Ensemble Agreement Scale Probability of 1-hour Snow over 0.3 in", "eas", "WEASD", surface, Time::OneHour, "prob >7.62", 1, Look::Prob, "Neighborhood chance of more than 0.3 in of snow in the hour (%)", false},
                {"eas_prob_3h_snow_0.1in", "Ensemble Agreement Scale Probability of 3-hour Snow over 0.1 in", "eas", "WEASD", surface, Time::ThreeHour, "prob >2.54", 1, Look::Prob, "Neighborhood chance of more than 0.1 in of snow in 3 hours (%)", false},
                {"eas_prob_3h_snow_0.3in", "Ensemble Agreement Scale Probability of 3-hour Snow over 0.3 in", "eas", "WEASD", surface, Time::ThreeHour, "prob >7.62", 1, Look::Prob, "Neighborhood chance of more than 0.3 in of snow in 3 hours (%)", false},
                {"mean_pwat", "Mean Precipitable Water", "mean", "PWAT", column, Time::Instant, "", 1, Look::Pwat, "Mean precipitable water", false},
                {"prob_pwat_1.5in", "Probability of Precipitable Water over 1.5 in", "prob", "PWAT", column, Time::Instant, "prob >37.5", 1, Look::Prob, "Chance of precipitable water over 1.5 in (%)", false},
                {"prob_pwat_2in", "Probability of Precipitable Water over 2 in", "prob", "PWAT", column, Time::Instant, "prob >50", 1, Look::Prob, "Chance of precipitable water over 2 in (%)", false},
                {"mean_2m_temp", "Mean 2 m Temperature", "mean", "TMP", "2 m above ground", Time::Instant, "", 1, Look::Temp, "Mean 2 m temperature", false},
                {"prob_2m_temp_0C", "Probability of 2 m Temperature under 0 C", "prob", "TMP", "2 m above ground", Time::Instant, "prob <273.15", 1, Look::Prob, "Chance of a 2 m temperature under 0 C (%)", false},
                {"mean_2m_dewp", "Mean 2 m Dew Point", "mean", "DPT", "2 m above ground", Time::Instant, "", 1, Look::Dew, "Mean 2 m dew point", false},
                {"prob_2m_dewp_55F", "Probability of 2 m Dew Point over 55 F", "prob", "DPT", "2 m above ground", Time::Instant, "prob >285.93", 1, Look::Prob, "Chance of a 2 m dew point over 55 F (%)", false},
                {"prob_2m_dewp_65F", "Probability of 2 m Dew Point over 65 F", "prob", "DPT", "2 m above ground", Time::Instant, "prob >291.48", 1, Look::Prob, "Chance of a 2 m dew point over 65 F (%)", false},
                {"prob_10m_wspd_20kt", "Probability of 10 m Wind Speed over 20 kt", "prob", "WIND", "10 m above ground", Time::Instant, "prob >10.3", 1, Look::Prob, "Chance of a 10 m wind over 20 kt (%)", false},
                {"prob_10m_wspd_30kt", "Probability of 10 m Wind Speed over 30 kt", "prob", "WIND", "10 m above ground", Time::Instant, "prob >15.4", 1, Look::Prob, "Chance of a 10 m wind over 30 kt (%)", false},
                {"mean_vis", "Mean Visibility", "mean", "VIS", surface, Time::Instant, "", 1.0 / 1609.344, Look::Vis, "Mean visibility (miles)", false},
                {"prob_vis_0.5mi", "Probability of Visibility under 0.5 mile", "prob", "VIS", surface, Time::Instant, "prob <800", 1, Look::Prob, "Chance of a visibility under half a mile (%)", false},
                {"prob_etop_30000ft", "Probability of Echo Top over 30,000 ft", "prob", "RETOP", column, Time::Instant, "prob >9144", 1, Look::Prob, "Chance of an echo top over 30,000 ft (%)", false},
                {"prob_etop_35000ft", "Probability of Echo Top over 35,000 ft", "prob", "RETOP", column, Time::Instant, "prob >10668", 1, Look::Prob, "Chance of an echo top over 35,000 ft (%)", false},
                {"prob_ceil_1000ft", "Probability of Ceiling under 1,000 ft", "prob", "HGT", "cloud ceiling", Time::Instant, "prob <305", 1, Look::Prob, "Chance of a ceiling under 1,000 ft (%)", false},
                {"prob_ceil_2000ft", "Probability of Ceiling under 2,000 ft", "prob", "HGT", "cloud ceiling", Time::Instant, "prob <610", 1, Look::Prob, "Chance of a ceiling under 2,000 ft (%)", false},
                {"prob_ceil_3000ft", "Probability of Ceiling under 3,000 ft", "prob", "HGT", "cloud ceiling", Time::Instant, "prob <915", 1, Look::Prob, "Chance of a ceiling under 3,000 ft (%)", false},
                {"prob_low_IFR", "Probability of Low Instrument Flight Rules", "prob", "FLGHT", surface, Time::Instant, "prob >=1 <2", 1, Look::Prob, "Chance of low IFR (%)", false},
                {"prob_IFR", "Probability of Instrument Flight Rules", "prob", "FLGHT", surface, Time::Instant, "prob >=2 <3", 1, Look::Prob, "Chance of IFR (%)", false},
                {"prob_marginal_VFR", "Probability of Marginal Visual Flight Rules", "prob", "FLGHT", surface, Time::Instant, "prob >=3 <4", 1, Look::Prob, "Chance of marginal VFR (%)", false},
                {"prob_VFR", "Probability of Visual Flight Rules", "prob", "FLGHT", surface, Time::Instant, "prob >=4 <0", 1, Look::Prob, "Chance of VFR (%)", false},
                {"mean_ml_cape", "Mean Mixed Layer CAPE", "mean", "CAPE", "90-0 mb above ground", Time::Instant, "", 1, Look::Cape, "Mean mixed-layer CAPE (J/kg)", false},
                {"prob_cape_500", "Probability of Mixed Layer CAPE over 500", "prob", "CAPE", "90-0 mb above ground", Time::Instant, "prob >500", 1, Look::Prob, "Chance of mixed-layer CAPE over 500 J/kg (%)", false},
                {"prob_cape_1000", "Probability of Mixed Layer CAPE over 1000", "prob", "CAPE", "90-0 mb above ground", Time::Instant, "prob >1000", 1, Look::Prob, "Chance of mixed-layer CAPE over 1000 J/kg (%)", false},
                {"prob_cape_2000", "Probability of Mixed Layer CAPE over 2000", "prob", "CAPE", "90-0 mb above ground", Time::Instant, "prob >2000", 1, Look::Prob, "Chance of mixed-layer CAPE over 2000 J/kg (%)", false},
                {"prob_cape_3000", "Probability of Mixed Layer CAPE over 3000", "prob", "CAPE", "90-0 mb above ground", Time::Instant, "prob >3000", 1, Look::Prob, "Chance of mixed-layer CAPE over 3000 J/kg (%)", false},
                {"mean_vwshr", "Mean 0-6 km Vertical Wind Shear", "mean", "VWSH", "0-6000 m above ground", Time::Instant, "", 1.943844, Look::Shear, "Mean 0-6 km wind shear (kt)", false},
                {"prob_vwshr_30kt", "Probability of 0-6 km Vertical Wind Shear over 30 kt", "prob", "VWSH", "0-6000 m above ground", Time::Instant, "prob >15.4", 1, Look::Prob, "Chance of 0-6 km wind shear over 30 kt (%)", false},
                {"prob_max_hlcy_25", "Probability of Maximum 2-5 km Updraft Helicity over 25", "prob", "MXUPHL", "5000-2000 m above ground", Time::Instant, "prob >25", 1, Look::Prob, "Chance of updraft helicity over 25 (%)", false},
                {"prob_max_hlcy_75", "Probability of Maximum 2-5 km Updraft Helicity over 75", "prob", "MXUPHL", "5000-2000 m above ground", Time::Instant, "prob >75", 1, Look::Prob, "Chance of updraft helicity over 75 (%)", false},
                {"prob_max_hlcy_150", "Probability of Maximum 2-5 km Updraft Helicity over 150", "prob", "MXUPHL", "5000-2000 m above ground", Time::Instant, "prob >150", 1, Look::Prob, "Chance of updraft helicity over 150 (%)", false},
            };
            for (const auto& row : rows) {
                Product x;
                x.source = "HREF";
                x.id = row.id;
                x.label = row.label;
                const std::string kind = row.kind, variable = row.variable, level = row.level, detail = row.detail;
                const Time time = row.time;
                const bool pressureLines = row.pressure;
                x.needs = [kind, variable, level, detail, time, pressureLines] (int hour) {
                    std::vector<GfsData::Need> out;
                    const auto accumulated = [] (int from, int to) { return std::to_string(from) + "-" + std::to_string(to) + " hour acc fcst"; };
                    if (time == Time::Instant) {
                        out.push_back({hour, GfsData::Want{"f", variable, level, std::to_string(hour) + " hour fcst", detail, kind}});
                    } else if (time == Time::OneHour && hour >= 1) {
                        out.push_back({hour, GfsData::Want{"f", variable, level, accumulated(hour - 1, hour), detail, kind}});
                    } else if (time == Time::ThreeHour && hour >= 3) {
                        out.push_back({hour, GfsData::Want{"f", variable, level, accumulated(hour - 3, hour), detail, kind}});
                    } else if (time == Time::Total) {   // the 3 hour amounts back to the start, added up
                        int n = 0;
                        for (int end = hour; end > 0; end -= 3) {
                            out.push_back({end, GfsData::Want{"a" + std::to_string(n++), variable, level, accumulated(std::max(end - 3, 0), end), detail, kind}});
                        }
                    }
                    if (pressureLines && !out.empty()) {
                        out.push_back({hour, GfsData::Want{"p", "MSLET", "mean sea level", std::to_string(hour) + " hour fcst", "", "mean"}});
                    }
                    return out;
                };
                const double scale = row.scale;
                x.derive = [time, scale] (Grids& g, const Context&) {
                    if (time == Time::Total) {
                        auto sum = g["a0"];
                        for (int k = 1; g.count("a" + std::to_string(k)); k++) {
                            const auto& piece = g["a" + std::to_string(k)];
                            for (size_t i = 0; i < sum.values.size(); i++) {
                                sum.values[i] += piece.values[i];
                            }
                        }
                        g["f"] = std::move(sum);
                    }
                    if (scale != 1.0 && g.count("f")) {
                        g["f"] = GfsGrid::scaled(g["f"], scale);
                    }
                };
                x.fill = [] (const Grids& g) { return pick(g, "f"); };
                x.fillTitle = row.title;
                x.legendStep = 0;
                switch (row.look) {
                    case Look::Precip: x.ramp = precipitation(); x.palette = "precip"; x.quantity = Quantity::Millimeters; break;
                    case Look::Snow: x.ramp = snowfall(); x.quantity = Quantity::Centimeters; break;
                    case Look::Radar: x.ramp = reflectivity(); x.palette = "radar_rain"; break;
                    case Look::Prob: x.ramp = probability(); x.palette = "prob"; break;
                    case Look::Temp: x.ramp = temperature(); x.quantity = Quantity::Temperature; x.legendStep = 5; break;
                    case Look::Dew: x.ramp = dewpoint(); x.quantity = Quantity::Temperature; x.legendStep = 5; break;
                    case Look::Pwat: x.ramp = precipitableWater(); x.quantity = Quantity::Millimeters; break;
                    case Look::Cape: x.ramp = capeRamp(); x.legendStep = 500; break;
                    case Look::Vis: x.ramp = visibilityRamp(); x.palette = "vis"; break;
                    case Look::Shear: x.ramp = shearRamp(); x.legendStep = 10; break;
                    case Look::Plain: x.ramp = humidity(); break;
                }
                if (pressureLines) {
                    auto lines = pressure();
                    lines.interval = 2;   // the model guidance site's 2 mb
                    lines.highsAndLows = false;
                    x.contours = {lines};
                }
                p.push_back(x);
            }
            {   // the low instrument flight rules and the instrument flight rules together: the second as the fill, the first as lines at 30, 60 and 90 per cent
                Product x;
                x.source = "HREF";
                x.id = "prob_lowIFR_IFR";
                x.label = "Probability of Low Instrument Flight Rules and Instrument Flight Rules";
                x.needs = [] (int hour) {
                    const auto when = std::to_string(hour) + " hour fcst";
                    return std::vector<GfsData::Need>{{hour, GfsData::Want{"f", "FLGHT", "surface", when, "prob >=2 <3", "prob"}}, {hour, GfsData::Want{"l", "FLGHT", "surface", when, "prob >=1 <2", "prob"}}};
                };
                x.fill = [] (const Grids& g) { return pick(g, "f"); };
                x.ramp = probability();
                x.palette = "prob";
                x.fillTitle = "Chance of IFR (%, fill); lines: chance of low IFR";
                x.legendStep = 0;
                ContourSet low;
                low.key = "l";
                low.base = 30;
                low.minimum = 30;
                low.interval = 30;
                low.title = "Chance of low IFR (%)";
                low.color = QColor{110, 20, 130};
                low.width = 2.0;
                x.contours = {low};
                p.push_back(x);
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
            x.palette = "precip";
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

        // ---- GEPS (Canada): the clone of the GFS charts is the 21 members' mean; its own are the spread of the members and the chance of a temperature either side of a limit
        const auto gepsRecipes = [&] {
            const auto spreadOf = [] (const char * id, const char * label, const char * variable, const char * level, double top, double step, const char * title, Quantity quantity) {
                Product x;
                x.source = "GEPS";
                x.id = id;
                x.label = label;
                x.wants = {GfsData::Want{"s", variable, level, "", "", "spr"}};
                x.fill = [] (const Grids& g) { return pick(g, "s"); };
                x.ramp = spreadRamp(top);
                x.quantity = quantity;
                x.fillTitle = title;
                x.legendStep = step;
                return x;
            };
            p.push_back(spreadOf("spread_2m_temp", "2m Temperature Spread among the members", "TMP", "2 m above ground", 6.0, 1.0, "Spread of the 2 m temperature (C)", Quantity::Other));
            p.push_back(spreadOf("spread_mslp", "Sea Level Pressure Spread among the members", "PRMSL", "mean sea level", 600.0, 100.0, "Spread of the sea level pressure (Pa)", Quantity::Other));
            for (const char * level : {"500", "850"}) {
                const std::string l = std::string{level} + " mb";
                p.push_back(spreadOf((std::string{"spread_"} + level + "_ht").c_str(), (std::string{level} + "mb Height Spread among the members").c_str(), "HGT", l.c_str(), std::string{level} == "500" ? 60.0 : 30.0,
                                     std::string{level} == "500" ? 10.0 : 5.0, ("Spread of the " + std::string{level} + " mb height (m)").c_str(), Quantity::Other));
            }
            // the share of the 21 members at or under / over a temperature, each member its own fetch of the field (the cache keeps them)
            const auto chance = [&] (const char * id, const char * label, double limit, bool under, const char * title) {
                Product x;
                x.source = "GEPS";
                x.id = id;
                x.label = label;
                for (int m = 0; m < 21; m++) {
                    const std::string name = m == 0 ? "c00" : "p" + std::string{m < 10 ? "0" : ""} + std::to_string(m);
                    x.wants.push_back(GfsData::Want{"t" + std::to_string(m), "TMP", "2 m above ground", "", "", name});
                }
                x.fill = [limit, under] (const Grids& g) {
                    auto out = pick(g, "t0");
                    for (size_t i = 0; i < out.values.size(); i++) {
                        int n = 0, hit = 0;
                        for (int m = 0; m < 21; m++) {
                            const float v = g.at("t" + std::to_string(m)).values[i];
                            if (!std::isnan(v)) {
                                n++;
                                hit += (under ? v <= limit : v >= limit) ? 1 : 0;
                            }
                        }
                        out.values[i] = n > 0 ? static_cast<float>(100.0 * hit / n) : std::nanf("");
                    }
                    return out;
                };
                x.ramp = probability();
                x.palette = "prob";
                x.fillTitle = title;
                x.legendStep = 10;
                p.push_back(x);
            };
            chance("prob_2m_temp_freezing", "Probability of 2m Temperature at or under Freezing", 0.0, true, "Chance of 0 C or colder (% of the 21 members)");
            chance("prob_2m_temp_90f", "Probability of 2m Temperature at or over 90 F", (90.0 - 32.0) * 5.0 / 9.0, false, "Chance of 90 F or hotter (% of the 21 members)");
            {   // the chance of an amount of precipitation: each member's running total since the start of the run (the file of an hour holds the total up to it)
                std::vector<std::string> members;
                for (int m = 0; m < 21; m++) {
                    members.push_back(m == 0 ? "c00" : "p" + std::string{m < 10 ? "0" : ""} + std::to_string(m));
                }
                const CumulativeEnsemble geps{"GEPS", members, [] (int) { return std::string{"0-*"}; }};
                for (const auto& [id, inches, text] : {std::tuple{"prob_total_0.01in", 0.01, "0.01"}, {"prob_total_0.1in", 0.1, "0.10"}, {"prob_total_0.25in", 0.25, "0.25"}, {"prob_total_0.5in", 0.5, "0.50"},
                                                       {"prob_total_1in", 1.0, "1.00"}, {"prob_total_2in", 2.0, "2.00"}}) {
                    auto x = exceedProduct(geps, id, std::string{"Probability of total precipitation of "} + text + " in or more (since the start of the run)", inches, 0);
                    x.fillTitle = std::string{"Chance of "} + text + " in or more of precipitation in total since the start of the run (% of the 21 members)";
                    p.push_back(x);
                }
                auto& made = generators();
                made["geps_total"] = [geps] (const std::vector<double>& v) {
                    const double inches = v.at(0);
                    auto x = exceedProduct(geps, generatedId("geps_total", {inches}), "Chance of " + trimmed(inches) + " in or more of precipitation in total (since the start of the run)", inches, 0);
                    x.fillTitle = "Chance of " + trimmed(inches) + " in or more of precipitation in total since the start of the run (% of the 21 members)";
                    return x;
                };
                made["geps_period"] = [geps] (const std::vector<double>& v) {
                    const double inches = v.at(0);
                    const int hours = std::max(3, static_cast<int>(std::lround(v.at(1) / 3.0)) * 3);
                    auto x = exceedProduct(geps, generatedId("geps_period", {inches, static_cast<double>(hours)}), "Chance of " + trimmed(inches) + " in or more of precipitation in " + std::to_string(hours) + " hours", inches, hours);
                    x.fillTitleFor = [inches, hours] (int hour) {
                        return "Chance of " + trimmed(inches) + " in or more of precipitation in the " + std::to_string(std::min(hours, hour)) + " hours ending at hour " + std::to_string(hour) + " (% of the 21 members)";
                    };
                    return x;
                };
            }
        };

        // ---- SPC's post-processing (SPC_POST v3, effective Nov 3 2026; in the test area before): probabilities in per cent, one product per file, a variable per message
        const auto spcRecipes = [&] (const char * source, bool gefs) {
            const auto add = [&] (const std::string& id, const std::string& label, const std::string& file, const char * stat, const std::string& title) {
                Product x;
                x.source = source;
                x.id = id;
                x.label = label;
                x.wants = {GfsData::Want{"p", "PROB", file, "", "", stat}};
                x.fill = [] (const Grids& g) { return pick(g, "p"); };
                x.ramp = probability();
                x.palette = "prob";
                x.fillTitle = title;
                x.legendStep = 10;
                p.push_back(x);
            };
            if (!gefs) {
                for (const auto& [id, period, text] : {std::tuple<const char *, const char *, const char *>{"1hr", "refsct_1hr", "1-hour"}, {"4hr", "refsct_4hr", "4-hour"}, {"full", "refsct_full", "rest of the convective day"}}) {
                    add(std::string{"thunder_"} + id, std::string{"REFSCT Thunderstorm Probability ("} + text + ")", std::string{"refsct/"} + period, "", std::string{"Chance of a thunderstorm, "} + text + " (%)");
                }
                const std::pair<const char *, const char *> kinds[] = {{"tor", "Tornado"}, {"hail", "Hail"}, {"wind", "Wind"}};
                for (int k = 0; k < 3; k++) {
                    const std::string band = "b" + std::to_string(k + 1);
                    add(std::string{"severe_"} + kinds[k].first + "_4hr", std::string{"GREFS Calibrated "} + kinds[k].second + " Probability (4-hour)", "grefs/grefs_{ee}.4hr;grefs/grefs_{pe}.4hr", band.c_str(),
                        std::string{"Chance of "} + kinds[k].first + " severe within 25 miles, 4 hours (%)");
                    add(std::string{"severe_"} + kinds[k].first + "_full", std::string{"GREFS Calibrated "} + kinds[k].second + " Probability (rest of the convective day)", "grefs/grefs_{ee}.*hr;grefs/grefs_{pe}.*hr", band.c_str(),
                        std::string{"Chance of "} + kinds[k].first + " severe within 25 miles, rest of the convective day (%)");
                }
            } else {
                add("thunder_24hr", "GEFSCT Thunderstorm Probability (24-hour, 12Z to 12Z)", "gefsct/gefsct", "", "Chance of a thunderstorm in the convective day (%)");
                for (const auto& [what, label] : {std::pair<const char *, const char *>{"tor", "Tornado"}, {"wind", "Wind"}, {"hail", "Hail"}, {"severe", "Any Severe"}}) {
                    for (const auto& [level, text] : {std::pair<const char *, const char *>{"std", "(Any Severe)"}, {"sig", "(Significant)"}}) {
                        add(std::string{"gefsmlp_"} + what + "_" + level, std::string{"GEFS-MLP "} + label + " Probability " + text, std::string{"gefs_mlp/gefsmlp_"} + what + "_" + level, "",
                            std::string{"Chance of "} + what + " severe within 25 miles (%)");
                    }
                }
            }
        };

        // ---- the mesoanalysis fields (SFCOA-HRRR, and the RRFS's RRFSD): the NetCDF variable names and units are the file's
        const auto mesoRecipes = [&] (const char * source) {
            struct Spec {
                const char * id;
                const char * label;
                const char * variable;   // "name" or "u_name|v_name" for a vector drawn as its length
                double scale;
                double offset;           // value * scale + offset = the chart's unit
                Quantity quantity;
                const char * title;
                double step;
            };
            const Ramp cin{{{-300.0, QColor{"#4b1f6f"}}, {-200.0, QColor{"#6c3fa0"}}, {-100.0, QColor{"#4a78c8"}}, {-50.0, QColor{"#7fb8e6"}}, {-25.0, QColor{"#bfe4f2"}}, {-5.0, QColor{191, 228, 242, 0}}, {0.0, QColor{191, 228, 242, 0}}}};
            const Ramp index{{{0.0, QColor{0, 0, 40, 0}}, {0.5, QColor{0, 0, 40, 0}}, {1.0, QColor{60, 170, 90}}, {2.0, QColor{230, 220, 60}}, {4.0, QColor{240, 150, 40}}, {8.0, QColor{220, 40, 40}},
                              {12.0, QColor{170, 30, 150}}, {20.0, QColor{255, 255, 255}}}};
            const Ramp stp = Ramp{{{0.0, QColor{0, 0, 40, 0}}, {0.25, QColor{0, 0, 40, 0}}, {0.5, QColor{40, 60, 200}}, {1.0, QColor{60, 170, 90}}, {2.0, QColor{230, 220, 60}}, {3.0, QColor{240, 150, 40}},
                                   {5.0, QColor{220, 40, 40}}, {8.0, QColor{170, 30, 150}}, {12.0, QColor{255, 255, 255}}}};
            const Ramp lapse{{{4.0, QColor{"#2f5aa8"}}, {5.5, QColor{"#7fb8e6"}}, {6.5, QColor{"#e8e0a0"}}, {7.5, QColor{"#f0a63a"}}, {8.5, QColor{"#d63a2a"}}, {10.0, QColor{"#7a1a6a"}}}};
            const Ramp height{{{0.0, QColor{"#2f5aa8"}}, {500.0, QColor{"#7fb8e6"}}, {1000.0, QColor{"#bfe4cf"}}, {1500.0, QColor{"#f1e27a"}}, {2000.0, QColor{"#f0a63a"}}, {3000.0, QColor{"#d63a2a"}}, {4500.0, QColor{"#7a1a6a"}}}};
            const Ramp angle{{{0.0, QColor{"#7a1a6a"}}, {30.0, QColor{"#d63a2a"}}, {60.0, QColor{"#f0a63a"}}, {90.0, QColor{"#7fcf7f"}}, {120.0, QColor{"#7fb8e6"}}, {180.0, QColor{"#2f5aa8"}}}};
            const Ramp mixing{{{0.0, QColor{"#a98a5a"}}, {4.0, QColor{"#dcd29a"}}, {8.0, QColor{"#a9d98a"}}, {12.0, QColor{"#4fb98a"}}, {16.0, QColor{"#2f8aa6"}}, {20.0, QColor{"#3a55b8"}}}};
            const std::vector<std::pair<Spec, Ramp>> specs{
                {{"sbcape", "Surface-Based CAPE", "sb_cape", 1.0, 0.0, Quantity::Other, "Surface-based CAPE (J/kg)", 500}, capeRamp()},
                {{"mlcape", "Mixed-Layer CAPE (lowest 100 mb)", "ml_cape", 1.0, 0.0, Quantity::Other, "Mixed-layer CAPE (J/kg)", 500}, capeRamp()},
                {{"mucape", "Most Unstable CAPE", "mu_cape", 1.0, 0.0, Quantity::Other, "Most unstable CAPE (J/kg)", 500}, capeRamp()},
                {{"sbcin", "Surface-Based CIN", "sb_cinh", 1.0, 0.0, Quantity::Other, "Surface-based CIN (J/kg)", 50}, cin},
                {{"mlcin", "Mixed-Layer CIN", "ml_cinh", 1.0, 0.0, Quantity::Other, "Mixed-layer CIN (J/kg)", 50}, cin},
                {{"mucin", "Most Unstable CIN", "mu_cinh", 1.0, 0.0, Quantity::Other, "Most unstable CIN (J/kg)", 50}, cin},
                {{"dcape", "Downdraft CAPE", "dcape", 1.0, 0.0, Quantity::Other, "Downdraft CAPE (J/kg)", 500}, capeRamp()},
                {{"srh_0_1km", "0-1 km Storm Relative Helicity", "srh_0_1km", 1.0, 0.0, Quantity::Other, "0-1 km storm relative helicity (m2/s2)", 0}, helicityRamp()},
                {{"srh_0_3km", "0-3 km Storm Relative Helicity", "srh_0_3km", 1.0, 0.0, Quantity::Other, "0-3 km storm relative helicity (m2/s2)", 0}, helicityRamp()},
                {{"srh_eff", "Effective Storm Relative Helicity", "srh_eff", 1.0, 0.0, Quantity::Other, "Effective storm relative helicity (m2/s2)", 0}, helicityRamp()},
                {{"stp_eff", "Significant Tornado Parameter (effective layer)", "stp_eff", 1.0, 0.0, Quantity::Other, "STP, effective layer: over 1, most significant tornadoes with a supercell", 1}, stp},
                {{"stp_fix", "Significant Tornado Parameter (fixed layer)", "stp_fix", 1.0, 0.0, Quantity::Other, "STP, fixed layer: over 1, most significant tornadoes with a supercell", 1}, stp},
                {{"scp", "Supercell Composite Parameter", "scp", 1.0, 0.0, Quantity::Other, "Supercell composite parameter (right movers)", 2}, index},
                {{"ship", "Significant Hail Parameter", "shp", 1.0, 0.0, Quantity::Other, "Significant hail parameter: over 1, significant hail", 1}, stp},
                {{"ehi_0_1km", "0-1 km Energy Helicity Index", "ehi_0_1km", 1.0, 0.0, Quantity::Other, "Energy helicity index, 0-1 km", 1}, index},
                {{"ehi_0_3km", "0-3 km Energy Helicity Index", "ehi_0_3km", 1.0, 0.0, Quantity::Other, "Energy helicity index, 0-3 km", 1}, index},
                {{"dcp", "Derecho Composite Parameter", "dcp", 1.0, 0.0, Quantity::Other, "Derecho composite parameter", 1}, index},
                {{"shear_0_6km", "0-6 km Bulk Wind Shear", "shear_0_6km_u|shear_0_6km_v", 1.943844, 0.0, Quantity::Other, "0-6 km bulk shear (kt)", 10}, shearRamp()},
                {{"shear_0_1km", "0-1 km Bulk Wind Shear", "shear_0_1km_u|shear_0_1km_v", 1.943844, 0.0, Quantity::Other, "0-1 km bulk shear (kt)", 10}, shearRamp()},
                {{"effective_shear", "Effective Bulk Wind Difference", "effective_shear_u|effective_shear_v", 1.943844, 0.0, Quantity::Other, "Effective bulk wind difference (kt)", 10}, shearRamp()},
                {{"mean_wind_0_6km", "0-6 km Mean Wind", "mean_wind_0_6km_u|mean_wind_0_6km_v", 1.943844, 0.0, Quantity::Other, "0-6 km mean wind (kt)", 10}, windSpeed()},
                {{"lapse_700_500", "700-500 mb Lapse Rate", "lapse_rate_700_500hPa", 1.0, 0.0, Quantity::Other, "700-500 mb lapse rate (K/km)", 1}, lapse},
                {{"lapse_0_3km", "0-3 km Lapse Rate", "lapse_rate_0_3km", 1.0, 0.0, Quantity::Other, "0-3 km lapse rate (K/km)", 1}, lapse},
                {{"temp_2m", "2 m Temperature", "tmpk_2m", 1.0, -273.15, Quantity::Temperature, "2 m temperature", 5}, temperature()},
                {{"dewpoint_2m", "2 m Dewpoint", "dwpk_2m", 1.0, -273.15, Quantity::Temperature, "2 m dewpoint", 5}, dewpoint()},
                {{"rh_2m", "2 m Relative Humidity", "relh_2m", 1.0, 0.0, Quantity::Other, "2 m relative humidity (%)", 10}, humidity()},
                {{"mixr_2m", "2 m Mixing Ratio", "mixr_2m", 1000.0, 0.0, Quantity::Other, "2 m water vapor mixing ratio (g/kg)", 2}, mixing},
                {{"thetae_2m", "2 m Equivalent Potential Temperature", "thetae_2m", 1.0, 0.0, Quantity::Other, "2 m equivalent potential temperature (K)", 10}, thetaERamp()},
                {{"pwat", "Precipitable Water", "pwat", 1.0, 0.0, Quantity::Other, "Precipitable water (mm)", 10}, precipitableWater()},
                {{"lcl_ml", "Mixed-Layer LCL Height", "ml_lcl_hght_agl", 1.0, 0.0, Quantity::Other, "Mixed-layer LCL height above ground (m)", 500}, height},
                {{"lfc_sb", "Surface-Based LFC Height", "sb_lfc_hght_agl", 1.0, 0.0, Quantity::Other, "Surface-based LFC height above ground (m)", 1000}, height},
                {{"fzl", "Freezing Level Height", "fzl_hght_agl", 1.0, 0.0, Quantity::Other, "Freezing level above ground (m)", 500}, height},
                {{"critical_angle", "Critical Angle", "critical_angle", 1.0, 0.0, Quantity::Other, "Critical angle (degrees; near 90 favors low level rotation)", 30}, angle},
            };
            for (const auto& [spec, ramp] : specs) {
                Product x;
                x.source = source;
                x.id = spec.id;
                x.label = spec.label;
                const std::string variable = spec.variable;
                const auto bar = variable.find('|');
                x.wants = {GfsData::Want{"a", variable.substr(0, bar), "", "", "", ""}};
                if (bar != std::string::npos) {
                    x.wants.push_back(GfsData::Want{"b", variable.substr(bar + 1), "", "", "", ""});
                }
                const double scale = spec.scale, offset = spec.offset;
                const bool vector = bar != std::string::npos;
                x.fill = [scale, offset, vector] (const Grids& g) {
                    auto out = pick(g, "a");
                    const GfsGrid::Grid * second = vector ? &g.at("b") : nullptr;
                    for (size_t i = 0; i < out.values.size(); i++) {
                        const double v = vector ? std::hypot(static_cast<double>(out.values[i]), static_cast<double>(second->values[i])) : static_cast<double>(out.values[i]);
                        out.values[i] = std::isfinite(v) ? static_cast<float>(v * scale + offset) : std::nanf("");
                    }
                    return out;
                };
                x.ramp = ramp;
                x.quantity = spec.quantity;
                x.fillTitle = spec.title;
                x.legendStep = spec.step;
                p.push_back(x);
            }
        };

        // ---- SPC's REFS products (the open Zarr store): the curated list of SPC's own viewer, each as the colors and units it has there
        // ---- the ECMWF's extended range forecast (EC46): the weekly mean of the ensemble, from the loader of Open-Meteo's seasonal data; the "hour" of a chart is a week (168 hours each)
        const auto openMeteoRecipes = [&] (const std::string& source) {
            const bool months = source == "SEAS5";
            const std::string prefix = months ? "seas5_" : "ec46_", period = months ? "month" : "week";
            const int hoursEach = months ? 720 : 168;
            struct Spec {
                const char * id;
                const char * label;
                const char * variable;
                Ramp ramp;
                Quantity quantity;
                const char * title;
                double legendStep;
            };
            const std::vector<Spec> specs{
                {"t2m_departure", "2 m Temperature: departure of the week", "temperature_2m_anomaly", temperatureDeparture(), Quantity::TemperatureDifference, "2 m temperature, the week's mean against the ECMWF climate", 2.0},
                {"precip_departure", "Precipitation: departure of the week", "precipitation_anomaly", precipitationDeparture(), Quantity::Millimeters, "Precipitation of the week against the ECMWF climate", 0.0},
                {"mslp_departure", "Sea Level Pressure: departure of the week", "pressure_msl_anomaly", pressureAnomaly(), Quantity::Other, "Sea level pressure, the week's mean against the ECMWF climate (mb)", 5.0},
                {"t2m", "2 m Temperature: mean of the week", "temperature_2m_mean", temperature(), Quantity::Temperature, "2 m temperature, the week's mean", 5.0},
                {"precip", "Precipitation: total of the week", "precipitation_mean", precipitation(), Quantity::Millimeters, "Precipitation of the week", 0.0},
                {"mslp", "Sea Level Pressure: mean of the week", "pressure_msl_mean", seaLevelPressure(), Quantity::Other, "Sea level pressure, the week's mean (mb)", 5.0},
            };
            // a month's departures are smaller than a week's in temperature and pressure and larger in the rain: the scales stretch to match
            const auto stretched = [] (Ramp ramp, double factor) {
                for (auto& stop : ramp.stops) {
                    stop.first *= factor;
                }
                return ramp;
            };
            for (const auto& spec : specs) {
                Product x;
                x.source = source;
                x.id = prefix + spec.id;
                x.label = months ? std::regex_replace(std::string{spec.label}, std::regex{"week"}, "month") : std::string{spec.label};
                x.wants = {want("v", spec.variable, "weekly")};
                x.fill = [] (const Grids& g) { return pick(g, "v"); };
                const std::string id = spec.id;
                x.ramp = months ? stretched(spec.ramp, id == "t2m_departure" ? 0.4 : id == "mslp_departure" ? 0.5 : id == "precip_departure" ? 2.0 : id == "precip" ? 2.5 : 1.0) : spec.ramp;
                x.quantity = spec.quantity;
                const std::string title = spec.title;
                x.fillTitleFor = [title, period, hoursEach, months] (int hour) {
                    std::string text = title;
                    if (months) {
                        for (const char * word : {"the week's", "of the week", "the week"}) {   // (the titles speak of a week)
                            for (auto at = text.find(word); at != std::string::npos; at = text.find(word, at + 1)) {
                                const std::string w{word};
                                text.replace(at, w.size(), w == "the week's" ? "the month's" : w == "of the week" ? "of the month" : "the month");
                            }
                        }
                    }
                    return text + ": " + period + " " + std::to_string(hour / hoursEach + 1) + " of the forecast (" + (months ? "calendar months, the first is the month of the run)" : "Monday to Sunday, the first is the week of the run)");
                };
                x.legendStep = spec.legendStep;
                p.push_back(x);
            }
        };

        // ---- the CFSv2's next 30 days against its normal (the loader of this source reads the CFS): temperature in colors with the precipitation as lines, and the precipitation itself
        const auto cfs30Recipes = [&] {
            const auto stretched = [] (Ramp ramp, double factor) {
                for (auto& stop : ramp.stops) {
                    stop.first *= factor;
                }
                return ramp;
            };
            {
                Product x;
                x.source = "CFS30";
                x.id = "cfs30_t2m_precip";
                x.label = "Next days: temperature and precipitation against the normal";
                x.wants = {want("v", "t2m", "30 days"), want("p", "precip", "30 days")};
                x.fill = [] (const Grids& g) { return pick(g, "v"); };
                x.ramp = stretched(temperatureDeparture(), 0.5);   // (a 30 day mean departs less than a week's)
                x.quantity = Quantity::TemperatureDifference;
                x.fillTitleFor = [] (int hour) { return "CFSv2, next " + std::to_string(hour / 24) + " days vs its normal: temperature"; };
                x.legendStep = 1.0;
                ContourSet lines;
                lines.key = "p";
                lines.interval = 0.5;
                lines.title = "precipitation, mm/day (green wetter, brown drier)";
                lines.color = QColor{20, 120, 60};
                lines.colorBelow = QColor{140, 80, 20};
                lines.split = 0.0;
                lines.smoothKm = 250.0;
                lines.width = 1.2;
                lines.uniform = true;
                x.contours = {lines};
                p.push_back(x);
            }
            {
                Product x;
                x.source = "CFS30";
                x.id = "cfs30_precip";
                x.label = "Next days: precipitation against the normal";
                x.wants = {want("v", "precip", "30 days")};
                x.fill = [] (const Grids& g) { return pick(g, "v"); };
                x.ramp = stretched(precipitationDeparture(), 0.04);   // (millimeters a day: 2.4 is a week's 60 mm over 25 days)
                x.quantity = Quantity::Millimeters;
                x.fillTitleFor = [] (int hour) { return "CFSv2, next " + std::to_string(hour / 24) + " days vs its normal: precipitation a day"; };
                x.legendStep = 0.0;
                p.push_back(x);
            }
            {
                Product x;
                x.source = "CFS30";
                x.id = "cfs30_t2m";
                x.label = "Next days: temperature against the normal";
                x.wants = {want("v", "t2m", "30 days")};
                x.fill = [] (const Grids& g) { return pick(g, "v"); };
                x.ramp = stretched(temperatureDeparture(), 0.5);
                x.quantity = Quantity::TemperatureDifference;
                x.fillTitleFor = [] (int hour) { return "CFSv2, next " + std::to_string(hour / 24) + " days vs its normal: 2 m temperature"; };
                x.legendStep = 1.0;
                p.push_back(x);
            }
        };

        const auto spcRefsRecipes = [&] {
            // SPC's own colors for the paintball of each member, in the store's order: HRRR, RRFS control, REFS M01, M02, M03, M05, then HRRR and RRFS control 6 and 12 hours older
            static const unsigned memberColors[] = {0x4c4c4c, 0x007399, 0x009985, 0x009947, 0x009909, 0x469900, 0x7f7f7f, 0x4cd3ff, 0xb3b3b3, 0xb3ecff};
            // the lines SPC's viewer draws over a chart: the neighborhood chances of the fields (10, 30, 50, 70 and 90 per cent) and the freezing and 60 F lines of the mean temperature
            using Lines = std::vector<std::pair<std::string, std::string>>;   // the array and what its lines say
            static const std::map<std::string, Lines> chances = [] {
                std::map<std::string, Lines> out;
                const auto both = [&out] (const std::vector<std::string>& charts, const Lines& lines) {
                    for (const auto& id : charts) {
                        out[id] = lines;
                    }
                };
                both({"cref_pb_t040"}, {{"cref_nmep_r013_t040", "40 dBZ chance"}});
                for (const std::string h : {"04h_max_overlap", "24h_max_overlap12h"}) {
                    const std::string n = h == "04h_max_overlap" ? "04h" : "24h";
                    both({"maxref1km_" + h + "_max", "maxref1km_mucape_" + h + "_pb_t>040"}, {{"maxref1km_mucape_" + h + "_nmep_r013_t>040", "40 dBZ chance"}});
                    both({"uh25_" + h + "_max"}, {{"uh25_" + h + "_nmep_r013_t>99.85p", "UH > 99.85 percentile chance"}, {"uh25_" + h + "_nmep_r013_t>99.95p", "UH > 99.95 percentile chance"}});
                    both({"uh25_" + h + "_pb_t>99.85p"}, {{"uh25_" + h + "_nmep_r013_t>99.85p", "chance"}});
                    both({"uh25_" + h + "_pb_t>99.95p"}, {{"uh25_" + h + "_nmep_r013_t>99.95p", "chance"}});
                    both({"wmax_" + h + "_max"}, {{"wmax_" + h + "_nmep_r013_t020", "20 m/s chance"}, {"wmax_" + h + "_nmep_r013_t030", "30 m/s chance"}});
                    both({"wmax_" + h + "_pb_t020"}, {{"wmax_" + h + "_nmep_r013_t020", "chance"}});
                    both({"s10m_" + h + "_max"}, {{"s10m_" + h + "_nmep_r013_t030", "30 kt chance"}, {"s10m_" + h + "_nmep_r013_t050", "50 kt chance"}});
                    both({"s10m_" + h + "_pb_t030"}, {{"s10m_" + h + "_nmep_r013_t030", "chance"}});
                    (void) n;
                }
                both({"uh25min_04h_min_overlap_min"}, {{"uh25min_04h_min_overlap_nmep_r013_t<-35", "UH < -35 chance"}, {"uh25min_04h_min_overlap_nmep_r013_t<-70", "UH < -70 chance"}});
                both({"uh25min_24h_min_overlap12h_min"}, {{"uh25min_24h_min_overlap12h_nmep_r013_t<-35", "UH < -35 chance"}, {"uh25min_24h_min_overlap12h_nmep_r013_t<-70", "UH < -70 chance"}});
                for (const char * id : {"qpf_03h_acc_pmmean", "qpf_03h_acc_mean", "qpf_03h_acc_max"}) {
                    out[id] = {{"qpf_03h_acc_nmep_r013_t>001", "1 in chance"}, {"qpf_03h_acc_nmep_r013_t>003", "3 in chance"}};
                }
                out["qpf_06h_acc_mean"] = {{"qpf_06h_acc_nmep_r013_t>0p100", "0.1 in chance"}, {"qpf_06h_acc_nmep_r013_t>0p250", "0.25 in chance"}};
                return out;
            }();
            const auto addChances = [] (Product& x, const std::string& array, const std::string& title, const QColor& color) {
                const std::string key = "n" + std::to_string(x.wants.size());
                x.wants.push_back(GfsData::Want{key, array, "", "", "", ""});
                ContourSet c;
                c.key = key;
                c.interval = 20.0;   // (the levels are in the labelled unit: per cent)
                c.base = 10.0;
                c.minimum = 9.0;
                c.scale = 100.0;
                c.title = title + " (%)";
                c.color = color;
                c.uniform = true;
                c.width = 1.4;
                x.contours.push_back(c);
            };
            const auto addTemperatureLine = [] (Product& x, double level, const QColor& color) {
                if (std::none_of(x.wants.begin(), x.wants.end(), [] (const GfsData::Want& w) { return w.key == "t2"; })) {
                    x.wants.push_back(GfsData::Want{"t2", "t2m_mean", "", "", "", ""});
                }
                ContourSet c;
                c.key = "t2";
                c.base = level;
                c.interval = 1000.0;   // one line: this level (and not the ones a thousand degrees away)
                c.minimum = level - 1.0;
                c.title = "2-m temperature (ensemble mean, F)";
                c.color = color;
                c.uniform = true;
                c.width = 2.0;
                x.contours.push_back(c);
            };
            const auto decorate = [&] (Product& x, const UtilitySpcRefs::Product& source) {
                const auto found = chances.find(source.id);
                if (found != chances.end()) {
                    const QColor colors[] = {QColor{0, 0, 0}, QColor{170, 0, 0}};
                    for (size_t i = 0; i < found->second.size(); i++) {
                        addChances(x, found->second[i].first, found->second[i].second, colors[i % 2]);
                    }
                }
                const auto starts = [&source] (const char * prefix) { return source.id.rfind(prefix, 0) == 0; };
                if (starts("fosberg_") || starts("rh2m_s10m_prob_") || starts("gust_") || starts("s10m_01h_max_") || starts("bliz_")) {
                    addTemperatureLine(x, 32.0, QColor{"#0000c4"});
                    if (!starts("bliz_")) {
                        addTemperatureLine(x, 60.0, QColor{"#bb271a"});
                    }
                }
            };
            for (const auto& source : UtilitySpcRefs::allProducts()) {   // every field of the store, as the SPC REFS screen had them
                Product x;
                x.source = "SPCREFS";
                x.id = source.id;
                x.label = source.label;
                x.pickerGroup = "SPC REFS: " + source.group;
                x.wants = {GfsData::Want{"a", source.id, "", "", "", source.members ? "m001" : ""}};
                decorate(x, source);
                if (source.kind == UtilitySpcRefs::Kind::Paintball) {   // bit i of the mask: member i is over the limit
                    x.derive = [] (Grids& g, const Context&) {
                        const auto mask = g.at("a");
                        for (int m = 0; m < 10; m++) {
                            auto bit = mask;
                            for (auto& v : bit.values) {
                                v = std::isnan(v) ? v : ((static_cast<long long>(v) >> m) & 1LL) ? 1.0f : 0.0f;
                            }
                            g["Member " + std::to_string(m + 1)] = std::move(bit);
                        }
                    };
                    const auto solid = [] (unsigned rgb) {
                        QColor c{static_cast<QRgb>(rgb)};
                        c.setAlpha(170);
                        return Ramp{{{0.0, QColor{255, 255, 255, 0}}, {0.49, QColor{255, 255, 255, 0}}, {0.5, c}, {1.0, c}}};
                    };
                    x.fill = [] (const Grids& g) { return pick(g, "Member 1"); };
                    x.ramp = solid(memberColors[0]);
                    for (int m = 1; m < 10; m++) {
                        x.overlays.push_back({"Member " + std::to_string(m + 1), solid(memberColors[m])});
                    }
                    x.fillTitle = source.label + ": each member over the limit in its own color";
                    x.legendStep = 0;
                    p.push_back(x);
                    continue;
                }
                const double factor = source.factor, offset = source.offset;
                x.fill = [factor, offset] (const Grids& g) {
                    auto out = pick(g, "a");
                    for (auto& v : out.values) {
                        v = std::isnan(v) ? v : static_cast<float>(v * factor + offset);
                    }
                    return out;
                };
                Ramp ramp;
                for (const auto& stop : source.stops) {
                    ramp.stops.emplace_back(stop.value, QColor{stop.r, stop.g, stop.b});
                }
                if (ramp.stops.empty()) {   // no table of its own: SPC's rainbow over a usual range
                    ramp.stops = {{0.0, QColor{"#3b4cc0"}}, {50.0, QColor{"#e6e61e"}}, {100.0, QColor{"#8c0a0a"}}};
                }
                ramp.banded = source.kind == UtilitySpcRefs::Kind::Banded;
                if (!ramp.banded && source.hideBelow > -1e29 && source.hideBelow > ramp.stops.front().first) {   // a smooth scale with nothing drawn below a value
                    const auto at = QColor::fromRgba(ramp.at(source.hideBelow));
                    Ramp cut;
                    cut.stops.emplace_back(source.hideBelow - 1e-6, QColor{at.red(), at.green(), at.blue(), 0});
                    cut.stops.emplace_back(source.hideBelow, at);
                    for (const auto& stop : ramp.stops) {
                        if (stop.first > source.hideBelow) {
                            cut.stops.push_back(stop);
                        }
                    }
                    ramp = cut;
                }
                x.ramp = ramp;
                x.fillTitle = source.label + (source.units.empty() ? "" : " (" + source.units + ")");
                x.legendStep = 0;
                p.push_back(x);
            }
            {   // the 1-hour rain, snow, freezing rain and mix amounts of the ensemble mean, each in its own colors by the ensemble's dominant precipitation type (the viewer's own bands), with the freezing line
                Product x;
                x.source = "SPCREFS";
                x.id = "qpf_01h_ptype";
                x.label = "1-h QPF and Dominant Precipitation Type";
                x.pickerGroup = "SPC REFS: Precipitation";
                x.wants = {GfsData::Want{"a", "qpf_01h_acc_mean", "", "", "", ""}, GfsData::Want{"p", "ptype_consensus", "", "", "", ""}};
                addTemperatureLine(x, 32.0, QColor{"#0000c4"});
                x.derive = [] (Grids& g, const Context&) {   // the type of each point: 1 rain, 2 snow, 3 freezing rain, 4 mix (0 none)
                    const auto kinds = {std::pair{1, "rain"}, std::pair{2, "snow"}, std::pair{3, "freezing"}, std::pair{4, "mix"}};
                    for (const auto& [code, name] : kinds) {
                        auto out = g.at("a");
                        const auto& type = g.at("p");
                        for (size_t i = 0; i < out.values.size(); i++) {
                            const float t = i < type.values.size() ? type.values[i] : std::nanf("");
                            if (std::isnan(t) || static_cast<int>(std::lround(t)) != code) {
                                out.values[i] = std::nanf("");
                            }
                        }
                        g[name] = std::move(out);
                    }
                };
                const auto bands = [] (double a, double b, double c, double d, const char * c1, const char * c2, const char * c3, const char * c4) {
                    Ramp r;
                    r.banded = true;
                    r.stops = {{a, QColor{c1}}, {b, QColor{c2}}, {c, QColor{c3}}, {d, QColor{c4}}};
                    return r;
                };
                x.fill = [] (const Grids& g) { return pick(g, "rain"); };
                x.ramp = bands(0.01, 0.1, 0.25, 1.0, "#a0fc4e", "#73c139", "#478625", "#1e4b10");
                x.overlays = {{"snow", bands(0.01, 0.05, 0.1, 0.2, "#75fbfd", "#4ba7f8", "#2254f5", "#0000f5")},
                              {"freezing", bands(0.01, 0.05, 0.1, 0.2, "#ea3729", "#b92c1f", "#892214", "#581b08")},
                              {"mix", bands(0.01, 0.05, 0.1, 0.2, "#de9def", "#be6bcb", "#9e3caa", "#801787")}};
                x.fillTitle = "1-h QPF (in) by dominant type: green rain, blue snow, red freezing rain, purple mix";
                x.legendStep = 0;
                p.push_back(x);
            }
        };

        // ---- IFS-ENS (ECMWF's ensemble, 50 members): the spread of a field with its mean as lines, a spaghetti of one contour from every member, and a single member by the member picker
        const auto ensRecipes = [&] {
            static const std::vector<std::string> names = [] {
                std::vector<std::string> out;
                for (int m = 1; m <= 50; m++) {
                    out.push_back((m < 10 ? "e0" : "e") + std::to_string(m));
                }
                return out;
            }();
            const auto make = [] (const std::string& id, const std::string& label) {
                Product x;
                x.source = "IFS-ENS";
                x.id = id;
                x.label = label;
                return x;
            };
            // the same field from each member: keys "<prefix>e01" ...
            const auto memberNeeds = [] (const std::string& prefix, const char * variable, const char * level, int hour, const std::string& forecast) {
                std::vector<GfsData::Need> out;
                for (const auto& name : names) {
                    out.push_back({hour, GfsData::Want{prefix + name, variable, level, forecast, "", name}});
                }
                return out;
            };
            // the mean and the spread (standard deviation) of the members' field: "<prefix>m" and "<prefix>s", in `scale` of the file's unit; the members are kept when `keep`
            const auto statistics = [] (Grids& g, const std::string& prefix, double scale, bool keep) {
                auto mean = g.at(prefix + names.front());
                auto spread = mean;
                for (size_t i = 0; i < mean.values.size(); i++) {
                    double sum = 0.0, square = 0.0;
                    int n = 0;
                    for (const auto& name : names) {
                        const float v = g.at(prefix + name).values[i];
                        if (std::isfinite(v)) {
                            sum += v;
                            square += static_cast<double>(v) * v;
                            n++;
                        }
                    }
                    mean.values[i] = n > 0 ? static_cast<float>(sum / n * scale) : std::nanf("");
                    spread.values[i] = n > 1 ? static_cast<float>(std::sqrt(std::max(0.0, square / n - (sum / n) * (sum / n))) * scale) : std::nanf("");
                }
                g[prefix + "m"] = std::move(mean);
                g[prefix + "s"] = std::move(spread);
                if (!keep) {
                    for (const auto& name : names) {
                        g.erase(prefix + name);
                    }
                }
            };
            // spread as the fill, the mean as lines
            const auto spreadChart = [&] (const char * id, const char * label, const char * variable, const char * level, double scale, double top, const char * title, ContourSet lines) {
                auto x = make(id, label);
                x.needs = [memberNeeds, variable, level] (int hour) { return memberNeeds("f", variable, level, hour, hour == 0 ? std::string{"0 hour fcst"} : std::to_string(hour) + " hour fcst"); };
                x.derive = [statistics, scale] (Grids& g, const Context&) { statistics(g, "f", scale, false); };
                x.fill = [] (const Grids& g) { return pick(g, "fs"); };
                x.ramp = spreadRamp(top);
                x.fillTitle = title;
                x.legendStep = 0;
                lines.key = "fm";
                lines.scale = 1.0;
                x.contours = {lines};
                p.push_back(x);
            };
            {
                auto h = heights(6.0);
                h.title = "Ensemble mean height (dam)";
                spreadChart("spread_500_ht", "500mb Height: ensemble mean and spread", "HGT", "500 mb", 0.1, 8.0, "Spread (standard deviation) of the 500 mb height among the 50 members (dam)", h);
                auto m = pressure();
                m.title = "Ensemble mean pressure (mb)";
                spreadChart("spread_mslp", "MSLP: ensemble mean and spread", "PRMSL", "mean sea level", 0.01, 8.0, "Spread of the sea level pressure among the 50 members (mb)", m);
                ContourSet t;
                t.interval = 5;
                t.title = "Ensemble mean 850 mb temperature (C)";
                t.color = QColor{30, 30, 30};
                spreadChart("spread_850_temp", "850mb Temperature: ensemble mean and spread", "TMP", "850 mb", 1.0, 5.0, "Spread of the 850 mb temperature among the 50 members (C)", t);
                ContourSet t2;
                t2.interval = 5;
                t2.title = "Ensemble mean 2 m temperature (C)";
                t2.color = QColor{30, 30, 30};
                spreadChart("spread_2m_temp", "2 m Temperature: ensemble mean and spread", "TMP", "2 m above ground", 1.0, 5.0, "Spread of the 2 m temperature among the 50 members (C)", t2);
            }
            // the chance of an amount of precipitation (the helpers above): the ready-made charts have the usual amounts; "Build a chart" has the amount (and the period) of the user's choosing
            {
                const CumulativeEnsemble ifs{"IFS-ENS", names, [] (int hour) { return "0-" + std::to_string(hour) + " hour acc fcst"; }};
                for (const auto& [id, inches, text] : {std::tuple{"ens_prob_total_0.01in", 0.01, "0.01"}, {"ens_prob_total_0.1in", 0.1, "0.10"}, {"ens_prob_total_0.25in", 0.25, "0.25"}, {"ens_prob_total_0.5in", 0.5, "0.50"},
                                                       {"ens_prob_total_1in", 1.0, "1.00"}, {"ens_prob_total_2in", 2.0, "2.00"}}) {
                    auto x = exceedProduct(ifs, id, std::string{"Probability of total precipitation of "} + text + " in or more (since the start of the run)", inches, 0);
                    x.fillTitle = std::string{"Chance of "} + text + " in or more of precipitation in total since the start of the run (% of the 50 members)";
                    p.push_back(x);
                }
                auto& made = generators();
                made["ens_total"] = [ifs] (const std::vector<double>& v) {   // inches
                    const double inches = v.at(0);
                    auto x = exceedProduct(ifs, generatedId("ens_total", {inches}), "Chance of " + trimmed(inches) + " in or more of precipitation in total (since the start of the run)", inches, 0);
                    x.fillTitle = "Chance of " + trimmed(inches) + " in or more of precipitation in total since the start of the run (% of the 50 members)";
                    return x;
                };
                made["ens_period"] = [ifs] (const std::vector<double>& v) {   // inches, hours
                    const double inches = v.at(0);
                    const int hours = std::max(3, static_cast<int>(std::lround(v.at(1) / 3.0)) * 3);
                    auto x = exceedProduct(ifs, generatedId("ens_period", {inches, static_cast<double>(hours)}), "Chance of " + trimmed(inches) + " in or more of precipitation in " + std::to_string(hours) + " hours", inches, hours);
                    x.fillTitleFor = [inches, hours] (int hour) {
                        return "Chance of " + trimmed(inches) + " in or more of precipitation in the " + std::to_string(std::min(hours, hour)) + " hours ending at hour " + std::to_string(hour) + " (% of the 50 members)";
                    };
                    return x;
                };
            }
            // spaghetti: one contour (or two) of a field from every member, each its own color, and the ensemble mean as the heavy black line
            const auto spaghetti = [&] (const std::string& id, const std::string& label, const char * variable, const char * level, double scale, std::vector<double> levels, const std::string& what) {
                auto x = make(id, label);
                x.linesOnly = true;
                x.needs = [memberNeeds, variable, level] (int hour) { return memberNeeds("z", variable, level, hour, hour == 0 ? std::string{"0 hour fcst"} : std::to_string(hour) + " hour fcst"); };
                x.derive = [scale] (Grids& g, const Context&) {
                    auto mean = g.at("z" + names.front());
                    for (size_t i = 0; i < mean.values.size(); i++) {
                        double sum = 0.0;
                        int n = 0;
                        for (const auto& name : names) {
                            const float v = g.at("z" + name).values[i];
                            if (std::isfinite(v)) {
                                sum += v;
                                n++;
                            }
                        }
                        mean.values[i] = n > 0 ? static_cast<float>(sum / n) : std::nanf("");
                    }
                    g["zm"] = std::move(mean);
                    (void) scale;
                };
                x.fillTitle = what + ": one thin line for each of the 50 members, the heavy black line the ensemble mean";
                for (const double value : levels) {
                    for (size_t m = 0; m < names.size(); m++) {
                        ContourSet c;
                        c.key = "z" + names[m];
                        c.scale = scale;
                        c.base = value;
                        c.interval = 100000.0;
                        c.color = QColor::fromHsv(static_cast<int>(m * 360 / names.size()), 230, 190);
                        c.width = 1.0;
                        c.labelled = false;
                        c.uniform = true;
                        x.contours.push_back(c);
                    }
                    ContourSet mean;
                    mean.key = "zm";
                    mean.scale = scale;
                    mean.base = value;
                    mean.interval = 100000.0;
                    mean.color = QColor{10, 10, 10};
                    mean.width = 2.6;
                    mean.uniform = true;
                    x.contours.push_back(mean);
                }
                p.push_back(x);
            };
            for (const auto& [low, high] : {std::pair{522, 564}, {528, 570}, {534, 576}, {540, 582}}) {
                spaghetti("spag_500_" + std::to_string(low) + "_" + std::to_string(high), "500mb " + std::to_string(low) + "/" + std::to_string(high) + " Height Contours (spaghetti)", "HGT", "500 mb", 0.1,
                          {static_cast<double>(low), static_cast<double>(high)}, "500 mb heights of " + std::to_string(low) + " and " + std::to_string(high) + " dam");
            }
            for (const auto& [low, high] : {std::pair{996, 1036}, {1000, 1040}, {1004, 1044}, {1008, 1048}}) {
                spaghetti("spag_mslp_" + std::to_string(low) + "_" + std::to_string(high), "MSLP " + std::to_string(low) + "/" + std::to_string(high) + " Isobar Contours (spaghetti)", "PRMSL", "mean sea level", 0.01,
                          {static_cast<double>(low), static_cast<double>(high)}, "sea level pressure of " + std::to_string(low) + " and " + std::to_string(high) + " mb");
            }
            // one member (the member picker chooses which; the first is the default)
            {
                auto x = make("member_2m_temp_mslp", "Member: 2 m Temperature and MSLP");
                x.wants = {GfsData::Want{"t", "TMP", "2 m above ground", "", "", "e01"}, GfsData::Want{"p", "PRMSL", "mean sea level", "", "", "e01"}};
                x.fill = [] (const Grids& g) { return pick(g, "t"); };
                x.ramp = temperature();
                x.quantity = Quantity::Temperature;
                x.fillTitle = "2 m temperature";
                x.legendStep = 5;
                x.contours = {pressure()};
                p.push_back(x);
                auto y = make("member_850_temp_mslp", "Member: 850mb Temperature and MSLP");
                y.wants = {GfsData::Want{"t", "TMP", "850 mb", "", "", "e01"}, GfsData::Want{"p", "PRMSL", "mean sea level", "", "", "e01"}};
                y.fill = [] (const Grids& g) { return pick(g, "t"); };
                y.ramp = temperature();
                y.quantity = Quantity::Temperature;
                y.fillTitle = "850 mb temperature";
                y.legendStep = 5;
                y.contours = {pressure()};
                p.push_back(y);
                auto z = make("member_500_ht", "Member: 500mb Height");
                z.wants = {GfsData::Want{"z", "HGT", "500 mb", "", "", "e01"}, GfsData::Want{"p", "PRMSL", "mean sea level", "", "", "e01"}};
                z.fill = [] (const Grids& g) { return GfsGrid::scaled(pick(g, "z"), 0.1); };
                z.ramp = Ramp{{{480, QColor{"#4b2d7f"}}, {510, QColor{"#3f74b8"}}, {540, QColor{"#9ad6e6"}}, {552, QColor{"#dcefb4"}}, {564, QColor{"#f3f0a4"}}, {576, QColor{"#f5ac5c"}}, {588, QColor{"#c5432e"}}, {600, QColor{"#8e1f2c"}}}};
                z.fillTitle = "500 mb height (dam)";
                z.legendStep = 12;
                z.contours = {heights(6.0)};
                p.push_back(z);
            }
        };

        // ---- ENS-ALL: every member of the ECMWF ensemble (50), the GEFS (31) and the GEPS (21) pooled as 102 members of one ensemble on a 0.5 degree grid, for the same valid time
        const auto ensAllRecipes = [&] {
            const auto make = [] (const std::string& id, const std::string& label) {
                Product x;
                x.source = "ENS-ALL";
                x.id = id;
                x.label = label;
                return x;
            };
            // the mean and the spread of all the members present under a prefix, in `scale` of the file's unit
            const auto statistics = [] (Grids& g, const std::string& prefix, double scale) {
                std::vector<const GfsGrid::Grid *> members;
                for (const auto& member : poolMembers()) {
                    const auto found = g.find(prefix + member.tag);
                    if (found != g.end() && !found->second.values.empty()) {
                        members.push_back(&found->second);
                    }
                }
                if (members.empty()) {
                    throw std::runtime_error{"no member could be read"};
                }
                auto mean = *members.front();
                auto spread = mean;
                for (size_t i = 0; i < mean.values.size(); i++) {
                    double sum = 0.0, square = 0.0;
                    int n = 0;
                    for (const auto * m : members) {
                        const float v = m->values[i];
                        if (std::isfinite(v)) {
                            sum += v;
                            square += static_cast<double>(v) * v;
                            n++;
                        }
                    }
                    mean.values[i] = n > 0 ? static_cast<float>(sum / n * scale) : std::nanf("");
                    spread.values[i] = n > 1 ? static_cast<float>(std::sqrt(std::max(0.0, square / n - (sum / n) * (sum / n))) * scale) : std::nanf("");
                }
                g[prefix + "m"] = std::move(mean);
                g[prefix + "s"] = std::move(spread);
                for (const auto& member : poolMembers()) {   // the members are done with
                    g.erase(prefix + member.tag);
                }
            };
            const auto spreadChart = [&] (const char * id, const char * label, const char * variable, const char * level, double scale, double top, const char * title, ContourSet lines) {
                auto x = make(id, label);
                x.pool = {variable, level, "f"};
                x.derive = [statistics, scale] (Grids& g, const Context&) { statistics(g, "f", scale); };
                x.fill = [] (const Grids& g) { return pick(g, "fs"); };
                x.ramp = spreadRamp(top);
                x.fillTitle = title;
                x.legendStep = 0;
                lines.key = "fm";
                lines.scale = 1.0;
                x.contours = {lines};
                p.push_back(x);
            };
            {
                auto h = heights(6.0);
                h.title = "Mean of all the members, height (dam)";
                spreadChart("all_spread_500_ht", "500mb Height: all ensembles, mean and spread", "HGT", "500 mb", 0.1, 8.0, "Spread (standard deviation) of the 500 mb height among the ECMWF, GEFS and GEPS members together (dam)", h);
                auto m = pressure();
                m.title = "Mean of all the members, pressure (mb)";
                spreadChart("all_spread_mslp", "MSLP: all ensembles, mean and spread", "PRMSL", "mean sea level", 0.01, 8.0, "Spread of the sea level pressure among all the members (mb)", m);
                ContourSet t;
                t.interval = 5;
                t.title = "Mean of all the members, 850 mb temperature (C)";
                t.color = QColor{30, 30, 30};
                spreadChart("all_spread_850_temp", "850mb Temperature: all ensembles, mean and spread", "TMP", "850 mb", 1.0, 5.0, "Spread of the 850 mb temperature among all the members (C)", t);
                ContourSet t2;
                t2.interval = 5;
                t2.title = "Mean of all the members, 2 m temperature (C)";
                t2.color = QColor{30, 30, 30};
                spreadChart("all_spread_2m_temp", "2 m Temperature: all ensembles, mean and spread", "TMP", "2 m above ground", 1.0, 5.0, "Spread of the 2 m temperature among all the members (C)", t2);
            }
            // spaghetti: a line from every member, each ensemble in a color family of its own (ECMWF reds, GEFS blues, GEPS greens), and the mean of all of them heavy and black
            const auto spaghetti = [&] (const std::string& id, const std::string& label, const char * variable, const char * level, double scale, std::vector<double> levels, const std::string& what) {
                auto x = make(id, label);
                x.linesOnly = true;
                x.pool = {variable, level, "z"};
                x.derive = [scale] (Grids& g, const Context&) {
                    std::vector<const GfsGrid::Grid *> members;
                    for (const auto& member : poolMembers()) {
                        const auto found = g.find("z" + member.tag);
                        if (found != g.end() && !found->second.values.empty()) {
                            members.push_back(&found->second);
                        }
                    }
                    if (members.empty()) {
                        throw std::runtime_error{"no member could be read"};
                    }
                    auto mean = *members.front();
                    for (size_t i = 0; i < mean.values.size(); i++) {
                        double sum = 0.0;
                        int n = 0;
                        for (const auto * m : members) {
                            if (std::isfinite(m->values[i])) {
                                sum += m->values[i];
                                n++;
                            }
                        }
                        mean.values[i] = n > 0 ? static_cast<float>(sum / n) : std::nanf("");
                    }
                    g["zm"] = std::move(mean);
                    (void) scale;
                };
                x.fillTitle = what + ": a line from each of the 50 ECMWF (red), 31 GEFS (blue) and 21 GEPS (green) members, the heavy black line the mean of all";
                for (const double value : levels) {
                    std::map<std::string, int> counts;
                    for (const auto& member : poolMembers()) {
                        ContourSet c;
                        c.key = "z" + member.tag;
                        c.scale = scale;
                        c.base = value;
                        c.interval = 100000.0;
                        const int n = counts[member.source]++;
                        const int span = member.source == "IFS-ENS" ? 50 : member.source == "GEFS" ? 31 : 21;
                        const int hue = member.source == "IFS-ENS" ? 355 : member.source == "GEFS" ? 215 : 125;   // a color family for each ensemble, the members a shade apart
                        c.color = QColor::fromHsv((hue + n * 28 / span) % 360, 200 + (n % 3) * 20, 170 + (n % 4) * 20);
                        c.width = 0.9;
                        c.labelled = false;
                        c.uniform = true;
                        if (value == levels.front() && n == 0) {
                            c.title = member.source == "IFS-ENS" ? "ECMWF ENS" : member.source == "GEFS" ? "GEFS" : "GEPS";
                        }
                        x.contours.push_back(c);
                    }
                    ContourSet mean;
                    mean.key = "zm";
                    mean.scale = scale;
                    mean.base = value;
                    mean.interval = 100000.0;
                    mean.color = QColor{10, 10, 10};
                    mean.width = 2.6;
                    mean.uniform = true;
                    if (value == levels.front()) {
                        mean.title = "Mean of all";
                    }
                    x.contours.push_back(mean);
                }
                p.push_back(x);
            };
            for (const auto& [low, high] : {std::pair{522, 564}, {528, 570}, {534, 576}, {540, 582}}) {
                spaghetti("all_spag_500_" + std::to_string(low) + "_" + std::to_string(high), "500mb " + std::to_string(low) + "/" + std::to_string(high) + " Height Contours (spaghetti)", "HGT", "500 mb", 0.1,
                          {static_cast<double>(low), static_cast<double>(high)}, "500 mb heights of " + std::to_string(low) + " and " + std::to_string(high) + " dam");
            }
            for (const auto& [low, high] : {std::pair{996, 1036}, {1000, 1040}, {1004, 1044}, {1008, 1048}}) {
                spaghetti("all_spag_mslp_" + std::to_string(low) + "_" + std::to_string(high), "MSLP " + std::to_string(low) + "/" + std::to_string(high) + " Isobar Contours (spaghetti)", "PRMSL", "mean sea level", 0.01,
                          {static_cast<double>(low), static_cast<double>(high)}, "sea level pressure of " + std::to_string(low) + " and " + std::to_string(high) + " mb");
            }
            {   // the chance of an amount of precipitation among all the members: each member's total since the start of its own run, or its amount in the period ending at the valid time
                for (const auto& [id, inches, text] : {std::tuple{"all_prob_24h_0.01in", 0.01, "0.01"}, {"all_prob_24h_0.1in", 0.1, "0.10"}, {"all_prob_24h_0.25in", 0.25, "0.25"}, {"all_prob_24h_0.5in", 0.5, "0.50"},
                                                       {"all_prob_24h_1in", 1.0, "1.00"}, {"all_prob_24h_2in", 2.0, "2.00"}}) {
                    auto x = exceedPooledProduct(id, std::string{"Probability of "} + text + " in or more of precipitation in 24 hours: all ensembles", inches, 24);
                    x.fillTitle = std::string{"Chance of "} + text + " in or more of precipitation in the 24 hours ending at the valid time (% of the 102 members of the ECMWF, GEFS and GEPS together)";
                    p.push_back(x);
                }
                auto& made = generators();
                made["all_period"] = [] (const std::vector<double>& v) {   // inches, hours
                    const double inches = v.at(0);
                    const int hours = std::max(6, static_cast<int>(std::lround(v.at(1) / 6.0)) * 6);
                    auto x = exceedPooledProduct(generatedId("all_period", {inches, static_cast<double>(hours)}), "Chance of " + trimmed(inches) + " in or more of precipitation in " + std::to_string(hours) + " hours: all ensembles", inches, hours);
                    x.fillTitleFor = [inches, hours] (int hour) {
                        return "Chance of " + trimmed(inches) + " in or more of precipitation in the " + std::to_string(std::min(hours, hour)) + " hours ending at hour " + std::to_string(hour) + " (% of all the members, ECMWF, GEFS and GEPS)";
                    };
                    return x;
                };
            }
        };

        // ---- GEOS-FP (NASA GMAO): the aerosol fields the model is used for, from its 3 hourly aerosol collection (kg/m3, drawn in micrograms; dust has a chart of its own in several forms)
        const auto geosRecipes = [&] {
            const auto stretched = [] (const Ramp& base, double factor) {
                Ramp out = base;
                for (auto& stop : out.stops) {
                    stop.first *= factor;
                }
                return out;
            };
            // micrograms per cubic meter of dust at the surface: nothing under 5, then yellows to browns to purple where a dust storm is
            const Ramp dust{{{0, QColor{246, 226, 150, 0}}, {5, QColor{246, 226, 150, 0}}, {10, QColor{"#f5dc8a"}}, {25, QColor{"#eab552"}}, {50, QColor{"#d98a3a"}}, {100, QColor{"#c0602a"}}, {250, QColor{"#9a3a25"}},
                             {500, QColor{"#6f1d3a"}}, {1000, QColor{"#4a0f5c"}}}};
            const Ramp optical{{{0, QColor{232, 243, 216, 0}}, {0.05, QColor{232, 243, 216, 0}}, {0.1, QColor{"#e8f3d8"}}, {0.25, QColor{"#cfe6a0"}}, {0.5, QColor{"#f1e27a"}}, {1.0, QColor{"#f0a63a"}},
                                {2.0, QColor{"#d63a2a"}}, {3.0, QColor{"#7a1a6a"}}}};
            const Ramp airQuality{{{0, QColor{0, 228, 0, 0}}, {4, QColor{0, 228, 0, 0}}, {12, QColor{"#00e400"}}, {35, QColor{"#ffff00"}}, {55, QColor{"#ff7e00"}}, {150, QColor{"#ff0000"}}, {250, QColor{"#8f3f97"}},
                                   {500, QColor{"#7e0023"}}}};
            const Ramp smoke{{{0, QColor{217, 205, 184, 0}}, {2, QColor{217, 205, 184, 0}}, {5, QColor{"#d9cdb8"}}, {10, QColor{"#b8a58a"}}, {25, QColor{"#8c735a"}}, {50, QColor{"#5d4a3a"}}, {100, QColor{"#3a2c25"}},
                              {250, QColor{"#1c1410"}}}};
            struct Spec {
                const char * id;
                const char * label;
                std::vector<std::pair<const char *, double>> parts;   // variable and the factor to micrograms (or the unit) per cubic meter, added up
                double scale;                                         // kg/m3 to micrograms, kg/m2 to milligrams (1e6) or 1 for an optical thickness
                const Ramp * ramp;
                const char * title;
            };
            const std::vector<Spec> specs{
                {"dust_surface", "Dust: surface concentration", {{"DUSMASS", 1.0}}, 1e9, &dust, "Dust at the surface (micrograms per cubic meter)"},
                {"dust_pm25", "Dust: fine (PM2.5) surface concentration", {{"DUSMASS25", 1.0}}, 1e9, &dust, "Dust smaller than 2.5 micrometers at the surface (micrograms per cubic meter)"},
                {"dust_column", "Dust: column mass", {{"DUCMASS", 1.0}}, 1e6, nullptr, "Dust in the whole column (milligrams per square meter)"},
                {"dust_aod", "Dust: aerosol optical thickness", {{"DUEXTTAU", 1.0}}, 1.0, &optical, "Dust optical thickness at 550 nm (the amount of light it takes out; over 1 is thick)"},
                {"aod_total", "All aerosols: optical thickness", {{"TOTEXTTAU", 1.0}}, 1.0, &optical, "Total aerosol optical thickness at 550 nm"},
                {"pm25_total", "Fine particles (PM2.5), all species", {{"DUSMASS25", 1.0}, {"SSSMASS25", 1.0}, {"BCSMASS", 1.0}, {"OCSMASS", 1.4}, {"SO4SMASS", 1.375}}, 1e9, &airQuality,
                 "PM2.5 at the surface: dust, sea salt, black and organic carbon, sulfate (micrograms per cubic meter)"},
                {"smoke_surface", "Smoke: black and organic carbon at the surface", {{"BCSMASS", 1.0}, {"OCSMASS", 1.4}}, 1e9, &smoke, "Black carbon plus organic matter at the surface (micrograms per cubic meter)"},
                {"sulfate_surface", "Sulfate at the surface", {{"SO4SMASS", 1.0}}, 1e9, &smoke, "Sulfate at the surface (micrograms per cubic meter)"},
                {"seasalt_surface", "Sea salt at the surface", {{"SSSMASS", 1.0}}, 1e9, &smoke, "Sea salt at the surface (micrograms per cubic meter)"},
            };
            for (const auto& spec : specs) {
                Product x;
                x.source = "GEOS-FP";
                x.id = spec.id;
                x.label = spec.label;
                std::vector<std::string> keys;
                for (size_t i = 0; i < spec.parts.size(); i++) {
                    keys.push_back("a" + std::to_string(i));
                    x.wants.push_back(GfsData::Want{keys.back(), spec.parts[i].first, "aerosol", "", ""});
                }
                x.wants.push_back(GfsData::Want{"u", "UGRD", "10 m above ground", "", ""});
                x.wants.push_back(GfsData::Want{"v", "VGRD", "10 m above ground", "", ""});
                const auto parts = spec.parts;
                const double scale = spec.scale;
                x.derive = [parts, keys, scale] (Grids& g, const Context&) {
                    auto sum = g.at(keys.front());
                    for (size_t i = 0; i < sum.values.size(); i++) {
                        double total = 0.0;
                        for (size_t k = 0; k < keys.size(); k++) {
                            total += static_cast<double>(g.at(keys[k]).values[i]) * parts[k].second;
                        }
                        sum.values[i] = std::isfinite(total) ? static_cast<float>(total * scale) : std::nanf("");
                    }
                    g["total"] = std::move(sum);
                };
                x.fill = [] (const Grids& g) { return pick(g, "total"); };
                x.ramp = spec.ramp ? *spec.ramp : stretched(dust, 2.0);   // the column in milligrams: twice the surface scale, roughly a kilometer and a half of mixed layer
                x.fillTitle = spec.title;
                x.legendStep = 0;
                x.barbU = "u";
                x.barbV = "v";
                x.periodHours = 3;   // the collection is 3 hour averages
                p.push_back(x);
            }
        };

        // The models in the registry's order: each one's clone of the GFS charts, then the charts of its own
        const std::map<std::string, std::function<void()>> own{{"GEFS", gefsRecipes}, {"GEPS", gepsRecipes}, {"SPCPOST", [&] { spcRecipes("SPCPOST", false); }}, {"SPCGEFS", [&] { spcRecipes("SPCGEFS", true); }}, {"SFCOA", [&] { mesoRecipes("SFCOA"); }}, {"SPCREFS", spcRefsRecipes}, {"CFS30", cfs30Recipes}, {"EC46", [&] { openMeteoRecipes("EC46"); }}, {"SEAS5", [&] { openMeteoRecipes("SEAS5"); }}, {"RRFSD", [&] { mesoRecipes("RRFSD"); }}, {"RRFS", [&] { rrfsRecipes("RRFS"); }}, {"HRRR", [&] { rrfsRecipes("HRRR"); }}, {"HRRR-AK", [&] { rrfsRecipes("HRRR-AK"); }}, {"RRFS-AK", [&] { rrfsRecipes("RRFS-AK"); }}, {"RRFS-HI", [&] { rrfsRecipes("RRFS-HI"); }}, {"RRFS-PR", [&] { rrfsRecipes("RRFS-PR"); }}, {"RRFS-NA", [&] { rrfsRecipes("RRFS-NA"); }}, {"GEOS-FP", geosRecipes}, {"IFS-ENS", ensRecipes}, {"ENS-ALL", ensAllRecipes}, {"REFS", refsRecipes}, {"HREF", hrefRecipes}, {"GFS-WAVE", [&] { waveRecipes("GFS-WAVE"); }}, {"GEFS-WAVE", [&] { waveRecipes("GEFS-WAVE"); }}, {"HAFSA", [&] { hafsRecipes("HAFSA"); }}, {"HAFSB", [&] { hafsRecipes("HAFSB"); }}};
        for (const auto& def : GfsModels::all()) {
            if (def.clone.enabled) {
                cloneFrom(def);
            }
            const auto found = own.find(def.id);
            if (found != own.end()) {
                found->second();
            }
        }
        for (auto& chart : p) {   // the NBM's hourly runs (01Z, 13Z ...) carry the 1 hour fields only; the 12 hour extremes are in the 00 and 12Z runs, the 6 and 12 hour chances in 00, 06, 12 and 18Z
            if (chart.source == "NBM") {
                if (chart.id == "2m_min_temp" || chart.id == "2m_max_temp") {
                    chart.mainCycleStep = 12;
                    chart.periodHours = 12;
                    chart.periodEndHourUtc = chart.id == "2m_min_temp" ? 12 : 0;
                } else if (chart.id == "tstm_prob" || chart.id == "6hour_precip_chance" || chart.id == "12hour_precip_chance") {
                    chart.mainCycleStep = 6;
                    chart.periodHours = chart.id == "12hour_precip_chance" ? 12 : 6;
                }
            }
        }
        return p;
    }();
    return list;
}

const std::vector<PoolMember>& poolMembers() {
    static const std::vector<PoolMember> list = [] {
        std::vector<PoolMember> out;
        const auto two = [] (int m) { return (m < 10 ? "0" : "") + std::to_string(m); };
        for (int m = 1; m <= 50; m++) {
            out.push_back({"IFS-ENS", "e" + two(m), "I" + two(m)});
        }
        out.push_back({"GEFS", "c00", "G00"});
        for (int m = 1; m <= 30; m++) {
            out.push_back({"GEFS", "p" + two(m), "G" + two(m)});
        }
        out.push_back({"GEPS", "c00", "C00"});
        for (int m = 1; m <= 20; m++) {
            out.push_back({"GEPS", "p" + two(m), "C" + two(m)});
        }
        return out;
    }();
    return list;
}

const Product * product(const std::string& id, const std::string& source) {
    for (const auto& p : products()) {
        if (p.id == id && p.source == source) {
            return &p;
        }
    }
    if (id.compare(0, 4, "gen~") == 0) {   // a chart made on request: built from its template and kept
        static std::mutex mutex;
        static std::map<std::string, Product> made;
        std::lock_guard lock{mutex};
        const auto key = source + "|" + id;
        if (const auto found = made.find(key); found != made.end()) {
            return &found->second;
        }
        std::vector<std::string> parts;
        for (const auto& part : QString::fromStdString(id).split('~')) {
            parts.push_back(part.toStdString());
        }
        const auto generator = parts.size() >= 3 ? generators().find(parts[1]) : generators().end();
        if (generator == generators().end()) {
            return nullptr;
        }
        std::vector<double> values;
        for (size_t i = 2; i < parts.size(); i++) {
            values.push_back(QString::fromStdString(parts[i]).toDouble());
        }
        try {
            auto product = generator->second(values);
            if (product.source != source || product.id != id) {   // the id spells the values the way the template does: another spelling is not that chart
                return nullptr;
            }
            return &(made[key] = std::move(product));
        } catch (const std::exception&) {
            return nullptr;
        }
    }
    return nullptr;
}

std::vector<Template> templates(const std::string& source) {
    const auto ingredientTemplate = [] (const std::string& id, const std::string& title, bool ensemble, const std::string& name) {
        const auto conditions = ingredientConditions(ensemble, name);
        std::vector<double> indices;
        std::vector<std::string> names;
        size_t first = 0, second = 0, third = 0;
        for (size_t i = 0; i < conditions.size(); i++) {
            indices.push_back(static_cast<double>(i));
            names.push_back(conditionText(conditions[i]));
            const std::string ing = ingredients()[conditions[i].ingredient].name;
            if (ing == "MUCAPE" && conditions[i].limit == 500) first = i;
            if (ing == "Sfc-500 mb shear" && conditions[i].limit == 40) second = i;
            if ((ing == "6-h rain" || ing == "Rain rate") && conditions[i].limit == 0.05) third = i;
        }
        auto optional = indices;
        auto optionalNames = names;
        optional.insert(optional.begin(), -1.0);
        optionalNames.insert(optionalNames.begin(), "(nothing more)");
        return Template{id, title, {{"a", "Ingredient 1", "", indices, static_cast<double>(first), names}, {"b", "and ingredient 2", "", optional, static_cast<double>(second), optionalNames},
                                    {"c", "and ingredient 3", "", optional, static_cast<double>(third), optionalNames}}};
    };
    if (std::find(ingredientSources().begin(), ingredientSources().end(), source) != ingredientSources().end()) {
        return {ingredientTemplate("ingr_" + source, "Ingredients met together (CAPE, shear, moisture, rain: where each holds and how many do)", false, source)};
    }
    if (source == "IFS-ENS") {   // the ECMWF ensemble: the chance of an amount of precipitation, in total or in a period
        const std::vector<double> amounts{0.01, 0.05, 0.1, 0.25, 0.5, 0.75, 1, 1.5, 2, 3, 4, 6};
        return {{"ens_total", "Chance of precipitation of an amount or more in total (since the start of the run)", {{"limit", "Total of", "in", amounts, 0.01}}},
                {"ens_period", "Chance of precipitation of an amount or more in a period", {{"limit", "Amount of", "in", amounts, 0.5}, {"hours", "in", "hours", {3, 6, 12, 24, 36, 48, 72, 96, 120, 168}, 24}}}};
    }
    {   // the same two templates for each of the other ensembles that cannot count on a template of its own: the GEPS (running totals) and all the ensembles pooled
        const std::vector<double> amounts{0.01, 0.05, 0.1, 0.25, 0.5, 0.75, 1, 1.5, 2, 3, 4, 6};
        if (source == "GEPS") {
            return {{"geps_total", "Chance of precipitation of an amount or more in total (since the start of the run)", {{"limit", "Total of", "in", amounts, 0.01}}},
                    {"geps_period", "Chance of precipitation of an amount or more in a period", {{"limit", "Amount of", "in", amounts, 0.5}, {"hours", "in", "hours", {3, 6, 12, 24, 36, 48, 72, 96, 120, 168}, 24}}}};
        }
        if (source == "ENS-ALL") {
            return {{"all_period", "Chance of precipitation of an amount or more in a period: all ensembles (ECMWF, GEFS, GEPS)", {{"limit", "Amount of", "in", amounts, 0.5}, {"hours", "in", "hours", {6, 12, 24, 48, 72, 96}, 24}}}};
        }
    }
    if (source != "GEFS") {
        return {};
    }
    const auto one = [] (const char * id, const char * title, const char * label, const char * unit, std::vector<double> choices, double standard) {
        return Template{id, title, {{"limit", label, unit, std::move(choices), standard}}};
    };
    return {
        ingredientTemplate("ingredients", "Chance of ingredients together (CAPE, shear, moisture, rain: the share of the members that have all)", true, "GEFS"),
        {"rain_over", "Chance of rain over a limit in a period", {{"limit", "Rain over", "in", {0.01, 0.1, 0.25, 0.5, 1, 1.5, 2, 3, 4, 5, 6, 8, 10}, 1}, {"hours", "in", "hours", {6, 12, 18, 24, 30, 36, 42, 48, 54, 60, 72, 84, 96, 120, 168, 240}, 24}}},
        {"rain_under", "Chance of rain under a limit in a period (dry)", {{"limit", "Rain under", "in", {0.01, 0.1, 0.25, 0.5, 1}, 0.01}, {"hours", "in", "hours", {6, 12, 18, 24, 30, 36, 42, 48, 54, 60, 72, 84, 96, 120, 168, 240}, 120}}},
        {"ptype", "Chance of a precipitation type in 6 hours", {{"type", "Type", "", {1, 2, 3, 4}, 2, {"rain", "snow", "ice pellets", "freezing rain"}}, {"limit", "with at least", "in", {0.01, 0.1, 0.25, 0.5, 1}, 0.01}}},
        {"heavy_rain", "Heavy rain potential (precipitable water + CAPE + rain)", {{"sigmas", "Precipitable water over", "std. dev. above normal", {1, 1.5, 2, 2.5, 3}, 2}, {"cape", "and MUCAPE over", "J/kg", {100, 250, 500, 1000, 1500, 2000}, 500}, {"qpf", "and rain in 6 hours over", "in", {0.01, 0.1, 0.25, 0.5, 1}, 0.1}}},
        {"stdanom", "Chance of an anomaly of so many standard deviations", {{"what", "Field", "", {1, 2, 3, 4, 5}, 1, {"500 mb height", "700 mb height", "700 mb temperature", "850 mb temperature", "precipitable water"}}, {"direction", "Anomaly", "", {1, 2}, 1, {"below normal", "above normal"}}, {"sigmas", "by more than", "std. dev.", {1, 2, 3, 4, 5}, 1}}},
        one("temp_under", "Chance of a 2 m temperature under a limit", "Temperature under", "F", {-40, -20, 0, 10, 20, 25, 28, 32, 40, 50, 60, 70}, 32),
        one("temp_over", "Chance of a 2 m temperature over a limit", "Temperature over", "F", {70, 80, 90, 95, 100, 105, 110, 115}, 90),
        one("tmin_under", "Chance of a 6 hour minimum temperature under a limit", "Lowest in 6 hours under", "F", {-40, -20, 0, 20, 32, 40}, 32),
        one("tmax_over", "Chance of a 6 hour maximum temperature over a limit", "Highest in 6 hours over", "F", {70, 80, 90, 100, 110, 120}, 90),
        one("tmax_under", "Chance of a 6 hour maximum temperature under a limit", "Highest in 6 hours under", "F", {-40, -20, 0, 20, 32}, 20),
        one("dewpoint_over", "Chance of a dewpoint over a limit", "Dewpoint over", "F", {50, 55, 60, 65, 70, 75, 80}, 65),
        one("heat_over", "Chance of a heat index over a limit", "Heat index over", "F", {90, 100, 105, 110, 115, 120}, 105),
        one("rh_under", "Chance of a relative humidity under a limit", "Humidity under", "%", {5, 10, 15, 20, 30}, 15),
        one("wind_over", "Chance of a 10 m wind over a limit", "Wind over", "kt", {10, 15, 20, 30, 40, 50, 75}, 20),
        one("gust_over", "Chance of wind gusts over a limit", "Gusts over", "kt", {20, 25, 30, 35, 40, 50, 60, 70}, 35),
        one("mslp_under", "Chance of a sea level pressure under a limit", "Pressure under", "mb", {960, 965, 970, 980, 990, 1000}, 990),
        one("mslp_over", "Chance of a sea level pressure over a limit", "Pressure over", "mb", {1020, 1030, 1035, 1040}, 1030),
        one("pw_over", "Chance of precipitable water over a limit", "Precipitable water over", "in", {1, 1.25, 1.5, 1.75, 2, 2.25}, 1.5),
        one("cloud_under", "Chance of a 6 hour mean cloud cover under a limit", "Cloud cover under", "%", {10, 30}, 10),
        one("cloud_over", "Chance of a 6 hour mean cloud cover over a limit", "Cloud cover over", "%", {70, 90}, 90),
        one("cape_over", "Chance of CAPE over a limit", "CAPE over", "J/kg", {250, 500, 1000, 1500, 2000, 3000, 4000, 5000, 6000, 8000}, 1000),
        one("mucape_over", "Chance of the most unstable CAPE over a limit", "MUCAPE over", "J/kg", {250, 500, 1000, 1500, 2000, 3000, 4000}, 500),
        one("z500_over", "Chance of a 500 mb height over a limit", "500 mb height over", "m", {5760, 5820, 5880, 5940, 6000}, 5940),
    };
}

std::string generatedChart(const std::string& templateId, const std::vector<double>& values) {
    return generatedId(templateId, values);
}

std::string sourceLabel(const std::string& source) {
    const auto * def = GfsModels::find(source);
    return def ? def->label : "NOAA/NCEP GFS 0.25 degree";
}

std::vector<std::string> sectorIds(const std::string& source) {
    const auto * def = GfsModels::find(source);
    if (def && def->sectors == GfsModels::Sectors::Conus) {   // the contiguous United States and its regions
        return {"CONUS", "NORTHEAST", "MID-ATLANTIC", "SOUTHEAST", "GREAT-LAKES", "OHIO-VALLEY", "S-PLAINS", "C-PLAINS", "N-PLAINS", "ROCKIES", "SOUTHWEST", "PACIFIC-NW", "CALIFORNIA", "GULF-COAST"};
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
        bool windFill{false};   // the chart's color fill is the speed of this wind instead of its own (the lines and barbs stay)
        std::string fillText;   // what the header says of that fill
        std::string fillPalette;   // the model guidance site's color bands for it (the jet levels only: its scale begins at 70 kt), else the program's own scale
        ContourSet contour;
        std::string barbU, barbV;
        QColor barbColor{20, 20, 20};   // the barbs stacked with others: a color for each level
        std::string barbColorName{"black"};
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
                // the same color for a level as its wind barbs have when they are stacked (850 red, 700 green, 500 blue ...)
                static const std::map<std::string, std::pair<QColor, std::string>> levelColors{
                    {"850", {QColor{200, 40, 40}, "red"}}, {"700", {QColor{30, 140, 60}, "green"}}, {"500", {QColor{30, 80, 200}, "blue"}}, {"300", {QColor{130, 50, 170}, "purple"}},
                    {"250", {QColor{200, 60, 150}, "pink"}}, {"200", {QColor{140, 90, 40}, "brown"}}};
                const auto& tint = levelColors.at(level);
                o.contour.title = std::string{level} + "mb height (dam, " + tint.second + ")";
                o.contour.color = tint.first;
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
            // the wind speed as the color fill, whatever the chart: one of these replaces the chart's own fill (the lines and the barbs stay), so that a temperature chart can show the jet too
            for (const auto& [id, label, level] : {std::tuple{"wfill_10m", "10 m wind speed", "10 m above ground"}, {"wfill_850", "850mb wind speed", "850 mb"}, {"wfill_700", "700mb wind speed", "700 mb"},
                                                    {"wfill_500", "500mb wind speed", "500 mb"}, {"wfill_300", "300mb wind speed", "300 mb"}, {"wfill_250", "250mb wind speed", "250 mb"}, {"wfill_200", "200mb wind speed", "200 mb"}}) {
                Overlay o;
                o.id = id;
                o.label = label;
                o.group = "Fill";
                o.sources = GfsModels::overlayModels();
                o.windFill = true;
                const std::string levelText = level;
                o.fillText = std::string{"Wind speed at "} + (levelText == "10 m above ground" ? "10 m" : levelText) + " (kt)";
                o.fillPalette = levelText == "300 mb" || levelText == "250 mb" || levelText == "200 mb" ? "isotach" : "";
                o.needs = [levelText] (int hour) { return std::vector<GfsData::Need>{{hour, {"u", "UGRD", levelText, "", ""}}, {hour, {"v", "VGRD", levelText, "", ""}}}; };
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
                static const std::map<std::string, std::pair<QColor, std::string>> levelColors{
                    {"barbs_10m", {QColor{90, 90, 90}, "gray"}}, {"barbs_850", {QColor{200, 40, 40}, "red"}}, {"barbs_700", {QColor{30, 140, 60}, "green"}}, {"barbs_500", {QColor{30, 80, 200}, "blue"}},
                    {"barbs_300", {QColor{130, 50, 170}, "purple"}}, {"barbs_250", {QColor{200, 60, 150}, "pink"}}, {"barbs_200", {QColor{140, 90, 40}, "brown"}}};
                if (const auto found = levelColors.find(id); found != levelColors.end()) {
                    o.barbColor = found->second.first;
                    o.barbColorName = found->second.second;
                }
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
    if (!product.pickerGroup.empty()) {
        return product.pickerGroup;
    }
    std::string id = product.id;
    if (id.compare(0, 4, "gen~") == 0) {
        return "Built charts";
    }
    std::transform(id.begin(), id.end(), id.begin(), [] (unsigned char c) { return static_cast<char>(std::tolower(c)); });   // ("prob_low_IFR")
    const auto contains = [] (const std::string& text, const char * part) { return text.find(part) != std::string::npos; };
    const auto startsWith = [] (const std::string& text, const char * prefix) { return text.compare(0, std::char_traits<char>::length(prefix), prefix) == 0; };
    // the kind of statistic comes first (what the ensemble makes of the members), the field second: the charts of the ensembles would otherwise all sit in "Surface"
    std::string core = id;   // the id without the words that say how it was made: "ens_850_wnd" is an 850 mb wind chart
    for (bool again = true; again;) {
        again = false;
        for (const char * prefix : {"ens_", "all_", "mean_", "member_", "combo_", "blend_", "lpmm_", "pmm_", "eas_"}) {
            if (startsWith(core, prefix)) {
                core = core.substr(std::char_traits<char>::length(prefix));
                again = true;
            }
        }
    }
    if (startsWith(id, "spread_") || startsWith(id, "all_spread_") || contains(id, "_spread")) {
        return "Ensemble spread";
    }
    if (startsWith(id, "swath")) {
        return "Swaths";
    }
    if ((contains(id, "prob_") || (id.size() > 5 && id.compare(id.size() - 5, 5, "_prob") == 0)) && !contains(id, "problem")) {   // the chance of something, by what it is the chance of
        const auto has = [&] (const char * part) { return contains(core, part); };
        if (has("precip") || has("rain") || has("snow") || has("ice") || has("sleet") || has("freezing_rain") || has("pwat") || has("total") || has("qpf")) {
            return "Probability: precipitation";
        }
        if (has("vis") || has("ceil") || has("ifr") || has("vfr")) {
            return "Probability: aviation and visibility";
        }
        if (has("cape") || has("tstm") || has("hlcy") || has("uphl") || has("refd") || has("cref") || has("refc") || has("etop") || has("vwshr") || has("shear") || has("cin") || has("tornado") || has("hail") || has("lightning")) {
            return "Probability: severe and storms";
        }
        return "Probability: temperature and wind";
    }
    if (startsWith(core, "pct") || contains(core, "efi_") || startsWith(core, "efi")) {
        return "Ensemble: percentiles and extremes";
    }
    if (startsWith(core, "thunder") || startsWith(id, "severe_") || startsWith(id, "gefsmlp_")) {   // the SPC's post-processed chances of thunder and severe weather
        return "Probability: severe and storms";
    }
    if (contains(id, "spag") || contains(id, "_iso") || contains(id, "paintball") || (contains(id, "_ht") && id.size() > 8 && std::isdigit(static_cast<unsigned char>(id[0])) && std::count(id.begin(), id.end(), '_') >= 2 && std::isdigit(static_cast<unsigned char>(id[id.find('_') + 1])))) {
        return "Ensemble: spaghetti and paintballs";   // (the last: "500_510_552_ht", a level then the contour values)
    }
    if (startsWith(id, "member_")) {
        return "Ensemble: single members";
    }
    const auto has = [&core] (const char * text) { return core.find(text) != std::string::npos; };
    if (has("trend") || has("chng")) {
        return "Changes and trends";
    }
    if (has("anom")) {
        return "Anomalies";
    }
    if (startsWith(core, "sat_")) {
        return "Satellite";
    }
    if (core == "sst" || core == "waves" || has("wave") || has("wv_ht") || has("swell") || has("dir_per")) {
        return "Ocean and waves";
    }
    // the severe weather charts in kinds: the fuel, the rotation, the storms themselves and the composite indices
    const bool endsWithCin = core.size() >= 3 && core.compare(core.size() - 3, 3, "cin") == 0;   // "sbcin", "mlcin", "ens_cin"
    if (has("cape") || has("cin_") || endsWithCin || has("lifted") || has("lapse") || has("lcl") || has("lfc")) {
        return "Severe: instability";
    }
    if (has("helicity") || has("hlcy") || has("srh") || has("relv") || has("uphl") || has("vwshr") || has("critical_angle") || has("wind_0_6km") ||
        has("shear_0_") || has("effective_shear")) {   // the low-level and storm shear of the severe weather indices (the deep layer shear of the cyclones is "dynamics")
        return "Severe: rotation and shear";
    }
    if (has("stp") || has("scp") || has("ship") || has("ehi") || has("dcp")) {
        return "Severe: composite indices";
    }
    if (has("radar") || has("reflectivity") || has("refd") || has("refc") || has("echo") || has("lightning") || has("updraft") || has("downdraft") || has("hail")) {
        return "Severe: storm structure";
    }
    if (has("dust") || has("aod") || has("pm25") || has("smoke") || has("sulfate") || has("seasalt") || has("aerosol")) {
        return "Air quality and smoke";
    }
    if (core == "thetae_2m" || has("mixr")) {   // moisture at the surface, not the mixing of the layer
        return "Surface";
    }
    // the lowest kilometer and the fire weather indices: the mixing, the ventilation, the dry air
    if (has("pbl") || has("vent") || has("transport") || has("haines") || has("mix_") || has("mixing") || has("fire") || core == "2m_rh_10m_wnd") {
        return "Boundary layer and fire weather";
    }
    if (has("shear") || has("steering") || has("div") || has("stream") || has("thetae") || has("vor") || has("trop") || has("850vor")) {
        return "Tropical and dynamics";
    }
    if (has("vis") || has("ceil") || has("fog") || has("flight")) {
        return "Aviation and visibility";
    }
    if (core == "rh_700_300") {
        return "Upper air";
    }
    if (has("precip") || has("snow") || has("rain") || has("qpf") || has("ptype") || has("thick") || has("pwat") || has("ice")) {
        return "Precipitation and moisture";
    }
    if (core.size() > 3 && std::isdigit(static_cast<unsigned char>(core[0])) && (has("_wnd") || has("_temp") || has("_rh") || has("rh_700") || has("_ht") || core.compare(1, 2, "00") == 0 || core.compare(0, 3, "250") == 0 || core.compare(0, 3, "925") == 0) && core.compare(0, 3, "10m") != 0 && core.compare(0, 2, "2m") != 0) {
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

std::vector<MemberChoice> memberChoices(const Product& product, const std::string& runId) {
    std::vector<GfsData::Want> wanted = product.wants;
    if (product.needs) {
        for (const auto& need : product.needs(12)) {
            wanted.push_back(need.want);
        }
    }
    std::vector<MemberChoice> out;
    if (wanted.empty() || product.linesOnly) {
        return out;
    }
    if (product.source == "SPCREFS") {   // the arrays with a member axis: ten members (SPC's viewer names them HRRR, RRFS and the REFS members)
        if (!product.wants.empty() && product.wants.front().stat == "m001") {
            const auto names = UtilitySpcRefs::memberNames(runId);   // the store's own names once one has been opened ("RRFS Ctl", "HRRR -6h" ...)
            for (int m = 1; m <= 10; m++) {
                const auto index = static_cast<size_t>(m - 1);
                out.push_back({"m" + std::string{m < 10 ? "00" : "0"} + std::to_string(m), index < names.size() && !names[index].empty() ? names[index] : "Member " + std::to_string(m)});
            }
        }
        return out;
    }
    if (product.source == "REFS") {
        if (product.id.compare(0, 7, "member_") == 0) {
            for (int m = 1; m <= 5; m++) {
                out.push_back({"m00" + std::to_string(m), "REFS M0" + std::to_string(m)});
            }
        }
        return out;
    }
    if (product.source == "IFS-ENS") {   // the single-member charts: the ensemble's 50 members, no mean (the mean and spread have charts of their own)
        if (product.id.compare(0, 7, "member_") == 0) {
            for (int m = 1; m <= 50; m++) {
                out.push_back({std::string{m < 10 ? "e0" : "e"} + std::to_string(m), "Member " + std::to_string(m)});
            }
        }
        return out;
    }
    if (product.source == "GEFS" || product.source == "GEPS") {
        // the charts made of the mean's fields alone: a spread among the wants (its lines, its derivations) has no one member's version
        for (const auto& w : wanted) {
            if (!w.stat.empty()) {
                return out;
            }
        }
        out.push_back({"", "Ensemble mean"});
        out.push_back({"c00", "Control"});
        for (int m = 1; m <= (product.source == "GEPS" ? 20 : 30); m++) {
            out.push_back({"p" + std::string{m < 10 ? "0" : ""} + std::to_string(m), "Member " + std::to_string(m)});
        }
    }
    return out;
}

Product withMember(const Product& product, const std::string& member, const std::string& runId) {
    if (member.empty()) {
        return product;
    }
    const auto choices = memberChoices(product, runId);
    const auto chosen = std::find_if(choices.begin(), choices.end(), [&member] (const MemberChoice& c) { return c.value == member; });
    if (chosen == choices.end()) {
        return product;
    }
    const bool refs = product.source == "REFS" || product.source == "SPCREFS";
    const bool ens = product.source == "IFS-ENS";
    const auto swap = [refs, ens, member] (GfsData::Want w) {
        if (ens ? (w.stat.size() > 1 && w.stat[0] == 'e') : refs ? w.stat.compare(0, 3, "m00") == 0 : w.stat.empty()) {
            w.stat = member;
        }
        return w;
    };
    Product out = product;
    for (auto& w : out.wants) {
        w = swap(w);
    }
    for (auto * f : {&out.needs, &out.fallbackNeeds}) {
        if (*f) {
            *f = [original = *f, swap] (int hour) {
                auto list = original(hour);
                for (auto& need : list) {
                    need.want = swap(need.want);
                }
                return list;
            };
        }
    }
    const auto named = [&chosen] (const std::string& text) {   // "Mean 500mb Height" -> "Member 7 500mb Height"; the rest gets the member in front
        return text.compare(0, 5, "Mean ") == 0 ? chosen->label + text.substr(4) : chosen->label + ": " + text;
    };
    out.label = refs || ens ? chosen->label + ": " + product.label.substr(product.label.find(": ") == std::string::npos ? 0 : product.label.find(": ") + 2) : named(product.label);
    if (!refs && product.fillTitle.compare(0, 5, "Mean ") == 0) {
        out.fillTitle = chosen->label + product.fillTitle.substr(4);
    }
    return out;
}

bool hasStandardDeviations(const Product& product) {
    return static_cast<bool>(product.sigma);
}

Product withPoolOnly(const Product& product, const std::string& source) {
    if (!product.pool.active() || source.empty()) {
        return product;
    }
    int count = 0;
    for (const auto& member : poolMembers()) {
        count += member.source == source ? 1 : 0;
    }
    const std::string name = source == "IFS-ENS" ? "ECMWF" : source;
    const std::string note = "only the " + std::to_string(count) + " members of the " + name;
    const auto retitle = [name, note] (std::string text) {
        for (const std::string phrase : {"% of the 102 members of the ECMWF, GEFS and GEPS together", "% of all the members, ECMWF, GEFS and GEPS"}) {
            const auto at = text.find(phrase);
            if (at != std::string::npos) {
                text.replace(at, phrase.size(), "% of " + note);
                return text;
            }
        }
        return text + " (" + note + ")";
    };
    Product out = product;
    out.pool.only = source;
    out.label = product.label + " - " + name + " only";
    if (!product.fillTitle.empty()) {
        out.fillTitle = retitle(product.fillTitle);
    }
    if (product.fillTitleFor) {
        const auto inner = product.fillTitleFor;
        out.fillTitleFor = [inner, retitle] (int hour) { return retitle(inner(hour)); };
    }
    return out;
}

Product withStandardDeviations(const Product& product) {
    if (!product.sigma) {
        return product;
    }
    Product out = product;
    out.derive = product.sigma;
    out.ramp = standardAnomaly();
    out.palette.clear();
    out.fillTitle = product.source == "CFS" || product.source == "CFS-ANL" ? "Anomaly from the CFS normal (1999-2010) in standard deviations of the 1991-2020 reanalysis" : "Anomaly from the 1991-2020 mean in standard deviations";
    out.legendStep = 1;
    out.label = product.label + " (std dev)";
    if (!product.sigmaReadKey.empty() && product.sigmaInterval > 0.0) {   // the plain anomaly beside the deviations: dashed, in its own color, so the three read together
        ContourSet anomaly;
        anomaly.key = product.sigmaReadKey;
        anomaly.interval = product.sigmaInterval;
        anomaly.title = "Anomaly (" + product.sigmaUnit + ")";
        anomaly.color = QColor{110, 60, 160};
        anomaly.dashed = true;
        anomaly.uniform = true;
        anomaly.width = 1.2;
        out.contours.push_back(anomaly);
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
        const bool already = !o->barbs && !o->windFill && std::any_of(base.contours.begin(), base.contours.end(), [o] (const ContourSet& c) { return c.title == o->contour.title; });
        if (!already) {
            used.push_back(o);
        }
    }
    if (used.empty()) {
        return base;
    }
    Product p = base;
    const bool seaLevelAsMslet = base.source.compare(0, 4, "RRFS") == 0 || base.source.compare(0, 4, "HRRR") == 0;   // the Rapid Refresh calls its sea level pressure MSLET, the HRRR MSLMA
    const char * seaLevelName = base.source.compare(0, 4, "HRRR") == 0 ? "MSLMA" : "MSLET";
    const auto add = [used, seaLevelAsMslet, seaLevelName] (std::vector<GfsData::Need> needs, int hour) {
        for (const auto * o : used) {
            for (auto need : o->needs(hour)) {
                need.want.key = overlayPrefix(*o) + need.want.key;
                if (seaLevelAsMslet && need.want.variable == "PRMSL") {
                    need.want.variable = seaLevelName;
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
        if (o->windFill) {   // the wind speed is the fill (only the last of these ticked counts)
            const std::string uKey = prefix + "u", vKey = prefix + "v";
            p.fill = [uKey, vKey] (const Grids& g) { return GfsGrid::scaled(GfsGrid::speed(pick(g, uKey.c_str()), pick(g, vKey.c_str())), msToKnots); };
            p.ramp = windSpeed();
            p.palette = o->fillPalette;
            p.fillTitle = o->fillText;
            p.fillTitleFor = nullptr;
            p.quantity = Quantity::Other;
            p.legendStep = 20;
            p.sigma = nullptr;
            p.overlays.clear();
        } else if (o->barbs) {   // each ticked wind is a set of its own: they stack on the chart's own barbs
            p.barbSets.push_back({prefix + o->barbU, prefix + o->barbV, o->barbColor, o->label, o->barbColorName});
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
            case Quantity::TemperatureDifference: return value * 1.8;
            case Quantity::Millimeters: return value / 25.4;
            case Quantity::Centimeters: return value / 2.54;
            case Quantity::Meters: return value * 3.28084;   // wave heights in feet
            default: return value;
        }
    }
    QString unitText(Quantity quantity, bool us) {
        switch (quantity) {
            case Quantity::Temperature: return us ? "°F" : "°C";
            case Quantity::TemperatureDifference: return us ? "°F" : "°C";
            case Quantity::Millimeters: return us ? "in" : "mm";
            case Quantity::Centimeters: return us ? "in" : "cm";
            case Quantity::Meters: return us ? "ft" : "m";
            default: return {};
        }
    }
    QString numberText(double rough) {
        const double value = std::round(rough * 1000.0) / 1000.0;   // 3.0000001 inches (from 76.2 millimeters) is 3
        const double a = std::abs(value);
        return QString::number(value, 'f', a >= 10 || a == std::floor(a) ? 0 : a >= 1 ? 1 : 2);
    }
}

GfsGrid::Grid fillOf(const Product& product, const Grids& fetched, const GfsData::Run& run, int forecastHour, const GfsClimate * climate) {
    Grids grids = fetched;
    for (auto& entry : grids) {
        GfsGrid::closeWrapGap(entry.second);
    }
    if (product.derive) {
        product.derive(grids, Context{forecastHour, run, climate, product.source});
    }
    return product.fill ? product.fill(grids) : GfsGrid::Grid{};
}

GfsGrid::Grid deltaInUserUnits(const GfsGrid::Grid& delta, Quantity quantity, bool us, std::string& unit) {
    double factor = 1.0;
    QString text = unitText(quantity, false);
    if (us) {
        switch (quantity) {
            case Quantity::Temperature: case Quantity::TemperatureDifference: factor = 1.8; break;
            case Quantity::Millimeters: factor = 1.0 / 25.4; break;
            case Quantity::Centimeters: factor = 1.0 / 2.54; break;
            case Quantity::Meters: factor = 3.28084; break;
            default: break;
        }
        text = unitText(quantity, true);
    }
    unit = text.toStdString();
    return factor == 1.0 ? delta : GfsGrid::scaled(delta, factor);
}

bool Probe::locate(double fx, double fy, double& lon, double& lat) const {
    const double x = fx * imageWidth, y = fy * imageHeight;
    if (x < areaLeft || x > areaLeft + areaWidth || y < areaTop || y > areaTop + areaHeight) {
        return false;
    }
    const Sector sector{"", west, south, east, north};   // View holds a reference: a temporary would be gone
    const View view{sector, QRectF{areaLeft, areaTop, areaWidth, areaHeight}};
    lon = view.lonAt(x);
    lat = view.latAt(y);
    while (lon > 180.0) {
        lon -= 360.0;
    }
    while (lon < -180.0) {
        lon += 360.0;
    }
    return true;
}

bool Probe::place(double lon, double lat, double& fx, double& fy) const {
    if (areaWidth <= 0.0 || imageWidth <= 0.0 || imageHeight <= 0.0) {
        return false;
    }
    const Sector sector{"", west, south, east, north};   // View holds a reference: a temporary would be gone
    const View view{sector, QRectF{areaLeft, areaTop, areaWidth, areaHeight}};
    const auto point = view.toPixel(lon, lat);
    if (point.x() < areaLeft || point.x() > areaLeft + areaWidth || point.y() < areaTop || point.y() > areaTop + areaHeight) {
        return false;
    }
    fx = point.x() / imageWidth;
    fy = point.y() / imageHeight;
    return true;
}

bool Probe::placeAnywhere(double lon, double lat, double& fx, double& fy) const {
    if (areaWidth <= 0.0 || imageWidth <= 0.0 || imageHeight <= 0.0) {
        return false;
    }
    const Sector sector{"", west, south, east, north};
    const View view{sector, QRectF{areaLeft, areaTop, areaWidth, areaHeight}};
    const auto point = view.toPixel(lon, lat);
    fx = point.x() / imageWidth;
    fy = point.y() / imageHeight;
    return true;
}

QString Probe::read(double fx, double fy) const {
    double lon = 0.0, lat = 0.0;
    if (!locate(fx, fy, lon, lat)) {
        return {};
    }
    const QChar degree{0x00B0};
    QStringList lines;
    lines << QString::number(std::abs(lat), 'f', 2) + degree + (lat >= 0.0 ? " N" : " S") + "   " + QString::number(std::abs(lon), 'f', 2) + degree + (lon >= 0.0 ? " E" : " W");
    for (const auto& field : fields) {
        const float value = field.grid.sample(lon, lat);
        if (std::isnan(value)) {
            continue;
        }
        const double number = field.convert ? shown(field.quantity, value, us) : value * field.scale;
        lines << QString::fromStdString(field.label) + ": " + numberText(number);
    }
    if (!windU.empty() && !windV.empty()) {
        const double u = windU.sample(lon, lat), v = windV.sample(lon, lat);
        if (!std::isnan(u) && !std::isnan(v)) {
            const double speed = std::hypot(u, v) * 1.94384;
            const double from = std::fmod(std::atan2(-u, -v) * 180.0 / pi + 360.0, 360.0);
            lines << "Wind: " + QString::number(speed, 'f', 0) + " kt from " + QString::number(from, 'f', 0) + degree;
        }
    }
    return lines.join("\n");
}

double shownValue(Quantity quantity, double value, bool us) {
    return shown(quantity, value, us);
}

double storedValue(Quantity quantity, double value, bool us) {
    if (!us) {
        return value;
    }
    switch (quantity) {
        case Quantity::Temperature: return (value - 32.0) / 1.8;
        case Quantity::TemperatureDifference: return value / 1.8;
        case Quantity::Millimeters: return value * 25.4;
        case Quantity::Centimeters: return value * 2.54;
        case Quantity::Meters: return value / 3.28084;
        default: return value;
    }
}

std::string unitLabel(Quantity quantity, bool us) {
    return unitText(quantity, us).toStdString();
}

Ramp defaultRamp(const Product& product, bool magColors) {
    if (magColors && !product.palette.empty()) {   // the model guidance site's color bands in place of ours
        if (const auto * bands = magPalette(product.palette)) {
            return *bands;
        }
    }
    return product.ramp;
}

std::string rampKey(const Ramp& ramp) {
    if (ramp.stops.empty()) {
        return {};
    }
    QByteArray text = ramp.banded ? "b" : "s";
    for (const auto& stop : ramp.stops) {
        text += ":" + QByteArray::number(stop.first, 'g', 8);
    }
    return "r" + std::to_string(ramp.stops.size()) + "_" + QByteArray::number(qHash(text), 16).toStdString();
}

std::string rampToText(const Ramp& ramp) {
    std::string out = std::string{"banded "} + (ramp.banded ? "1" : "0") + "\n";
    for (const auto& [value, color] : ramp.stops) {
        out += QByteArray::number(value, 'g', 10).toStdString() + " " + std::to_string(color.red()) + " " + std::to_string(color.green()) + " " + std::to_string(color.blue()) + " " + std::to_string(color.alpha()) + "\n";
    }
    return out;
}

bool rampFromText(const std::string& text, Ramp& ramp, std::string& error) {
    Ramp parsed;
    int lineNumber = 0;
    for (const auto& raw : QString::fromStdString(text).split('\n')) {
        lineNumber++;
        const auto line = raw.trimmed();
        if (line.isEmpty() || line.startsWith('#')) {
            continue;
        }
        const auto parts = line.split(QRegularExpression{"[\\s,;]+"}, Qt::SkipEmptyParts);
        if (parts[0].compare("banded", Qt::CaseInsensitive) == 0) {
            parsed.banded = parts.size() > 1 && parts[1] != "0" && parts[1].compare("false", Qt::CaseInsensitive) != 0;
            continue;
        }
        bool okValue = false;
        const double value = parts[0].toDouble(&okValue);
        int channel[4] = {0, 0, 0, 255};
        bool okChannels = parts.size() >= 4 && parts.size() <= 5;
        for (int c = 0; okChannels && c < parts.size() - 1; c++) {
            bool ok = false;
            channel[c] = parts[c + 1].toInt(&ok);
            okChannels = ok && channel[c] >= 0 && channel[c] <= 255;
        }
        if (!okValue || !okChannels) {
            error = "line " + std::to_string(lineNumber) + " is not \"value red green blue [alpha]\": " + line.toStdString();
            return false;
        }
        parsed.stops.emplace_back(value, QColor{channel[0], channel[1], channel[2], channel[3]});
    }
    if (parsed.stops.size() < 2) {
        error = "a scale needs at least two stops";
        return false;
    }
    std::stable_sort(parsed.stops.begin(), parsed.stops.end(), [] (const auto& a, const auto& b) { return a.first < b.first; });
    ramp = std::move(parsed);
    return true;
}

QImage render(const Product& drawn, const Sector& sector, const Grids& fetched, const GfsData::Run& run, int forecastHour, const Options& options) {
    const bool timing = std::getenv("WXQT_TIMING") != nullptr;   // WXQT_TIMING=1: the seconds each stage of the drawing takes, to stderr
    auto lapAt = std::chrono::steady_clock::now();
    const auto lap = [&] (const char * what) {
        if (timing) {
            const auto now = std::chrono::steady_clock::now();
            std::fprintf(stderr, "render %-14s %.2f s\n", what, std::chrono::duration<double>(now - lapAt).count());
            lapAt = now;
        }
    };
    Product product = drawn;
    product.ramp = defaultRamp(drawn, options.magColors);
    if (const auto found = options.palettes.find(rampKey(product.ramp)); found != options.palettes.end()) {   // the user's own scale for this kind of chart
        product.ramp = found->second;
    }
    if (product.ramp.stops.empty()) {   // a chart of lines only has no scale: a clear one so nothing reads an empty list
        product.ramp.stops = {{0.0, QColor{0, 0, 0, 0}}, {1.0, QColor{0, 0, 0, 0}}};
    }
    Grids grids = fetched;
    for (auto& entry : grids) {
        GfsGrid::closeWrapGap(entry.second);
    }
    if (product.derive) {
        product.derive(grids, Context{forecastHour, run, options.climate, product.source});
    }
    auto fill = product.fill ? product.fill(grids) : GfsGrid::Grid{};
    if (!options.fillOverride.empty()) {   // a maximum or a change: the fill is made, and only it is drawn
        fill = options.fillOverride;
        const auto title = product.fillTitleFor ? product.fillTitleFor(forecastHour) : product.fillTitle;
        product.fillTitle = options.overrideNote + (title.empty() ? "" : ": " + title);
        product.fillTitleFor = nullptr;
        product.contours.clear();
        product.barbU.clear();
        product.barbV.clear();
        product.barbSets.clear();
        product.streamU.clear();
        product.streamV.clear();
        product.overlays.clear();
        product.linesOnly = false;
        if (!options.overrideRamp.stops.empty()) {
            product.ramp = options.overrideRamp;
            product.palette.clear();
        }
        product.quantity = options.overrideQuantity;
    }
    if (product.linesOnly && !product.contours.empty()) {   // nothing to fill: a blank grid of the first line's field
        const auto found = grids.find(product.contours.front().key);
        if (found != grids.end()) {
            fill = found->second;
            std::fill(fill.values.begin(), fill.values.end(), std::nanf(""));
        }
    }
    if (fill.empty()) {
        return {};
    }
    // the wind the hover and a map layer take: the chart's own, else the first ticked
    const std::string windU = !product.barbU.empty() ? product.barbU : (product.barbSets.empty() ? std::string{} : product.barbSets.front().u);
    const std::string windV = !product.barbU.empty() ? product.barbV : (product.barbSets.empty() ? std::string{} : product.barbSets.front().v);
    if (options.fillOut != nullptr) {
        *options.fillOut = fill;
        if (options.windUOut != nullptr && options.windVOut != nullptr && !windU.empty() && grids.count(windU) && grids.count(windV)) {
            *options.windUOut = grids.at(windU);
            *options.windVOut = grids.at(windV);
        }
        if (options.contoursOut != nullptr) {
            for (const auto& set : product.contours) {
                if (const auto found = grids.find(set.key); found != grids.end()) {
                    options.contoursOut->push_back({set, found->second});
                }
            }
        }
        if (options.rampOut != nullptr) {
            *options.rampOut = product.ramp;
        }
        return QImage{1, 1, QImage::Format_ARGB32_Premultiplied};
    }
    for (const auto& set : product.contours) {
        if (grids.find(set.key) == grids.end()) {
            return {};
        }
    }
    const bool us = options.fahrenheit;
    // a chart in standard deviations says how big one deviation is in the plain unit over the view: it differs from place to place, so a range (the 10th to 90th percentile), not a number
    double deviationLow = 0.0, deviationHigh = 0.0;
    if (!product.sigmaSizeKey.empty() && grids.count(product.sigmaSizeKey) && !product.linesOnly) {
        auto values = GfsGrid::cropped(grids.at(product.sigmaSizeKey), sector.west, sector.south, sector.east, sector.north).values;
        values.erase(std::remove_if(values.begin(), values.end(), [] (float v) { return !std::isfinite(v); }), values.end());
        if (values.size() >= 10) {
            std::sort(values.begin(), values.end());
            deviationLow = values[values.size() / 10];
            deviationHigh = values[values.size() * 9 / 10];
        }
    }
    const int headerHeight = 54, legendHeight = 62 + (deviationHigh > 0.0 ? 14 : 0), margin = 8;
    const QRectF mapArea{static_cast<double>(margin), static_cast<double>(headerHeight), static_cast<double>(options.width - 2 * margin), 0.0};
    const double mapHeight = std::clamp(View::heightFor(sector, mapArea.width()), 200.0, 1400.0);
    // a larger picture of the same design: everything is placed in the 1x picture's pixels, the painter scales it, the fill is computed at the final pixels. A tall area at a large scale is a
    // great many pixels (a picture of South America 6600 wide is 60 million, 650 MB while it is made): the scale is held to about 40 million pixels
    const double S = std::clamp(options.scale, 1.0, std::max(1.0, std::sqrt(40e6 / (static_cast<double>(options.width) * (headerHeight + mapHeight + legendHeight)))));
    const QRectF area{mapArea.left(), mapArea.top(), mapArea.width(), mapHeight};
    View view{sector, area};
    QImage image(static_cast<int>(std::lround(options.width * S)), static_cast<int>(std::lround((headerHeight + mapHeight + legendHeight) * S)), QImage::Format_ARGB32_Premultiplied);
    image.fill(QColor{250, 250, 250});

    if (options.probe) {   // the fields of this chart, kept for the hover read-out
        auto& probe = *options.probe;
        probe = {};
        probe.west = sector.west;
        probe.east = sector.east;
        probe.south = sector.south;
        probe.north = sector.north;
        probe.areaLeft = area.left() * S;
        probe.areaTop = area.top() * S;
        probe.areaWidth = area.width() * S;
        probe.areaHeight = area.height() * S;
        probe.imageWidth = image.width();
        probe.imageHeight = image.height();
        probe.us = us;
        probe.validUtc = QDateTime::fromString(QString::fromStdString(run.id()), "yyyyMMddHH").addSecs(static_cast<qint64>(forecastHour) * 3600);
        probe.validUtc.setTimeSpec(Qt::UTC);
        if (!product.linesOnly) {
            QString title = QString::fromStdString(product.fillTitleFor ? product.fillTitleFor(forecastHour) : product.fillTitle);
            if (const auto unit = unitText(product.quantity, us); !unit.isEmpty()) {
                title += " (" + unit + ")";
            }
            probe.fields.push_back({title.toStdString(), GfsGrid::reducedTo(GfsGrid::cropped(fill, sector.west, sector.south, sector.east, sector.north), 120000), 1.0, product.quantity, true});
        }
        for (const auto& set : product.contours) {
            probe.fields.push_back({set.title.empty() ? set.key : set.title, GfsGrid::reducedTo(GfsGrid::cropped(grids.at(set.key), sector.west, sector.south, sector.east, sector.north), 120000), set.scale, Quantity::Other, false});
        }
        for (const auto& value : product.pointValues) {
            if (grids.count(value.key)) {
                probe.fields.push_back({value.title.empty() ? value.key : value.title, GfsGrid::reducedTo(GfsGrid::cropped(grids.at(value.key), sector.west, sector.south, sector.east, sector.north), 120000), 1.0, product.quantity, true});
            }
        }
        if (!windU.empty() && grids.count(windU) && grids.count(windV)) {
            probe.windU = GfsGrid::reducedTo(GfsGrid::cropped(grids.at(windU), sector.west, sector.south, sector.east, sector.north), 120000);
            probe.windV = GfsGrid::reducedTo(GfsGrid::cropped(grids.at(windV), sector.west, sector.south, sector.east, sector.north), 120000);
        }
    }
    // the fill, a pixel at a time (the ramp is in the grid's units; fully clear parts show the pale ground)
    const Ramp * ramp = &product.ramp;
    QImage map(static_cast<int>(std::lround(area.width() * S)), static_cast<int>(std::lround(area.height() * S)), QImage::Format_ARGB32_Premultiplied);
    map.fill(QColor{244, 244, 244});
    for (int y = 0; y < map.height(); y++) {
        auto * row = reinterpret_cast<QRgb *>(map.scanLine(y));
        const double lat = view.latAt(area.top() + (y + 0.5) / S);
        for (int x = 0; x < map.width(); x++) {
            const float value = fill.sample(view.lonAt(area.left() + (x + 0.5) / S), lat);
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
    lap("fill");
    for (const auto& [key, overlayRamp] : product.overlays) {
        const auto found = grids.find(key);
        if (found == grids.end()) {
            continue;
        }
        for (int y = 0; y < map.height(); y++) {
            auto * row = reinterpret_cast<QRgb *>(map.scanLine(y));
            const double lat = view.latAt(area.top() + (y + 0.5) / S);
            for (int x = 0; x < map.width(); x++) {
                const float value = found->second.sample(view.lonAt(area.left() + (x + 0.5) / S), lat);
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
    p.scale(S, S);
    p.drawImage(area, map);
    p.setClipRect(area);

    lap("overlays");
    // coastlines and borders
    p.setPen(QPen{QColor{40, 40, 40, 210}, 0.9});
    for (const auto& line : options.lines) {
        QPainterPath path;
        bool started = false;
        double previous = 0.0;
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
            if (started && std::abs(lon - previous) > 180.0) {   // a coast that crosses the date line (Chukotka, Fiji, the Antarctic ring): not a line across the whole chart
                started = false;
            }
            previous = lon;
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

    lap("coastlines");
    // the wider the view, the sparser the lines, the highs and lows, and the barbs
    const double span = sector.east - sector.west;
    const double extremeRadius = span > 150.0 ? 12.0 : span > 80.0 ? 8.0 : 6.0;
    std::vector<QRectF> placed;   // where the numbers on the lines already are
    for (const auto& set : product.contours) {
        const double interval = set.interval * (span > 150.0 ? 2.0 : 1.0);
        GfsGrid::Grid smooth;
        if (set.smoothKm > 0.0) {   // a 3 x 3 average n times spreads as sqrt(2n/3) cells: the passes that reach the radius (at most 60)
            const double cells = set.smoothKm / (std::max(grids.at(set.key).step, 0.001) * 111.0);
            smooth = GfsGrid::smoothed(grids.at(set.key), std::clamp(static_cast<int>(std::lround(1.5 * cells * cells)), 1, 60));
        }
        const GfsGrid::Grid fine = set.smoothKm > 0.0 ? GfsGrid::Grid{} : GfsGrid::reducedForLines(grids.at(set.key));   // a 3 km grid is averaged to about 8 km for its lines
        const auto& grid = set.smoothKm > 0.0 ? smooth : (fine.empty() ? grids.at(set.key) : fine);
        double lo = 1e18, hi = -1e18;
        for (int y = 0; y < map.height(); y += 6) {
            for (int x = 0; x < map.width(); x += 6) {
                const float value = grid.sample(view.lonAt(area.left() + x / S), view.latAt(area.top() + y / S));
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
            const bool heavy = !set.uniform && std::fmod(std::abs(level - set.base), interval * 5) < 1e-6 || (set.colorBelow.isValid() && std::abs(level - set.split) < 1e-6);
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
                if (options.vectorOut != nullptr) {   // handed over: the map draws the line (and its number) at its own zoom
                    VectorLines::Line out;
                    out.color = pen.color();
                    out.width = pen.widthF();
                    out.dashed = set.dashed;
                    for (const auto& pt : line) {
                        out.lonLat.emplace_back(pt.lon, pt.lat);
                    }
                    options.vectorOut->lines.push_back(std::move(out));
                } else {
                    p.setPen(pen);
                    p.drawPath(path);
                }
                // one label near the middle of a line long enough to carry it
                if (set.labelled && path.length() > 150.0) {
                    const auto mid = path.pointAtPercent(0.5);
                    const QString text = interval < 1.0 ? QString::number(level, 'f', interval < 0.1 ? 2 : 1) : QString::number(static_cast<int>(std::lround(level)));
                    const QRectF box{mid.x() - text.size() * 4.0 - 4.0, mid.y() - 9.0, text.size() * 8.0 + 8.0, 18.0};
                    const bool crowded = std::any_of(placed.begin(), placed.end(), [&box] (const QRectF& other) { return other.intersects(box); });   // a number over another is unreadable: skip it
                    if (!crowded && area.adjusted(24, 14, -24, -14).contains(mid)) {
                        placed.push_back(box);
                        if (options.vectorOut != nullptr) {
                            options.vectorOut->labels.push_back({view.lonAt(mid.x()), view.latAt(mid.y()), text, color.darker(130)});
                        } else {
                            halo(mid, interval < 1.0 ? QString::number(level, 'f', interval < 0.1 ? 2 : 1) : QString::number(static_cast<int>(std::lround(level))), color.darker(130));   // a fractional interval keeps its decimals
                        }
                    }
                }
            }
        }
        if (set.highsAndLows) {
            for (const auto& e : GfsGrid::extremes(grid, extremeRadius, sector.west, sector.south, sector.east, sector.north)) {
                const auto at = view.toPixel(e.lon, e.lat);
                if (area.adjusted(20, 20, -20, -20).contains(at)) {
                    if (options.vectorOut != nullptr) {
                        options.vectorOut->labels.push_back({e.lon, e.lat, e.high ? "H" : "L", e.high ? QColor{20, 60, 170} : QColor{190, 30, 30}});
                        options.vectorOut->labels.push_back({e.lon, view.latAt(at.y() + 13), QString::number(static_cast<int>(std::lround(e.value * set.scale))), QColor{40, 40, 40}});
                    } else {
                        halo(at, e.high ? "H" : "L", e.high ? QColor{20, 60, 170} : QColor{190, 30, 30});
                        halo(at + QPointF{0, 13}, QString::number(static_cast<int>(std::lround(e.value * set.scale))), QColor{40, 40, 40});
                    }
                }
            }
        }
    }

    lap("contours");
    // wind barbs on a grid of pixels
    if (!options.noBarbs) {
        struct DrawnSet {
            const GfsGrid::Grid * u;
            const GfsGrid::Grid * v;
            QColor color;
        };
        std::vector<DrawnSet> sets;
        if (!product.barbU.empty() && grids.count(product.barbU) && grids.count(product.barbV)) {
            sets.push_back({&grids.at(product.barbU), &grids.at(product.barbV), QColor{20, 20, 20}});
        }
        for (const auto& set : product.barbSets) {
            if (grids.count(set.u) && grids.count(set.v)) {
                sets.push_back({&grids.at(set.u), &grids.at(set.v), set.color});
            }
        }
        const double pixelsPerDegree = area.width() / std::max(0.1, span);
        const auto look = BarbGrid::look(span, pixelsPerDegree);
        for (double lat = std::ceil(std::max(-85.0, sector.south) / look.step) * look.step; !sets.empty() && lat <= std::min(85.0, sector.north); lat += look.step) {
            const double lonStep = BarbGrid::lonStep(look.step, lat);
            for (double lon = std::ceil(sector.west / lonStep) * lonStep; lon <= sector.east; lon += lonStep) {
                const QPointF at = view.toPixel(lon, lat);
                if (!area.contains(at)) {
                    continue;
                }
                // every set from the same origin: the first the longest, each next a little shorter and drawn over it
                for (size_t i = 0; i < sets.size(); i++) {
                    float uu = sets[i].u->sample(lon, lat), vv = sets[i].v->sample(lon, lat);
                    if (std::isnan(uu) || std::isnan(vv)) {
                        uu = sets[i].u->sample(lon + 360.0, lat);
                        vv = sets[i].v->sample(lon + 360.0, lat);
                    }
                    if (std::isnan(uu) || std::isnan(vv)) {
                        continue;
                    }
                    QColor ink = sets[i].color;
                    p.setPen(QPen{QColor{ink.red(), ink.green(), ink.blue(), 230}, sets.size() > 1 ? 1.3 : 1.0});
                    p.setBrush(ink);
                    const double from = std::fmod(std::atan2(-uu, -vv) * 180.0 / pi + 360.0, 360.0);
                    WindBarb::draw(p, at, from, std::hypot(uu, vv) * msToKnots, look.length * (1.0 - 0.12 * static_cast<double>(i)), lat < 0.0);
                }
            }
        }
    }
    // numbers at points ("41 63": the 10th and 90th percentile): at cities, biggest first, then evenly over what is left of the map; never one box on another
    if (!product.pointValues.empty()) {
        bool all = true;
        for (const auto& value : product.pointValues) {
            all = all && grids.count(value.key) > 0;
        }
        if (all) {
            QFont numberFont = font;
            numberFont.setBold(true);
            numberFont.setPointSizeF(std::max(7.0, font.pointSizeF() * 0.9));
            const QFontMetricsF numberMetrics{numberFont};
            const double gap = 5.0;   // between the numbers of one point
            std::vector<QRectF> boxes;
            // one point's numbers side by side, centred on `at`; false (nothing drawn) when no value there or the box would cover another
            const auto place = [&] (const QPointF& at, double lon, double lat) {
                std::vector<QString> texts;
                double width = -gap;
                for (const auto& value : product.pointValues) {
                    const float v = grids.at(value.key).sample(lon, lat);
                    if (std::isnan(v)) {
                        return false;
                    }
                    texts.push_back(QString::number(static_cast<int>(std::lround(shown(product.quantity, v, us)))));
                    width += numberMetrics.horizontalAdvance(texts.back()) + gap;
                }
                const QRectF box{at.x() - width / 2.0 - 3.0, at.y() - numberMetrics.height() / 2.0 - 2.0, width + 6.0, numberMetrics.height() + 4.0};
                if (!area.adjusted(4, 4, -4, -4).contains(box)) {
                    return false;
                }
                for (const auto& other : boxes) {
                    if (other.intersects(box.adjusted(-2, -1, 2, 1))) {
                        return false;
                    }
                }
                boxes.push_back(box);
                p.setPen(Qt::NoPen);
                p.setBrush(QColor{255, 255, 255, 150});
                p.drawRoundedRect(box, 3.0, 3.0);
                double x = at.x() - width / 2.0;
                for (size_t i = 0; i < texts.size(); i++) {
                    QPainterPath path;
                    path.addText(QPointF{x, at.y() + numberMetrics.ascent() / 2.0 - 1.0}, numberFont, texts[i]);
                    p.setPen(QPen{QColor{255, 255, 255, 230}, 2.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin});
                    p.setBrush(Qt::NoBrush);
                    p.drawPath(path);
                    p.setPen(Qt::NoPen);
                    p.setBrush(product.pointValues[i].color);
                    p.drawPath(path);
                    x += numberMetrics.horizontalAdvance(texts[i]) + gap;
                }
                return true;
            };
            // the cities (Options::places, biggest first) that fall inside the view
            std::vector<std::pair<float, float>> inView;
            for (const auto& city : options.places) {
                if (city.first >= sector.west && city.first <= sector.east && city.second >= sector.south && city.second <= sector.north) {
                    inView.push_back(city);
                }
            }
            const double ground = span > 150.0 ? 120.0 : span > 80.0 ? 96.0 : 80.0;   // pixels apart, at the least, so the wider the view the fewer
            std::vector<QPointF> spots;
            for (const auto& city : inView) {
                const auto at = view.toPixel(city.first, city.second);
                bool near = false;
                for (const auto& other : spots) {
                    near = near || std::hypot(at.x() - other.x(), at.y() - other.y()) < ground;
                }
                if (!near && place(at, city.first, city.second)) {
                    spots.push_back(at);
                }
                if (spots.size() >= 60) {
                    break;
                }
            }
            // what the cities left out (oceans, other countries): an even grid, skipping any spot near a number already there
            for (double y = area.top() + ground / 2.0; y < area.bottom(); y += ground) {
                for (double x = area.left() + ground / 2.0; x < area.right(); x += ground) {
                    bool near = false;
                    for (const auto& other : spots) {
                        near = near || std::hypot(x - other.x(), y - other.y()) < ground * 0.8;
                    }
                    if (!near && place({x, y}, view.lonAt(x), view.latAt(y))) {
                        spots.emplace_back(x, y);
                    }
                }
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
        if (now && options.windRadii) {   // 64 kt over 50 over 34, each quadrant's radius out along its quarter of the circle
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
    if (options.hasMarker) {   // the place a local area is round: a ring and its name
        const auto at = view.toPixel(options.markerLon, options.markerLat);
        if (area.contains(at)) {
            p.setClipRect(area);
            p.setPen(QPen{QColor{255, 255, 255}, 4.0});
            p.drawEllipse(at, 6.0, 6.0);
            p.setPen(QPen{QColor{200, 20, 20}, 2.0});
            p.drawEllipse(at, 6.0, 6.0);
            p.drawLine(QPointF{at.x() - 10, at.y()}, QPointF{at.x() + 10, at.y()});
            p.drawLine(QPointF{at.x(), at.y() - 10}, QPointF{at.x(), at.y() + 10});
            if (!options.markerName.empty()) {
                QFont f = p.font();
                f.setBold(true);
                p.setFont(f);
                const QString label = QString::fromStdString(options.markerName);
                p.setPen(QColor{255, 255, 255});
                for (const QPointF d : {QPointF{1, 1}, QPointF{-1, 1}, QPointF{1, -1}, QPointF{-1, -1}}) {
                    p.drawText(at + QPointF{10, -8} + d, label);
                }
                p.setPen(QColor{140, 10, 10});
                p.drawText(at + QPointF{10, -8}, label);
            }
            p.setClipping(false);
        }
    }
    if (options.ticks) {   // latitude and longitude marks along the frame: a step that gives five or six across, numbered along the bottom and the left
        const auto stepFor = [] (double span) {
            static const double steps[] = {0.1, 0.25, 0.5, 1.0, 2.0, 5.0, 10.0, 15.0, 30.0};
            for (const double step : steps) {
                if (span / step <= 7.0) {
                    return step;
                }
            }
            return 30.0;
        };
        const auto numberOf = [] (double v, double step, const char * plus, const char * minus) {
            const double a = std::fabs(v);
            return QString::number(a, 'f', step < 1.0 ? (step < 0.25 ? 2 : 1) : 0) + QString::fromUtf8("\xC2\xB0") + (v >= 0 ? plus : minus);
        };
        QFont f = p.font();
        f.setBold(false);
        f.setPixelSize(11);
        p.setFont(f);
        p.setPen(QColor{40, 40, 40});
        const double lonStep = stepFor(sector.east - sector.west);
        for (double lon = std::ceil(sector.west / lonStep) * lonStep; lon <= sector.east + 1e-9; lon += lonStep) {
            const double x = view.toPixel(lon, sector.south).x();
            if (x < area.left() + 18 || x > area.right() - 18) {
                continue;
            }
            for (const double y : {area.top(), area.bottom()}) {
                const double dir = y == area.top() ? 1.0 : -1.0;
                p.drawLine(QPointF{x, y}, QPointF{x, y + dir * 7.0});
            }
            double shownLon = lon;
            if (shownLon > 180.0) {
                shownLon -= 360.0;
            }
            p.drawText(QRectF{x - 30, area.bottom() - 22, 60, 14}, Qt::AlignHCenter, numberOf(shownLon, lonStep, "E", "W"));
        }
        const double latStep = stepFor(sector.north - sector.south);
        for (double lat = std::ceil(sector.south / latStep) * latStep; lat <= sector.north + 1e-9; lat += latStep) {
            const double y = view.toPixel(sector.west, lat).y();
            if (y < area.top() + 10 || y > area.bottom() - 10) {
                continue;
            }
            for (const double x : {area.left(), area.right()}) {
                const double dir = x == area.left() ? 1.0 : -1.0;
                p.drawLine(QPointF{x, y}, QPointF{x + dir * 7.0, y});
            }
            p.drawText(QRectF{area.left() + 9, y - 7, 60, 14}, Qt::AlignLeft | Qt::AlignVCenter, numberOf(lat, latStep, "N", "S"));
        }
        p.setPen(QPen{QColor{60, 60, 60}, 1.0});
    }

    // header
    const auto init = QDateTime::fromString(QString::fromStdString(run.id()), "yyyyMMddHH");
    auto initUtc = QDateTime{init.date(), init.time(), Qt::UTC};
    const auto valid = initUtc.addSecs(forecastHour * 3600);
    QFont big = font;
    big.setPixelSize(17);
    const QString headline = QString::fromStdString(product.source + "  " + product.label);
    {   // a long title (a chart with several things ticked onto it) gets a smaller type, down to 11, so that it stays clear of the run and valid times at the right
        const double room = image.width() / S - 2.0 * margin - 330.0;
        while (big.pixelSize() > 11 && QFontMetricsF{big}.horizontalAdvance(headline) > room) {
            big.setPixelSize(big.pixelSize() - 1);
        }
    }
    p.setFont(big);
    p.setPen(QColor{25, 25, 25});
    p.drawText(QPointF{static_cast<double>(margin), 22.0}, headline);
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
    QString barbText = ";  barbs: kt";
    if (!product.barbSets.empty()) {   // stacked winds: which color is which level
        QStringList names;
        if (!product.barbU.empty()) {
            names << "chart's own (black)";
        }
        for (const auto& set : product.barbSets) {
            names << QString::fromStdString(set.name + " (" + set.colorName + ")");
        }
        barbText = ";  barbs, kt: " + names.join(", ");
    }
    p.drawText(QPointF{static_cast<double>(margin), 43.0}, fillTitle + (lines.isEmpty() ? "" : (fillTitle.isEmpty() ? "lines: " : ";  lines: ") + lines.join(", ")) + (!product.barbU.empty() || !product.barbSets.empty() ? barbText : QString{}));
    const QString times = "Run " + initUtc.toString("ddd yyyy-MM-dd HH") + "Z   F" + QString::number(forecastHour).rightJustified(3, '0') + "   Valid " + valid.toString("ddd yyyy-MM-dd HH") + "Z";
    p.drawText(QRectF{0, 8, image.width() / S - static_cast<double>(margin), 20}, Qt::AlignRight, times);

    // legend: even steps in value, or (legendStep 0) one equal-width block per ramp stop
    const double barLeft = margin + 6.0, barTop = area.bottom() + 12.0, barWidth = area.width() - 12.0, barHeight = 16.0;
    const auto& stops = ramp->stops;
    const double first = stops.front().first, last = stops.back().first;
    const bool stepped = product.legendStep <= 0.0;
    const auto valueAt = [&] (double t) {   // t 0..1 along the bar
        if (ramp->banded) {   // one flat block for each band
            return stops[std::min(static_cast<size_t>(t * static_cast<double>(stops.size())), stops.size() - 1)].first;
        }
        if (!stepped) {
            return first + (last - first) * t;
        }
        const double position = t * static_cast<double>(stops.size() - 1);
        const size_t i = std::min(static_cast<size_t>(position), stops.size() - 2);
        return stops[i].first + (stops[i + 1].first - stops[i].first) * (position - static_cast<double>(i));
    };
    for (int x = 0; !product.linesOnly && x < static_cast<int>(barWidth); x++) {
        const QRgb c = ramp->at(valueAt(x / barWidth));
        // a ramp that begins clear is drawn as the pale ground on the bar
        const int a = qAlpha(c);
        const QColor ground{244, 244, 244};
        p.fillRect(QRectF{barLeft + x, barTop, 1.5, barHeight}, QColor{(qRed(c) * a + ground.red() * (255 - a)) / 255, (qGreen(c) * a + ground.green() * (255 - a)) / 255, (qBlue(c) * a + ground.blue() * (255 - a)) / 255});
    }
    p.setPen(QColor{60, 60, 60});
    p.setBrush(Qt::NoBrush);
    if (!product.linesOnly) {
        p.drawRect(QRectF{barLeft, barTop, barWidth, barHeight});
    } else if (product.contours.size() > 1) {   // lines of several sets (models compared): a key, a sample of each line's color and what it is
        double x = barLeft;
        for (const auto& set : product.contours) {
            if (set.title.empty()) {   // a line of many alike (a member of fifty) is not in the key
                continue;
            }
            const QString name = QString::fromStdString(set.title);
            const double width = QFontMetricsF{p.font()}.horizontalAdvance(name);
            p.setPen(QPen{set.color, 3.0});
            p.drawLine(QPointF{x, barTop + barHeight / 2.0}, QPointF{x + 26.0, barTop + barHeight / 2.0});
            p.setPen(QColor{60, 60, 60});
            p.drawText(QRectF{x + 32.0, barTop, width + 4.0, barHeight}, Qt::AlignLeft | Qt::AlignVCenter, name);
            x += 32.0 + width + 22.0;
        }
    } else {   // the lines are the chart: a line of explanation where the color bar would be
        p.drawText(QRectF{barLeft, barTop, barWidth, barHeight}, Qt::AlignLeft | Qt::AlignVCenter, QString::fromStdString(product.fillTitle));
    }
    const auto tick = [&] (double t, double value) {
        const double x = barLeft + t * barWidth;
        p.drawLine(QPointF{x, barTop + barHeight}, QPointF{x, barTop + barHeight + 4});
        p.drawText(QRectF{x - 24, barTop + barHeight + 4, 48, 14}, Qt::AlignHCenter, numberText(shown(product.quantity, value, us)));
    };
    if (product.linesOnly) {
        // no scale to number
    } else if (ramp->banded) {   // the number at the lower edge of each band, as the model guidance site's legends have it
        for (size_t i = 0; i < stops.size(); i++) {
            tick(static_cast<double>(i) / static_cast<double>(stops.size()), stops[i].first);
        }
    } else if (stepped) {
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
    if (deviationHigh > 0.0) {   // a line under the ticks: how big one deviation is here (it is not the same everywhere)
        const auto number = [] (double v) { return QString::number(v, 'f', v < 10.0 ? 1 : 0); };
        p.setPen(QColor{90, 90, 90});
        const QString unitName = QString::fromStdString(product.sigmaUnit);
        const QString low = number(deviationLow), high = number(deviationHigh);
        const QString text = low == high ? "1 std dev = about " + high + " " + unitName
                                         : "1 std dev = " + low + " to " + high + " " + unitName + " over this map (it varies by place; hover for the value here)";
        p.drawText(QRectF{barLeft, barTop + barHeight + 18.0, barWidth, 14}, Qt::AlignLeft, text);
    }
    // a small key for each overlay's colors (the same amounts as the main scale, from light to heavy)
    double keyLeft = barLeft;
    const bool paintball = product.source == "SPCREFS" && !product.overlays.empty() && product.overlays.front().first.rfind("Member ", 0) == 0;
    if (paintball) {   // the members' colors, each with its name (the old REFS screen's key): the first is the chart's own fill
        const auto names = UtilitySpcRefs::memberNames();
        std::vector<QColor> colors{product.ramp.stops.back().second};
        for (const auto& overlay : product.overlays) {
            colors.push_back(overlay.second.stops.back().second);
        }
        const double top = image.height() / S - 15.0;
        const QFontMetricsF metrics{p.font()};
        p.setPen(QColor{70, 70, 70});
        for (size_t i = 0; i < colors.size(); i++) {
            const QString name = i < names.size() ? QString::fromStdString(names[i]) : QString{"Member %1"}.arg(i + 1);
            p.setBrush(QColor{colors[i].red(), colors[i].green(), colors[i].blue()});
            p.setPen(QColor{60, 60, 60});
            p.drawRect(QRectF{keyLeft, top, 9, 9});
            p.setPen(QColor{70, 70, 70});
            p.drawText(QRectF{keyLeft + 13, top - 1, metrics.horizontalAdvance(name) + 4, 12}, Qt::AlignLeft | Qt::AlignVCenter, name);
            keyLeft += 13 + metrics.horizontalAdvance(name) + 14;
        }
        p.setBrush(Qt::NoBrush);
    }
    for (const auto& [key, overlayRamp] : product.overlays) {
        if (paintball) {
            break;
        }
        const QString name = key == "snow" ? "Snow" : key == "mix" ? "Mixed" : QString::fromStdString(key);
        const double top = image.height() / S - 15.0, width = 80.0;
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
    p.drawText(QRectF{0, image.height() / S - 16.0, image.width() / S - static_cast<double>(margin), 14}, Qt::AlignRight, QString::fromStdString("Data: " + sourceLabel(product.source)));
    return image;
}
}
