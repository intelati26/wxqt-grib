// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "space/SpaceData.h"
#include <future>
#include "util/UtilityIO.h"

void SpaceData::load(Bundle& bundle) {
    const auto get = [] (const char * path) {
        return std::async(std::launch::async, [path] { return UtilityIO::downloadAsByteArray(SpaceData::url(path)).toStdString(); });
    };
    auto scales = get("products/noaa-scales.json");
    auto kp = get("products/noaa-planetary-k-index-forecast.json");
    auto xray = get("json/goes/primary/xrays-1-day.json");
    auto wind = get("json/rtsw/rtsw_wind_1m.json");
    auto mag = get("json/rtsw/rtsw_mag_1m.json");
    auto flare = get("json/goes/primary/xray-flares-latest.json");
    auto alerts = get("products/alerts.json");
    auto proton = get("json/goes/primary/integral-protons-1-day.json");
    auto electron = get("json/goes/primary/integral-electrons-1-day.json");
    auto cycle = get("json/solar-cycle/observed-solar-cycle-indices.json");
    auto predicted = get("json/solar-cycle/predicted-solar-cycle.json");
    const auto note = [&bundle] (const char * name) {
        bundle.problems += (bundle.problems.empty() ? "" : ", ") + std::string{name};
    };
    bundle.scales = UtilitySpace::parseScales(scales.get());
    if (bundle.scales.empty()) note("storm scales");
    bundle.kp = UtilitySpace::parseKp(kp.get());
    if (bundle.kp.empty()) note("Kp index");
    bundle.xray = UtilitySpace::parseXray(xray.get(), 5);
    if (bundle.xray.empty()) note("X-ray flux");
    bundle.wind = UtilitySpace::parseWind(wind.get(), 5);
    if (bundle.wind.empty()) note("solar wind");
    bundle.mag = UtilitySpace::parseMag(mag.get(), 5);
    if (bundle.mag.empty()) note("magnetic field");
    bundle.flare = UtilitySpace::parseFlare(flare.get());
    bundle.proton = UtilitySpace::parseFlux(proton.get(), ">=10 MeV");
    bundle.electron = UtilitySpace::parseFlux(electron.get(), ">=2 MeV");
    if (bundle.proton.empty() && bundle.electron.empty()) note("particle flux");
    bundle.cycle = UtilitySpace::parseCycleObserved(cycle.get(), 1990);
    bundle.predicted = UtilitySpace::parseCyclePredicted(predicted.get());
    if (bundle.cycle.empty()) note("solar cycle");
    bundle.alerts = UtilitySpace::parseAlerts(alerts.get(), 6);
}

UtilitySpace::Ovation SpaceData::loadOvation() {
    return UtilitySpace::parseOvation(UtilityIO::downloadAsByteArray(url("json/ovation_aurora_latest.json")).toStdString());
}
