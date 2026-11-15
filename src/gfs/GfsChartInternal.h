// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef GFSCHARTINTERNAL_H
#define GFSCHARTINTERNAL_H

#include <functional>
#include <cmath>
#include <functional>
#include <map>
#include <string>
#include <vector>
#include "gfs/GfsChart.h"
#include "gfs/GfsModels.h"

// What the files that make the chart catalogue share (GfsChart.cpp and the GfsProducts*.cpp, one for each family of models): small helpers, and the functions that add a family's charts to the
// list. Not for use outside the chart code.
namespace GfsChart {
namespace detail {

inline constexpr double pi = 3.14159265358979323846;
inline constexpr double msToKnots = 1.943844;

// a grid of the fetched ones by its key (an empty grid when it was not fetched)
inline GfsGrid::Grid pick(const Grids& g, const std::string& key) {
    const auto found = g.find(key);
    return found == g.end() ? GfsGrid::Grid{} : found->second;
}

// a record to fetch, by the key it is kept under
inline GfsData::Want want(const std::string& key, const std::string& variable, const std::string& level) {
    return {key, variable, level, ""};
}

// the speed of two components (m/s), in knots, as a chart's fill
inline std::function<GfsGrid::Grid(const Grids&)> speedOf(const char * u, const char * v) {
    return [u, v] (const Grids& g) { return GfsGrid::scaled(GfsGrid::speed(pick(g, u), pick(g, v)), msToKnots); };
}

// the 500 mb style height contours in decameters (grid "z"), dark or a little lighter
inline ContourSet heights(double interval, bool dark = true) {
    ContourSet c;
    c.key = "z";
    c.scale = 0.1;
    c.interval = interval;
    c.title = "Height (dam)";
    c.color = dark ? QColor{30, 30, 30} : QColor{60, 60, 60};
    return c;
}

// the sea level pressure contours in millibars (grid "p"), with the highs and lows
inline ContourSet pressure() {
    ContourSet c;
    c.key = "p";
    c.scale = 0.01;
    c.interval = 4;
    c.title = "Sea level pressure (mb)";
    c.highsAndLows = true;
    return c;
}

// the relative vorticity of the wind (grids "u" and "v") in 1e-5 per second
inline GfsGrid::Grid vorticityOf(const Grids& g) {
    return GfsGrid::scaled(GfsGrid::vorticity(GfsGrid::smoothed(pick(g, "u"), 1), GfsGrid::smoothed(pick(g, "v"), 1)), 1e5);
}

// Ertel potential vorticity on a pressure level, in PVU (1e-6 K m2 / kg s), from the temperature at the level and at the levels above (grid "ta", at pAbove mb) and below (grid "tb", at pBelow mb) it and
// the wind at the level (grids "u", "v"): PV = -g (zeta + f) dtheta/dp, zeta the relative vorticity on the pressure surface (the slope terms are left out: good in the stratosphere, where the surfaces
// are nearly flat). The southern hemisphere's sign is turned over, so the vortex of either pole is positive. The GFS files carry no PV on pressure levels (only the 2 PVU surface), so it is worked out.
// pAbove equal to the level's own pressure is a one-sided difference (the IFS open data has 10 and 50 mb, no 7 or 20). The stability is the gradient in log pressure divided by the level's own
// pressure, dtheta/dp = (dtheta/dln p) / p: the plain difference over the layer is the gradient at its middle, which for a wide layer in the stratosphere (theta rises like 1/p) is far too small
// (checked 2026-11-15 against GEOS-FP's own EPV at 10 mb: 458 PVU for the plain difference of 7 and 20 mb, 234 for 10 and 50, GEOS's own 718).
inline std::function<GfsGrid::Grid(const Grids&)> potentialVorticityOf(double pAbove, double pLevel, double pBelow) {
    return [=] (const Grids& g) {
        auto zeta = GfsGrid::vorticity(GfsGrid::smoothed(pick(g, "u"), 1), GfsGrid::smoothed(pick(g, "v"), 1));   // 1 / s
        {   // the vorticity is not worked out within about 1.25 degrees of a pole (cos latitude under 0.02: GfsGrid::vorticity leaves those rows clear), which drew a white disc at the pole: each of those rows
            // takes the mean of the nearest row that has a value (the ring just beside the cap)
            const auto rowMean = [&zeta] (int row) {
                double sum = 0.0;
                int count = 0;
                for (int c = 0; c < zeta.columns; c++) {
                    const float v = zeta.values[static_cast<size_t>(row) * static_cast<size_t>(zeta.columns) + static_cast<size_t>(c)];
                    if (!std::isnan(v)) {
                        sum += v;
                        count++;
                    }
                }
                return count > 0 ? sum / count : std::nan("");
            };
            const auto fillRows = [&zeta, &rowMean] (int from, int step) {   // from the pole inward until a row has values
                int edge = from;
                while (edge >= 0 && edge < zeta.rows && std::isnan(rowMean(edge))) {
                    edge += step;
                }
                if (edge < 0 || edge >= zeta.rows) {
                    return;
                }
                const auto mean = static_cast<float>(rowMean(edge));
                for (int row = from; row != edge; row += step) {
                    for (int c = 0; c < zeta.columns; c++) {
                        zeta.values[static_cast<size_t>(row) * static_cast<size_t>(zeta.columns) + static_cast<size_t>(c)] = mean;
                    }
                }
            };
            fillRows(0, 1);
            fillRows(zeta.rows - 1, -1);
        }
        const auto& ta = pick(g, "ta"), & tb = pick(g, "tb");
        constexpr double kappa = 0.2857, gravity = 9.80665, omega = 7.2921e-5, pi = 3.14159265358979;
        const double thetaAbove = std::pow(1000.0 / pAbove, kappa), thetaBelow = std::pow(1000.0 / pBelow, kappa);
        for (int r = 0; r < zeta.rows; r++) {
            const double lat = zeta.lat0 - r * zeta.step;
            const double f = 2.0 * omega * std::sin(lat * pi / 180.0), hemisphere = lat < 0.0 ? -1.0 : 1.0;
            for (int c = 0; c < zeta.columns; c++) {
                const size_t at = static_cast<size_t>(r) * static_cast<size_t>(zeta.columns) + static_cast<size_t>(c);
                const double dtheta = ((tb.values[at] + 273.15) * thetaBelow - (ta.values[at] + 273.15) * thetaAbove) / (std::log(pBelow / pAbove) * pLevel * 100.0);   // K / Pa
                zeta.values[at] = static_cast<float>(hemisphere * 1e6 * -gravity * (zeta.values[at] + f) * dtheta);
            }
        }
        return zeta;
    };
}

// the number without trailing zeros: 2, 0.25
std::string trimmed(double value);

// equivalent potential temperature (Bolton 1980) in K from temperature in C, relative humidity in percent and the pressure in hPa
double thetaE(double tC, double rh, double p);

// template id -> a function making the product for the chosen values (filled when the registry is built)
std::map<std::string, std::function<Product(const std::vector<double>&)>>& generators();

// the id of a chart made for chosen values: "gen~<template>~<value>~..."
std::string generatedId(const std::string& templateId, const std::vector<double>& values);

// the chance of an amount of precipitation: the ensemble whose members each have a running total since the start of the run, the scale of the chances, and the charts made from them
struct CumulativeEnsemble {
    std::string source;
    std::vector<std::string> members;
    std::function<std::string(int hour)> forecast;
};
const Ramp& exceedRamp();
Product exceedProduct(const CumulativeEnsemble& ensemble, const std::string& id, const std::string& label, double inches, int period);
Product exceedPooledProduct(const std::string& id, const std::string& label, double inches, int period);

// the ingredients of a pattern as conditions (GfsChartIngredients.cpp)
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
struct IngredientCondition {
    size_t ingredient;
    double limit;
};
GfsGrid::Grid combine2(const GfsGrid::Grid& a, const GfsGrid::Grid& b, const std::function<float(float, float)>& f);
GfsGrid::Grid mapped(const GfsGrid::Grid& a, const std::function<float(float)>& f);
const std::vector<Ingredient>& ingredients();
std::vector<IngredientCondition> ingredientConditions(bool ensemble, const std::string& source);
std::string ingredientNames(const std::vector<IngredientCondition>& chosen);
std::string conditionText(const IngredientCondition& c);
const std::vector<std::string>& ingredientSources();

// the color scales of the charts (GfsChartRamps.cpp)
Ramp temperature();
Ramp potentialVorticityRamp(double top);   // PVU, 0 to top
Ramp upperStratosphereTemperature();   // degrees C, -80 to +10 (5, 2 and 1 mb)
Ramp stratosphereTemperature();   // degrees C, -100 to -20
Ramp bands4F(const Ramp& base, double lowF, double highF);
Ramp windSpeed();
Ramp vorticityRamp();   // units of 1e-5 per second; the cyclonic side only
Ramp precipitation();   // millimeters
Ramp reflectivity();   // dBZ
Ramp humidity();   // percent
Ramp precipitableWater();   // millimeters
Ramp snowChange();   // centimeters; transparent where nothing changes
Ramp shearRamp();   // knots: calm to violent, the low shear tropical cyclones like in the pale greens
Ramp divergenceRamp();   // 1e-5 per second: convergence in blues, divergence in warm colors, little of either left clear
Ramp thetaERamp();   // kelvin
Ramp heightAnomaly();   // decameters: below normal in blues, above in warm colors, within 1.5 clear
Ramp standardAnomaly();   // standard deviations: within a quarter of one clear
Ramp temperatureDeparture();   // degrees C: below the climate in blues, above in warm colors, within half a degree clear
Ramp seaLevelPressure();   // millibars: lows in purples and blues, highs in yellows to reds
Ramp precipitationDeparture();   // millimeters in the period: drier than the climate in browns, wetter in greens, within a few clear
Ramp pressureAnomaly();   // millibars
Ramp dewpoint();   // degrees C: dry in the browns, humid in greens and blues
Ramp lowWind();   // knots, for the near-surface and 850 mb winds
Ramp capeRamp();   // J/kg
Ramp snowPrecipitation();   // millimeters of water
Ramp mixedPrecipitation();   // freezing rain and ice pellets
Ramp cloudCover();   // percent: clear sky pale, overcast a dark grey-blue
Ramp spreadRamp(double top);   // the spread of an ensemble, from none (white) up through yellow and orange to a deep purple at `top`
Ramp tropicalWind();   // knots: the Saffir-Simpson steps, tropical depression and storm in the cool colors, hurricanes warm to purple
Ramp satelliteIr();   // brightness temperature in kelvin: warm surfaces dark, cool cloud light grey, then the cold tops in color to the coldest in white
Ramp waterVapor();   // brightness temperature in kelvin: the moist, cold air in white through blue, dry air warm in orange to black
Ramp seaSurfaceTemperature();   // degrees C: fine steps through the range that matters to a storm (26 C is where the warm water starts)
Ramp airQuality(const std::vector<double>& steps);   // the six steps of the US air quality index (good, moderate, unhealthy for sensitive groups, unhealthy, very unhealthy, hazardous): the first step's value is where "good" ends
Ramp uvIndex();   // the WHO steps: low (green), moderate (yellow), high (orange), very high (red), extreme (violet)
Ramp currentSpeed();   // meters per second: slack water pale, the Gulf Stream and the Agulhas (1.5 and more) in strong colors
Ramp seaLevelHeight();   // meters above the mean: below in blues, above in warm colors
Ramp waveHeight();   // meters
Ramp helicityRamp();   // m2/s2: storm-relative helicity, the 150 and 300 steps are where rotation starts to matter
Ramp updraftHelicity();   // m2/s2: rotating updrafts, from 25 (weak) to 200 and more (a strong supercell)
Ramp lowLevelVorticity();   // 1e-3 per second: the strongest vertical vorticity of the last hour in the lowest 1-2 km; the weak values clear, a mesocyclone from about 10, a tornadic circulation from 20
Ramp echoTopRamp();   // thousands of feet
Ramp ceilingRamp();   // feet: the flight category steps (500 LIFR, 1000 IFR, 3000 MVFR) in the strong colors
Ramp visibilityRamp();   // statute miles: 1 and 3 are the IFR and MVFR steps
Ramp lightningRamp();   // flashes: any lightning is yellow, a lot is purple
Ramp wavePeriod();   // seconds: short choppy seas pale, long swell in deep colors
Ramp probability();   // percent: nothing under 5, then pale green to deep magenta
Ramp snowfall();   // centimeters of new snow, the steps at whole and simple inches (0.1, 1, 2, 4, 8, 12, 20, 30)

// the charts of one family of models, added to the list
void geosProducts(std::vector<Product>& p);

void sstProducts(std::vector<Product>& p, const char * model);

void camsProducts(std::vector<Product>& p, const char * model);

void mfWaveProducts(std::vector<Product>& p, const char * model);

void currentProducts(std::vector<Product>& p, const char * model);

void waveProducts(std::vector<Product>& p, const char * model);

void hafsProducts(std::vector<Product>& p, const char * model);

void gepsProducts(std::vector<Product>& p);

void spcProducts(std::vector<Product>& p, const char * source, bool gefs);

void mesoProducts(std::vector<Product>& p, const char * source);

void openMeteoProducts(std::vector<Product>& p, const std::string& source);

void pppProducts(std::vector<Product>& p);

void cfs30Products(std::vector<Product>& p);

void spcRefsProducts(std::vector<Product>& p);

void ensProducts(std::vector<Product>& p);

void ensAllProducts(std::vector<Product>& p);

void hrefProducts(std::vector<Product>& p);

void gefsProducts(std::vector<Product>& p);

void rrfsProducts(std::vector<Product>& p, const std::string& model);

void refsProducts(std::vector<Product>& p);

void nbmProducts(std::vector<Product>& p);

// (plainPrecip: the GFS precipitation charts before the lines are put on them, for the models whose maps have none)
void cloneFrom(std::vector<Product>& p, const GfsModels::Def& def, const std::map<std::string, Product>& plainPrecip);

// the GFS charts; plainPrecip is filled with the precipitation charts before the lines are put on them
void gfsProducts(std::vector<Product>& p, std::map<std::string, Product>& plainPrecip);

}  // namespace detail
}  // namespace GfsChart

#endif
