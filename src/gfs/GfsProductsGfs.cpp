// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

// The GFS charts: the upper air, the surface, the tropical set, the anomalies, the precipitation (by period, by type, by rate) and the quick one-record ones. Other models that read like the GFS are
// cloned from these (GfsProductsClones.cpp).
#include "gfs/GfsChartInternal.h"
#include <QTimeZone>
#include "cfs/UtilityCfs.h"
#include "gfs/GfsClimate.h"
#include "util/Utility.h"
#include "gfs/GfsNormals.h"

namespace GfsChart {
namespace detail {

void gfsProducts(std::vector<Product>& p, std::map<std::string, Product>& plainPrecip) {
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
// the stratosphere, for the polar vortex: wind speed, and temperature, with the heights as lines (the levels the model files carry: 100, 70, 50, 30, 20, 10, 5, 2 and 1 mb)
for (const auto& [level, interval] : {std::pair{"100 mb", 6.0}, {"70 mb", 8.0}, {"50 mb", 8.0}, {"30 mb", 12.0}, {"20 mb", 12.0}, {"10 mb", 16.0}, {"5 mb", 20.0}, {"2 mb", 24.0}, {"1 mb", 28.0}}) {
    const std::string number = std::string{level}.substr(0, std::string{level}.size() - 3);
    auto wind = upper((number + "_wnd_ht").c_str(), (number + "mb Wind and Height").c_str(), level, interval);
    wind.legendStep = 20;
    p.push_back(wind);
    auto temp = upper((number + "_temp_ht").c_str(), (number + "mb Temperature, Wind and Height").c_str(), level, interval);
    temp.wants.push_back(want("t", "TMP", level));
    temp.fill = [] (const Grids& g) { return pick(g, "t"); };
    temp.ramp = number == "5" || number == "2" || number == "1" ? upperStratosphereTemperature() : stratosphereTemperature();
    temp.fillTitle = "Temperature";
    temp.legendStep = 10;
    temp.quantity = Quantity::Temperature;
    p.push_back(temp);
}
// Ertel potential vorticity on a pressure level, in PVU (1e-6 K m2 / kg s), from the temperature at the level and at the levels above and below it and the wind at the level: PV = -g (zeta + f) dtheta/dp,
// zeta the relative vorticity on the pressure surface (the slope terms are left out: good in the stratosphere, where the surfaces are nearly flat). The southern hemisphere's sign is turned over, so the
// vortex of either pole is positive. The GFS files carry no PV on pressure levels (only the 2 PVU surface), so it is worked out; the levels above and below are those the files have.
const auto potentialVorticity = [] (double pAbove, double pLevel, double pBelow) {
    return [=] (const Grids& g) {
        auto zeta = GfsGrid::vorticity(GfsGrid::smoothed(pick(g, "u"), 1), GfsGrid::smoothed(pick(g, "v"), 1));   // 1 / s
        const auto& ta = pick(g, "ta"), & tb = pick(g, "tb");
        constexpr double kappa = 0.2857, gravity = 9.80665, omega = 7.2921e-5, pi = 3.14159265358979;
        const double thetaAbove = std::pow(1000.0 / pAbove, kappa), thetaBelow = std::pow(1000.0 / pBelow, kappa);
        (void) pLevel;
        for (int r = 0; r < zeta.rows; r++) {
            const double lat = zeta.lat0 - r * zeta.step;
            const double f = 2.0 * omega * std::sin(lat * pi / 180.0), hemisphere = lat < 0.0 ? -1.0 : 1.0;
            for (int c = 0; c < zeta.columns; c++) {
                const size_t at = static_cast<size_t>(r) * static_cast<size_t>(zeta.columns) + static_cast<size_t>(c);
                const double dtheta = ((tb.values[at] + 273.15) * thetaBelow - (ta.values[at] + 273.15) * thetaAbove) / ((pBelow - pAbove) * 100.0);   // K / Pa
                zeta.values[at] = static_cast<float>(hemisphere * 1e6 * -gravity * (zeta.values[at] + f) * dtheta);
            }
        }
        return zeta;
    };
};
// potential vorticity: the stratosphere's levels (the polar vortex is its high PV), the scale to the level's own size (PV grows steeply with height as the air gets more stable; the tops are a quarter above
// the largest value seen on the 15 November 2026 00Z initial state north of 30 N)
for (const auto& [level, above, below, top, interval] : {std::tuple{100, 70, 150, 40.0, 6.0}, {70, 50, 100, 60.0, 8.0}, {50, 40, 70, 90.0, 8.0}, {30, 20, 40, 220.0, 12.0}, {20, 10, 30, 360.0, 12.0}, {10, 7, 20, 560.0, 16.0}}) {
    const auto levelText = std::to_string(level) + " mb";
    auto x = upper((std::to_string(level) + "_pv_ht").c_str(), (std::to_string(level) + "mb Potential Vorticity, Wind and Height").c_str(), levelText.c_str(), interval);
    x.wants.push_back(want("ta", "TMP", (std::to_string(above) + " mb").c_str()));
    x.wants.push_back(want("tb", "TMP", (std::to_string(below) + " mb").c_str()));
    x.fill = potentialVorticity(above, level, below);
    x.ramp = potentialVorticityRamp(top);
    x.fillTitle = "Potential vorticity (PVU)";
    x.legendStep = top >= 360 ? 100 : top >= 200 ? 50 : top >= 90 ? 20 : 10;
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
    t.setTimeZone(QTimeZone::utc());
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
    t.setTimeZone(QTimeZone::utc());
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
{   // the relative humidity near the ground from the temperature and the dewpoint, so that every model that gives those has it (the AIGFS has no humidity at 2 m, only the two)
    Product x;
    x.id = "2m_rh_10m_wnd_derived";
    x.label = "2 m Relative Humidity (from temperature and dewpoint), MSLP and 10m Wind";
    x.wants = {want("t", "TMP", "2 m above ground"), want("d", "DPT", "2 m above ground"), want("u", "UGRD", "10 m above ground"), want("v", "VGRD", "10 m above ground"), want("p", "PRMSL", "mean sea level")};
    x.derive = [] (Grids& g, const Context&) {
        auto rh = g.at("t");
        const auto& d = g.at("d");
        for (size_t i = 0; i < rh.values.size() && i < d.values.size(); i++) {   // (degrees C: the vapor pressure over the saturation pressure, Magnus)
            const double t = rh.values[i], td = d.values[i];
            const double value = 100.0 * std::exp(17.625 * td / (243.04 + td)) / std::exp(17.625 * t / (243.04 + t));
            rh.values[i] = std::isfinite(value) ? static_cast<float>(std::clamp(value, 0.0, 100.0)) : std::nanf("");
        }
        g["rh"] = std::move(rh);
    };
    x.fill = [] (const Grids& g) { return pick(g, "rh"); };
    x.ramp = humidity();   // (our scale: the site's two bands show only the air above 70 and 90 per cent)
    x.fillTitle = "2 m relative humidity (%)";
    x.legendStep = 10;
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
    // the same under the other sets of lines (1000-850 and 850-700 mb thickness, 850 mb temperature, 10 m wind): the amount in the colors of what is falling
    const auto splitType = [] (Grids& g) {
        const auto& total = g["precip"];
        auto rain = total, snow = total, mix = total;
        for (size_t i = 0; i < total.values.size(); i++) {
            const bool isSnow = g["cs"].values[i] >= 0.5f;
            const bool isMix = g["cf"].values[i] >= 0.5f || g["ci"].values[i] >= 0.5f;
            rain.values[i] = (!isSnow && !isMix) ? total.values[i] : 0.0f;
            snow.values[i] = isSnow && !isMix ? total.values[i] : 0.0f;
            mix.values[i] = isMix ? total.values[i] : 0.0f;
        }
        g["rain"] = std::move(rain);
        g["snow"] = std::move(snow);
        g["mix"] = std::move(mix);
    };
    for (const auto& [baseId, typedId, typedLabel] : {std::tuple{"1000_850_thick", "1000_850_thick_type", "MSLP, 1000-850mb thickness and precipitation type"},
                                                      {"850_700_thick", "850_700_thick_type", "MSLP, 850-700mb thickness and precipitation type"},
                                                      {"850_temp_mslp_precip", "850_temp_mslp_precip_type", "MSLP, 850mb temperature and precipitation type"},
                                                      {"10m_wnd_precip", "10m_wnd_precip_type", "MSLP, 10m wind and precipitation type"}}) {
        const auto found = std::find_if(p.begin(), p.end(), [id = std::string{baseId}] (const Product& q) { return q.id == id; });
        if (found == p.end()) {
            continue;
        }
        Product y = *found;
        y.id = typedId;
        y.label = typedLabel;
        y.needs = [inner = found->needs] (int hour) {
            auto needs = inner(hour);
            for (const auto& [key, variable] : {std::pair{"cr", "CRAIN"}, {"cs", "CSNOW"}, {"cf", "CFRZR"}, {"ci", "CICEP"}}) {
                needs.push_back({hour, {key, variable, "surface", ""}});
            }
            return needs;
        };
        y.derive = [inner = found->derive, splitType] (Grids& g, const Context& context) {
            if (inner) {
                inner(g, context);
            }
            splitType(g);
        };
        y.fill = x.fill;
        y.ramp = x.ramp;
        y.palette = x.palette;
        y.overlays = x.overlays;
        y.fillTitleFor = x.fillTitleFor;
        p.push_back(std::move(y));
    }
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

}

}  // namespace detail
}  // namespace GfsChart
