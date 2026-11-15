// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

// The ingredients of a pattern (CAPE over so much, shear over so much ...) as conditions: for a single run, where each point meets how many of them; for an ensemble, the share of the members that meet all of them together.
#include "gfs/GfsChartInternal.h"

namespace GfsChart {
namespace detail {

// ---- ingredients: the conditions of a pattern (CAPE over so much, shear over so much ...) as parts, for the "ingredients" charts: on a single run, where each point meets how many of them; on the
// ensemble, the share of the members that meet all of them together
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

}  // namespace detail
}  // namespace GfsChart
