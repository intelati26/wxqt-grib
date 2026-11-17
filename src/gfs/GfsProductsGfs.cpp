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
    p.push_back(cloudLayersProduct("GFS"));   // the clouds by height (low, middle, high) in one map
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
// potential vorticity: the stratosphere's levels (the polar vortex is its high PV), the scale to the level's own size (PV grows steeply with height as the air gets more stable; the tops are a quarter above
// the largest value seen on the 15 November 2026 00Z initial state north of 30 N, with the log-pressure stability)
for (const auto& [level, above, below, top, interval] : {std::tuple{100, 70, 150, 40.0, 6.0}, {70, 50, 100, 60.0, 8.0}, {50, 40, 70, 95.0, 8.0}, {30, 20, 40, 220.0, 12.0}, {20, 10, 30, 340.0, 12.0}, {10, 7, 20, 710.0, 16.0}}) {
    const auto levelText = std::to_string(level) + " mb";
    auto x = upper((std::to_string(level) + "_pv_ht").c_str(), (std::to_string(level) + "mb Potential Vorticity, Wind and Height").c_str(), levelText.c_str(), interval);
    x.wants.push_back(want("ta", "TMP", (std::to_string(above) + " mb").c_str()));
    x.wants.push_back(want("tb", "TMP", (std::to_string(below) + " mb").c_str()));
    x.fill = potentialVorticityOf(above, level, below);
    x.ramp = potentialVorticityRamp(top);
    x.fillTitle = "Potential vorticity (PVU)";
    x.legendStep = top >= 340 ? 100 : top >= 200 ? 50 : top >= 90 ? 20 : 10;
    p.push_back(x);
}
// Potential vorticity on an isentropic surface (a surface of one potential temperature, 850 K near 10 mb, 650 K near 20 mb, 530 K near 45 mb): the pressure-level PV (potentialVorticityOf, in log pressure)
// at the levels round it, the pressure where the potential temperature crosses theta0 found in each column (linear in theta against ln p between two levels), and the PV, the wind and the pressure
// put on it the same way. This is the form the polar vortex is usually shown in (the air does not cross these surfaces, so the PV follows the air). `levels` are the pressures (mb, the top first)
// whose temperature is read; PV is made at every one that has a level on each side; the wind is read at those.
for (const auto& [theta0, levels, top, name] : {std::tuple{850.0, std::vector<int>{3, 5, 7, 10, 20, 30}, 900.0, "850 K"}, {650.0, std::vector<int>{7, 10, 20, 30, 40, 50}, 400.0, "650 K"}, {600.0, std::vector<int>{7, 10, 20, 30, 40, 50, 70}, 250.0, "600 K"}, {530.0, std::vector<int>{20, 30, 40, 50, 70, 100}, 130.0, "530 K"}, {500.0, std::vector<int>{20, 30, 40, 50, 70, 100}, 70.0, "500 K"}, {330.0, std::vector<int>{100, 150, 200, 250, 300, 400, 500, 600, 700, 850}, 10.0, "330 K"}}) {
    Product x;
    const std::string tag = std::to_string(static_cast<int>(theta0)) + "K";
    x.id = tag + "_pv";
    x.label = std::string{name} + " Isentropic Potential Vorticity, Wind and Pressure";
    const auto levelName = [] (int mb) { return std::to_string(mb) + " mb"; };
    for (size_t i = 0; i < levels.size(); i++) {
        x.wants.push_back(want("t" + std::to_string(levels[i]), "TMP", levelName(levels[i])));
        if (i > 0 && i + 1 < levels.size()) {
            x.wants.push_back(want("u" + std::to_string(levels[i]), "UGRD", levelName(levels[i])));
            x.wants.push_back(want("v" + std::to_string(levels[i]), "VGRD", levelName(levels[i])));
        }
    }
    x.derive = [theta0, levels] (Grids& g, const Context&) {
        constexpr double kappa = 0.2857;
        const size_t n = levels.size();
        std::vector<GfsGrid::Grid> pv(n), u(n), v(n);
        for (size_t i = 1; i + 1 < n; i++) {
            Grids one;
            const auto key = [&levels] (char c, size_t at) { return std::string{c} + std::to_string(levels[at]); };
            one["u"] = g[key('u', i)];
            one["v"] = g[key('v', i)];
            one["ta"] = g[key('t', i - 1)];
            one["tb"] = g[key('t', i + 1)];
            pv[i] = potentialVorticityOf(levels[i - 1], levels[i], levels[i + 1])(one);
            u[i] = one["u"];
            v[i] = one["v"];
        }
        auto out = pv[1];
        GfsGrid::Grid pressure = out, windU = out, windV = out;
        std::vector<double> theta(n), logp(n);
        for (size_t i = 0; i < n; i++) {
            logp[i] = std::log(static_cast<double>(levels[i]));
        }
        const double nan = std::nan("");
        for (size_t at = 0; at < out.values.size(); at++) {
            bool ok = true;
            for (size_t i = 0; i < n; i++) {
                theta[i] = (g["t" + std::to_string(levels[i])].values[at] + 273.15) * std::pow(1000.0 / levels[i], kappa);
                ok = ok && !std::isnan(theta[i]);
            }
            out.values[at] = pressure.values[at] = windU.values[at] = windV.values[at] = static_cast<float>(nan);
            if (!ok) {
                continue;
            }
            for (size_t i = 1; i + 2 < n; i++) {   // between two levels that both have a PV: theta goes down with the index (the pressure grows)
                if ((theta[i] - theta0) * (theta[i + 1] - theta0) > 0.0 || theta[i] == theta[i + 1]) {
                    continue;
                }
                const double f = (theta0 - theta[i]) / (theta[i + 1] - theta[i]);
                out.values[at] = static_cast<float>(pv[i].values[at] + f * (pv[i + 1].values[at] - pv[i].values[at]));
                pressure.values[at] = static_cast<float>(std::exp(logp[i] + f * (logp[i + 1] - logp[i])));
                windU.values[at] = static_cast<float>(u[i].values[at] + f * (u[i + 1].values[at] - u[i].values[at]));
                windV.values[at] = static_cast<float>(v[i].values[at] + f * (v[i + 1].values[at] - v[i].values[at]));
                break;
            }
        }
        g["ipv"] = std::move(out);
        g["ip"] = std::move(pressure);
        g["iu"] = std::move(windU);
        g["iv"] = std::move(windV);
    };
    x.fill = [] (const Grids& g) { return pick(g, "ipv"); };
    x.ramp = potentialVorticityRamp(top);
    x.fillTitle = "Potential vorticity on the surface (PVU)";
    x.legendStep = top >= 800 ? 200 : top >= 400 ? 100 : top >= 200 ? 50 : 20;
    ContourSet c;
    c.key = "ip";
    c.scale = 1.0;
    c.interval = theta0 > 800 ? 1.0 : theta0 > 620 ? 2.0 : theta0 < 400 ? 25.0 : 5.0;   // (330 K crosses 150 to 500 mb: lines every 25)
    c.title = "Pressure of the " + std::string{name} + " surface (mb)";
    x.contours = {c};
    x.barbU = "iu";
    x.barbV = "iv";
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
// the temperature and precipitable water anomalies from the 1991-2020 reanalysis normal (section 14 of the plan), with the view in standard deviations (the normalized anomaly) as the height
// ones have. The 2 m one is against the reanalysis' daily mean near the surface, so the time of day is in it (a warm afternoon reads warmer than the day as a whole).
{
    struct Spec {
        const char * id, * label, * variable, * level, * title;
        GfsClimate::Field (*field)();
        double kelvin;   // added to the file's value to be in the normal's unit (degrees C -> K), 0 for precipitable water
        bool mslp;       // the lines: the sea level pressure (else the 850 mb heights)
    };
    static const Spec specs[] = {
        {"850_temp_anom", "850mb Temperature Anomaly, Height and Wind", "TMP", "850 mb", "850 mb temperature anomaly from the 1991-2020 mean (C)", [] { return GfsClimate::temperature(850); }, 273.15, false},
        {"2m_temp_anom", "2m Temperature Anomaly, MSLP and 10m Wind", "TMP", "2 m above ground", "2 m temperature against the 1991-2020 daily mean near the surface (C)", &GfsClimate::surfaceTemperature, 273.15, true},
        {"pwat_anom", "Precipitable Water Anomaly and MSLP", "PWAT", "entire atmosphere (considered as a single layer)", "Precipitable water anomaly from the 1991-2020 mean (mm)", &GfsClimate::precipitableWater, 0.0, true},
    };
    for (const auto& spec : specs) {
        Product x;
        x.id = spec.id;
        x.label = spec.label;
        x.wants = {want("f", spec.variable, spec.level)};
        if (spec.mslp) {
            x.wants.push_back(want("p", "PRMSL", "mean sea level"));
            if (std::string{spec.variable} == "TMP") {
                x.wants.push_back(want("u", "UGRD", "10 m above ground"));
                x.wants.push_back(want("v", "VGRD", "10 m above ground"));
            }
        } else {
            x.wants.push_back(want("z", "HGT", "850 mb"));
            x.wants.push_back(want("u", "UGRD", "850 mb"));
            x.wants.push_back(want("v", "VGRD", "850 mb"));
            x.wants.push_back(want("ps", "PRES", "surface"));   // where the ground is above 850 mb the level is underground
        }
        const auto field = spec.field();
        const double kelvin = spec.kelvin;
        const bool masked = !spec.mslp;
        const auto inNormalUnit = [kelvin] (const GfsGrid::Grid& g) {
            auto out = g;
            for (auto& v : out.values) {
                v = static_cast<float>(v + kelvin);
            }
            return out;
        };
        x.derive = [validDay, field, inNormalUnit, masked] (Grids& g, const Context& context) {
            GfsGrid::Grid normal;
            std::string error;
            if (!g.count("f") || !context.climate || !context.climate->at(field, validDay(context), normal, error)) {
                return;
            }
            auto anom = GfsGrid::anomaly(inNormalUnit(g.at("f")), normal);   // (a difference: kelvin and degrees C alike)
            if (masked && g.count("ps")) {
                for (size_t i = 0; i < anom.values.size() && i < g.at("ps").values.size(); i++) {
                    if (g.at("ps").values[i] < 85000.0f) {
                        anom.values[i] = std::nanf("");
                    }
                }
            }
            g["anom"] = std::move(anom);
        };
        x.fill = [] (const Grids& g) { return pick(g, "anom"); };
        x.ramp = spec.kelvin > 0.0 ? temperatureDeparture() : precipitationDeparture();
        x.fillTitle = spec.title;
        x.legendStep = 0;
        x.sigma = [sigma = sigmaOf(field, 1.0, false, nullptr, 1.0), inNormalUnit] (Grids& g, const Context& context) {   // (the helper reads its field as "z": the heights wait aside)
            const bool heights = g.count("z") > 0;
            GfsGrid::Grid kept;
            if (heights) {
                kept = g.at("z");
            }
            g["z"] = inNormalUnit(g.at("f"));
            sigma(g, context);
            if (heights) {
                g["z"] = std::move(kept);
            } else {
                g.erase("z");
            }
        };
        x.sigmaUnit = spec.kelvin > 0.0 ? "C" : "mm";
        x.sigmaSizeKey = "sd_size";
        x.sigmaReadKey = "anom_unit";
        x.sigmaInterval = spec.kelvin > 0.0 ? 2.0 : 5.0;
        x.contours = {spec.mslp ? pressure() : heights(3.0)};
        for (const auto& w : x.wants) {
            if (w.key == "u") {
                x.barbU = "u";
                x.barbV = "v";
            }
        }
        p.push_back(x);
    }
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
// ---- section 14 of the plan: the rest of Tropical Tidbits' GFS menu
{   // snowfall at 10:1: in 6 hour pieces, each piece's precipitation (the running totals' difference) times the percent of it that is frozen at the piece's end (CPOFP), as
    // centimeters of snow (1 mm of water is 1 cm of snow at 10:1)
    for (const auto& [id, label, period] : {std::tuple{"snow_24h", "Snowfall in 24 hours (10:1)", 24}, {"snow_total", "Total Snowfall (10:1)", 0}}) {
        Product x;
        x.id = id;
        x.label = label;
        const int span = period;
        const auto bounds = [span] (int hour) {   // the pieces' ends, the last first, back to the start of the period
            std::vector<int> ends;
            const int from = span > 0 ? std::max(0, hour - span) : 0;
            for (int e = hour; e > from; e -= 6) {
                ends.push_back(e);
            }
            return std::make_pair(ends, from);
        };
        x.needs = [bounds] (int hour) {
            std::vector<GfsData::Need> needs;
            const auto [ends, from] = bounds(hour);
            for (const int e : ends) {
                needs.push_back({e, {"a" + std::to_string(e), "APCP", "surface", "0-*"}});
                needs.push_back({e, {"f" + std::to_string(e), "CPOFP", "surface", ""}});
            }
            if (from > 0) {
                needs.push_back({from, {"a" + std::to_string(from), "APCP", "surface", "0-*"}});
            }
            return needs;
        };
        x.derive = [bounds] (Grids& g, const Context& context) {
            const auto [ends, from] = bounds(context.hour);
            GfsGrid::Grid total;
            for (const int e : ends) {
                const int start = std::max(from, e - 6);
                const auto endKey = "a" + std::to_string(e), startKey = "a" + std::to_string(start), fracKey = "f" + std::to_string(e);
                if (!g.count(endKey) || !g.count(fracKey) || (start > 0 && !g.count(startKey))) {
                    continue;
                }
                const auto& a1 = g.at(endKey);
                if (total.empty()) {
                    total = a1;
                    std::fill(total.values.begin(), total.values.end(), 0.0f);
                }
                for (size_t i = 0; i < total.values.size(); i++) {
                    const double water = a1.values[i] - (start > 0 ? g.at(startKey).values[i] : 0.0f);
                    const double frozen = std::clamp(static_cast<double>(g.at(fracKey).values[i]), 0.0, 100.0) / 100.0;   // (-50 where no precipitation: none)
                    if (water > 0.0 && std::isfinite(frozen)) {
                        total.values[i] += static_cast<float>(water * frozen);   // mm of water = cm of snow at 10:1
                    }
                }
            }
            g["snow"] = std::move(total);
        };
        x.fill = [] (const Grids& g) { return pick(g, "snow"); };
        x.ramp = snowfall();
        x.quantity = Quantity::Centimeters;
        x.fillTitleFor = [span] (int hour) { return std::string{"Snowfall at 10:1, hours "} + std::to_string(span > 0 ? std::max(0, hour - span) : 0) + "-" + std::to_string(hour); };
        x.legendStep = 0;
        p.push_back(x);
    }
}
{   // temperature advection and frontogenesis at 850 and 700 mb (horizontal, on the pressure surface; the frontogenesis is the 2-D kinematic one of Petterssen: the change of the
    // potential temperature gradient's size by the wind's deformation and convergence)
    for (const int level : {850, 700}) {
        for (const bool front : {false, true}) {
            Product x;
            const std::string L = std::to_string(level);
            x.id = L + (front ? "_frontogenesis" : "_temp_adv");
            x.label = L + "mb " + (front ? "Frontogenesis, Height and Wind" : "Temperature Advection, Height and Wind");
            const std::string lt = L + " mb";
            x.wants = {want("t", "TMP", lt.c_str()), want("u", "UGRD", lt.c_str()), want("v", "VGRD", lt.c_str()), want("z", "HGT", lt.c_str()), want("ps", "PRES", "surface")};
            const double pLevel = level;
            x.derive = [front, pLevel] (Grids& g, const Context&) {
                if (!g.count("t") || !g.count("u") || !g.count("v")) {
                    return;
                }
                const auto t = GfsGrid::smoothed(g.at("t"), 2), u = GfsGrid::smoothed(g.at("u"), 2), v = GfsGrid::smoothed(g.at("v"), 2);
                GfsGrid::Grid tx, ty;
                GfsGrid::gradient(t, tx, ty);
                auto out = t;
                if (!front) {
                    for (size_t i = 0; i < out.values.size(); i++) {
                        out.values[i] = static_cast<float>(-(u.values[i] * tx.values[i] + v.values[i] * ty.values[i]) * 10800.0);   // C per 3 hours
                    }
                } else {
                    GfsGrid::Grid ux, uy, vx, vy;
                    GfsGrid::gradient(u, ux, uy);
                    GfsGrid::gradient(v, vx, vy);
                    const double toTheta = std::pow(1000.0 / pLevel, 0.2857);   // (the gradient of theta is the temperature's times this on a pressure surface)
                    for (size_t i = 0; i < out.values.size(); i++) {
                        const double thx = tx.values[i] * toTheta, thy = ty.values[i] * toTheta;
                        const double size = std::hypot(thx, thy);
                        const double f = size > 1e-12 ? -(thx * thx * ux.values[i] + thx * thy * (vx.values[i] + uy.values[i]) + thy * thy * vy.values[i]) / size : 0.0;
                        out.values[i] = static_cast<float>(f * 1e5 * 10800.0);   // K per 100 km per 3 hours
                    }
                }
                if (g.count("ps")) {   // under the ground: nothing
                    for (size_t i = 0; i < out.values.size() && i < g.at("ps").values.size(); i++) {
                        if (g.at("ps").values[i] < pLevel * 100.0) {
                            out.values[i] = std::nanf("");
                        }
                    }
                }
                g["f"] = std::move(out);
            };
            x.fill = [] (const Grids& g) { return pick(g, "f"); };
            x.ramp = front ? Ramp{{{0, QColor{255, 220, 120, 0}}, {0.5, QColor{255, 220, 120, 0}}, {1, QColor{"#ffe08a"}}, {2, QColor{"#ffb347"}}, {4, QColor{"#f46d43"}}, {8, QColor{"#d73027"}}, {16, QColor{"#7f0026"}}}}
                           : temperatureDeparture();
            x.fillTitle = front ? "Frontogenesis (K per 100 km per 3 hours; the potential temperature's gradient strengthening)" : "Temperature advection (C per 3 hours: warm red, cold blue)";
            x.legendStep = 0;
            x.contours = {heights(3.0)};
            x.barbU = "u";
            x.barbV = "v";
            p.push_back(x);
        }
    }
}
{   // the Okubo-Weiss number at 850 mb: the strain (stretching and shearing deformation) squared less the vorticity squared: strain-dominated flow (fronts, the dilatation axes) red,
    // rotation-dominated (the eddies, a tropical wave's pouch) blue; streamlines over it
    Product x;
    x.id = "850_okubo_weiss";
    x.label = "850mb Okubo-Weiss (strain against rotation) and Streamlines";
    x.wants = {want("u", "UGRD", "850 mb"), want("v", "VGRD", "850 mb"), want("ps", "PRES", "surface")};
    x.derive = [] (Grids& g, const Context&) {
        if (!g.count("u") || !g.count("v")) {
            return;
        }
        const auto u = GfsGrid::smoothed(g.at("u"), 10), v = GfsGrid::smoothed(g.at("v"), 10);   // (the synoptic scale: at 0.25 degrees the strain and the vorticity of small eddies swamp it)
        GfsGrid::Grid ux, uy, vx, vy;
        GfsGrid::gradient(u, ux, uy);
        GfsGrid::gradient(v, vx, vy);
        auto out = u;
        for (size_t i = 0; i < out.values.size(); i++) {
            const double sn = ux.values[i] - vy.values[i], ss = vx.values[i] + uy.values[i], zeta = vx.values[i] - uy.values[i];
            out.values[i] = static_cast<float>((sn * sn + ss * ss - zeta * zeta) * 1e10);
            if (g.count("ps") && i < g.at("ps").values.size() && g.at("ps").values[i] < 85000.0f) {
                out.values[i] = std::nanf("");
            }
        }
        g["ow"] = std::move(out);
    };
    x.fill = [] (const Grids& g) { return pick(g, "ow"); };
    x.ramp = Ramp{{{-200, QColor{"#2c3e9c"}}, {-60, QColor{"#5a8fd0"}}, {-15, QColor{"#b8d6ee"}}, {-5, QColor{184, 214, 238, 0}}, {5, QColor{248, 200, 180, 0}}, {15, QColor{"#f8c8b4"}},
                   {60, QColor{"#e8724e"}}, {200, QColor{"#a01e1e"}}}};
    x.fillTitle = "Okubo-Weiss number (10^-10 per s^2, the wind smoothed to the synoptic scale: strain red, rotation blue)";
    x.legendStep = 0;
    x.streamU = "u";
    x.streamV = "v";
    p.push_back(x);
}
{   // Q-vector divergence at 300 mb (the 200-400 mb layer's middle), the geostrophic wind from the heights (no Q within 10 degrees of the equator): -2 div Q is the forcing for rising
    // air (red) and sinking (blue) of the quasi-geostrophic omega equation; the fields smoothed first, as such charts are
    Product x;
    x.id = "300_qvector_div";
    x.label = "300mb Q-Vector Divergence (QG forcing for vertical motion) and Height";
    x.wants = {want("t", "TMP", "300 mb"), want("z", "HGT", "300 mb")};
    x.derive = [] (Grids& g, const Context&) {
        if (!g.count("t") || !g.count("z")) {
            return;
        }
        // on a 1 degree grid: Q is made of second derivatives, which at 0.25 degrees are the grid's noise, not the synoptic forcing
        const auto coarse = [] (const GfsGrid::Grid& in) {
            const double step = std::max(1.0, in.step);
            GfsGrid::Grid out;
            out.step = step;
            out.lon0 = in.lon0;
            out.lat0 = in.lat0;
            out.columns = std::max(2, static_cast<int>(std::lround(in.columns * in.step / step)));
            out.rows = std::max(2, static_cast<int>(std::lround((in.rows - 1) * in.step / step)) + 1);
            out.values.resize(static_cast<size_t>(out.columns) * static_cast<size_t>(out.rows));
            for (int r = 0; r < out.rows; r++) {
                for (int c = 0; c < out.columns; c++) {
                    out.values[static_cast<size_t>(r) * static_cast<size_t>(out.columns) + static_cast<size_t>(c)] = in.sample(out.lon0 + c * step, out.lat0 - r * step);
                }
            }
            return GfsGrid::smoothed(out, 2);
        };
        const auto t = coarse(g.at("t")), z = coarse(g.at("z"));
        GfsGrid::Grid zx, zy, tx, ty;
        GfsGrid::gradient(z, zx, zy);
        GfsGrid::gradient(t, tx, ty);
        auto ug = z, vg = z;
        for (int r = 0; r < z.rows; r++) {
            const double lat = z.lat0 - r * z.step;
            const double f = 2.0 * 7.292e-5 * std::sin(lat * M_PI / 180.0);
            for (int c = 0; c < z.columns; c++) {
                const size_t i = static_cast<size_t>(r) * static_cast<size_t>(z.columns) + static_cast<size_t>(c);
                const bool tropics = std::fabs(lat) < 10.0;
                ug.values[i] = tropics ? std::nanf("") : static_cast<float>(-9.80665 / f * zy.values[i]);
                vg.values[i] = tropics ? std::nanf("") : static_cast<float>(9.80665 / f * zx.values[i]);
            }
        }
        GfsGrid::Grid ugx, ugy, vgx, vgy;
        GfsGrid::gradient(ug, ugx, ugy);
        GfsGrid::gradient(vg, vgx, vgy);
        auto q1 = z, q2 = z;
        const double rp = 287.04 / 30000.0;
        for (size_t i = 0; i < q1.values.size(); i++) {
            q1.values[i] = static_cast<float>(-rp * (ugx.values[i] * tx.values[i] + vgx.values[i] * ty.values[i]));
            q2.values[i] = static_cast<float>(-rp * (ugy.values[i] * tx.values[i] + vgy.values[i] * ty.values[i]));
        }
        auto div = GfsGrid::divergence(q1, q2);
        for (auto& v : div.values) {
            v = static_cast<float>(-2.0 * v * 1e17);
        }
        g["qd"] = GfsGrid::smoothed(div, 1);
    };
    x.fill = [] (const Grids& g) { return pick(g, "qd"); };
    x.ramp = Ramp{{{-20, QColor{"#2c3e9c"}}, {-8, QColor{"#5a8fd0"}}, {-2, QColor{"#b8d6ee"}}, {-0.6, QColor{184, 214, 238, 0}}, {0.6, QColor{248, 200, 180, 0}}, {2, QColor{"#f8c8b4"}},
                   {8, QColor{"#e8724e"}}, {20, QColor{"#a01e1e"}}}};
    x.fillTitle = "-2 div Q (10^-17 K per m^2 per s: forcing for rising red, sinking blue)";
    x.legendStep = 0;
    x.contours = {heights(12.0)};
    p.push_back(x);
}
{   // the irrotational (divergent) wind at 200 mb and its velocity potential: the divergence put through laplacian(chi) = div on the globe; the outflow of the big ascent (the MJO, the
    // monsoons) from the minima of chi
    Product x;
    x.id = "200_velocity_potential";
    x.label = "200mb Velocity Potential and Irrotational Wind";
    x.wants = {want("u", "UGRD", "200 mb"), want("v", "VGRD", "200 mb")};
    x.derive = [] (Grids& g, const Context&) {
        if (!g.count("u") || !g.count("v")) {
            return;
        }
        auto chi = GfsGrid::velocityPotential(GfsGrid::divergence(g.at("u"), g.at("v")));
        if (chi.empty()) {
            return;
        }
        GfsGrid::Grid cx, cy;
        GfsGrid::gradient(chi, cx, cy);
        g["iu"] = std::move(cx);
        g["iv"] = std::move(cy);
        g["chi"] = GfsGrid::scaled(chi, 1e-6);
    };
    x.fill = [] (const Grids& g) { return pick(g, "chi"); };
    x.ramp = Ramp{{{-15, QColor{"#1a7a3a"}}, {-8, QColor{"#5cb85c"}}, {-3, QColor{"#c4e6b8"}}, {-1, QColor{196, 230, 184, 0}}, {1, QColor{230, 200, 230, 0}}, {3, QColor{"#e6c4e6"}},
                   {8, QColor{"#a05ab0"}}, {15, QColor{"#5a1a6e"}}}};   // (rising air's outflow: chi low, green, as the MJO maps)
    x.fillTitle = "Velocity potential (10^6 m^2/s: green the divergent outflow over rising air); barbs: the irrotational wind";
    x.legendStep = 0;
    x.barbU = "iu";
    x.barbV = "iv";
    x.cloneTo = [] (const std::string& model) {   // (the whole globe is needed: the global models and analyses only)
        static const std::set<std::string> global{"GEFS", "IFS", "AIFS", "AIGFS", "ICON", "UKMO", "GSM", "ARPEGE", "GRAPES", "GDPS", "GEPS", "HGEFS", "ERA5", "GEOS-FP", "GFS-ANL", "GFS1-ANL", "CFS", "CFS-ANL"};
        return global.count(model) > 0;
    };
    p.push_back(x);
}
{   // surface-based CAPE and the wind crossover: how far the wind turns from 850 to 500 mb (veering, clockwise, positive), drawn as lines from 45 degrees; barbs at 850 mb
    Product x;
    x.id = "sbcape_crossover";
    x.label = "Surface-Based CAPE and the 850-500mb Wind Crossover";
    x.wants = {want("c", "CAPE", "surface"), want("u", "UGRD", "850 mb"), want("v", "VGRD", "850 mb"), want("u5", "UGRD", "500 mb"), want("v5", "VGRD", "500 mb")};
    x.derive = [] (Grids& g, const Context&) {
        if (!g.count("u") || !g.count("u5")) {
            return;
        }
        auto turn = g.at("u");
        for (size_t i = 0; i < turn.values.size(); i++) {
            const double a8 = std::atan2(g.at("v").values[i], g.at("u").values[i]), a5 = std::atan2(g.at("v5").values[i], g.at("u5").values[i]);
            double d = (a8 - a5) * 180.0 / M_PI;   // (the direction the wind blows toward, counterclockwise from east: veering is a clockwise turn, so 850 less 500)
            while (d > 180.0) d -= 360.0;
            while (d < -180.0) d += 360.0;
            const bool unstable = g.count("c") && i < g.at("c").values.size() && g.at("c").values[i] >= 100.0f;   // (a crossover matters where there is something to lift)
            turn.values[i] = !unstable || std::hypot(g.at("u5").values[i], g.at("v5").values[i]) < 5.0 ? std::nanf("") : static_cast<float>(d);   // (a light 500 mb wind has no direction to speak of)
        }
        g["turn"] = GfsGrid::smoothed(turn, 2);
    };
    x.fill = [] (const Grids& g) { return pick(g, "c"); };
    x.ramp = capeRamp();
    x.fillTitle = "Surface-based CAPE (J/kg)";
    x.legendStep = 500;
    ContourSet c;
    c.key = "turn";
    c.interval = 45.0;
    c.minimum = 44.0;
    c.title = "Veering 850-500 mb (degrees, from 45; where CAPE is 100 J/kg or more)";
    c.color = QColor{120, 30, 160};
    c.width = 1.4;
    x.contours = {c};
    x.barbU = "u";
    x.barbV = "v";
    p.push_back(x);
}
{   // the isentropic analysis of the lower troposphere at 300 K and 290 K: the relative humidity, the wind and the pressure on the surface (the air rides up a surface where the pressure
    // falls along the wind: the lift of the warm conveyor), from the pressure levels (theta against ln p between two levels); underground where the surface's pressure is over the ground's
    for (const double theta0 : {300.0, 290.0}) {
        Product x;
        const std::string tag = std::to_string(static_cast<int>(theta0));
        x.id = tag + "K_isen";
        x.label = tag + " K Isentropic Relative Humidity, Wind and Pressure";
        static const std::vector<int> levels{400, 500, 600, 700, 850, 925, 1000};
        for (const int mb : levels) {
            const auto l = std::to_string(mb) + " mb";
            x.wants.push_back(want("t" + std::to_string(mb), "TMP", l));
            x.wants.push_back(want("r" + std::to_string(mb), "RH", l));
            x.wants.push_back(want("u" + std::to_string(mb), "UGRD", l));
            x.wants.push_back(want("v" + std::to_string(mb), "VGRD", l));
        }
        x.wants.push_back(want("ps", "PRES", "surface"));
        x.derive = [theta0] (Grids& g, const Context&) {
            const auto key = [] (char c, int mb) { return std::string{c} + std::to_string(mb); };
            if (!g.count(key('t', 500))) {
                return;
            }
            auto rh = g.at(key('t', 500)), pr = rh, uu = rh, vv = rh;
            const size_t n = levels.size();
            for (size_t at = 0; at < rh.values.size(); at++) {
                rh.values[at] = pr.values[at] = uu.values[at] = vv.values[at] = std::nanf("");
                for (size_t i = 0; i + 1 < n; i++) {   // the top first: theta falls with the index
                    const int a = levels[i], b = levels[i + 1];
                    const double ta = (g.at(key('t', a)).values[at] + 273.15) * std::pow(1000.0 / a, 0.2857), tb = (g.at(key('t', b)).values[at] + 273.15) * std::pow(1000.0 / b, 0.2857);
                    if (!std::isfinite(ta) || !std::isfinite(tb) || (ta - theta0) * (tb - theta0) > 0.0 || ta == tb) {
                        continue;
                    }
                    const double f = (theta0 - ta) / (tb - ta);
                    const double p = std::exp(std::log(static_cast<double>(a)) + f * (std::log(static_cast<double>(b)) - std::log(static_cast<double>(a))));
                    if (g.count("ps") && p * 100.0 > g.at("ps").values[at]) {   // underground
                        break;
                    }
                    const auto lerp = [&] (char c) { return static_cast<float>(g.at(key(c, a)).values[at] + f * (g.at(key(c, b)).values[at] - g.at(key(c, a)).values[at])); };
                    rh.values[at] = lerp('r');
                    uu.values[at] = lerp('u');
                    vv.values[at] = lerp('v');
                    pr.values[at] = static_cast<float>(p);
                    break;
                }
            }
            g["irh"] = std::move(rh);
            g["ip"] = std::move(pr);
            g["iu"] = std::move(uu);
            g["iv"] = std::move(vv);
        };
        x.fill = [] (const Grids& g) { return pick(g, "irh"); };
        x.ramp = humidity();
        x.palette = "rh";
        x.fillTitle = "Relative humidity on the surface (%)";
        x.legendStep = 10;
        ContourSet c;
        c.key = "ip";
        c.interval = 50.0;
        c.title = "Pressure of the " + tag + " K surface (mb)";
        x.contours = {c};
        x.barbU = "iu";
        x.barbV = "iv";
        p.push_back(x);
    }
}
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
{   // the dynamic tropopause (the 2 PVU surface, section 14 of the plan): its pressure or its potential temperature, and the wind on it; each hemisphere its own surface (+2 PVU in the
    // north, -2 in the south). Troughs show as the tropopause folded down (high pressure, low theta) from the pole.
    for (const bool theta : {false, true}) {
        Product x;
        x.id = theta ? "dt_theta" : "dt_pressure";
        x.label = theta ? "Dynamic Tropopause (2 PVU) Potential Temperature and Wind" : "Dynamic Tropopause (2 PVU) Pressure and Wind";
        const char * north = "PV=2e-06 (Km^2/kg/s) surface";
        const char * south = "PV=-2e-06 (Km^2/kg/s) surface";
        x.wants = {want("pn", "PRES", north), want("ps", "PRES", south), want("un", "UGRD", north), want("us", "UGRD", south), want("vn", "VGRD", north), want("vs", "VGRD", south)};
        if (theta) {
            x.wants.push_back(want("tn", "TMP", north));
            x.wants.push_back(want("ts", "TMP", south));
        }
        x.derive = [theta] (Grids& g, const Context&) {
            if (!g.count("pn") || !g.count("ps")) {
                return;
            }
            const auto& pn = g.at("pn");
            auto field = pn, u = pn, v = pn;
            for (int r = 0; r < pn.rows; r++) {
                const bool northern = pn.lat0 - r * pn.step >= 0.0;
                const auto& pr = g.at(northern ? "pn" : "ps");
                const auto& ur = g.at(northern ? "un" : "us");
                const auto& vr = g.at(northern ? "vn" : "vs");
                for (int c = 0; c < pn.columns; c++) {
                    const size_t i = static_cast<size_t>(r) * static_cast<size_t>(pn.columns) + static_cast<size_t>(c);
                    const double mb = pr.values[i] * 0.01;
                    u.values[i] = ur.values[i];
                    v.values[i] = vr.values[i];
                    if (theta) {
                        const double t = g.at(northern ? "tn" : "ts").values[i];   // (degrees C)
                        field.values[i] = static_cast<float>((t + 273.15) * std::pow(1000.0 / mb, 0.2857));
                    } else {
                        field.values[i] = static_cast<float>(mb);
                    }
                }
            }
            g["dt"] = std::move(field);
            g["u"] = std::move(u);
            g["v"] = std::move(v);
        };
        x.fill = [] (const Grids& g) { return pick(g, "dt"); };
        x.ramp = theta ? Ramp{{{270, QColor{"#3b0f70"}}, {290, QColor{"#2c5fa8"}}, {305, QColor{"#3e9bcd"}}, {315, QColor{"#6fc9a8"}}, {325, QColor{"#b8e186"}}, {335, QColor{"#f6e46b"}},
                                {345, QColor{"#f9a54a"}}, {360, QColor{"#e4552c"}}, {380, QColor{"#a51f2c"}}}}
                       : Ramp{{{100, QColor{"#a51f2c"}}, {150, QColor{"#e4552c"}}, {200, QColor{"#f9a54a"}}, {250, QColor{"#f6e46b"}}, {300, QColor{"#b8e186"}}, {350, QColor{"#6fc9a8"}},
                               {400, QColor{"#3e9bcd"}}, {500, QColor{"#2c5fa8"}}, {650, QColor{"#3b0f70"}}}};   // (a low tropopause, high pressure, in the cold colors: the troughs)
        x.fillTitle = theta ? "Potential temperature on the 2 PVU surface (K)" : "Pressure of the 2 PVU surface (mb)";
        x.legendStep = 0;
        x.barbU = "u";
        x.barbV = "v";
        p.push_back(x);
    }
}
{   // a simulated infrared picture: the outgoing longwave at the top of the atmosphere as a brightness temperature, (OLR / sigma)^(1/4); the file has it as the mean of the last 3 to 6
    // hours, not an instant (section 14 of the plan)
    Product x;
    x.id = "sim_ir";
    x.label = "Simulated Infrared (outgoing longwave as brightness temperature)";
    x.wants = {want("olr", "ULWRF", "top of atmosphere")};
    x.derive = [] (Grids& g, const Context&) {
        if (!g.count("olr")) {
            return;
        }
        auto t = g.at("olr");
        for (auto& v : t.values) {
            v = v > 0.0f ? static_cast<float>(std::pow(v / 5.670374e-8, 0.25) - 273.15) : std::nanf("");
        }
        g["tb"] = std::move(t);
    };
    x.fill = [] (const Grids& g) { return pick(g, "tb"); };
    // (enhanced IR on the scale of this brightness temperature: the outgoing longwave of clear air is already -25 to 0 C (the water vapor takes some), so the clear ground is the darker greys,
    // white from -40, the cold tops in colors from -50)
    x.ramp = Ramp{{{-85, QColor{"#5a1a6e"}}, {-75, QColor{"#b02a8f"}}, {-68, QColor{"#e0302a"}}, {-62, QColor{"#f2c330"}}, {-56, QColor{"#3fae49"}}, {-50, QColor{"#2f7fd0"}},
                   {-42, QColor{"#ffffff"}}, {-32, QColor{"#bdbdbd"}}, {-20, QColor{"#7a7a7a"}}, {-8, QColor{"#454545"}}, {10, QColor{"#141414"}}}};
    x.fillTitle = "Brightness temperature (C) of the outgoing longwave, the mean of the last hours";
    x.legendStep = 0;
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
