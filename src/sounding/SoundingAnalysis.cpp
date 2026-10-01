// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "sounding/SoundingAnalysis.h"
#include <cmath>
#include "sounding/SoundingThermo.h"

using namespace SoundingIndices;

namespace {
    constexpr double knotsToMs = 0.514444;
    bool have(double v) { return v > -9998.0; }
}

SoundingAnalysis SoundingAnalysis::compute(const SoundingProfile& p) {
    using Kind = SoundingParcel::Kind;
    SoundingAnalysis a;
    a.sb = SoundingParcel::lift(p, Kind::SurfaceBased);
    a.ml = SoundingParcel::lift(p, Kind::MixedLayer);
    a.mu = SoundingParcel::lift(p, Kind::MostUnstable);
    a.effective = effectiveInflowLayer(p, a.mu);
    a.rightMover = stormMotion(p, a.mu, a.effective, false);
    a.leftMover = stormMotion(p, a.mu, a.effective, true);
    a.meanWind06 = meanWind(p, 0, 6000);
    a.shear01 = bulkShear(p, 0, 1000);
    a.shear03 = bulkShear(p, 0, 3000);
    a.shear06 = bulkShear(p, 0, 6000);
    a.srh01 = helicity(p, 0, 1000, a.rightMover);
    a.srh03 = helicity(p, 0, 3000, a.rightMover);

    double effShearMs = 0.0;
    if (a.effective.valid) {
        a.effectiveSrh = helicity(p, a.effective.botAgl, a.effective.topAgl, a.rightMover);
        a.effectiveShearKt = effectiveBulkShear(p, a.mu, a.effective);
        if (have(a.effectiveShearKt)) effShearMs = a.effectiveShearKt * knotsToMs;
    }
    a.precipitableWaterIn = precipitableWater(p);
    a.meanMixingLow100 = meanMixingRatioMb(p, p.sfcPres(), p.sfcPres() - 100.0);
    a.meanMixing03 = meanMixingRatio(p, 0, 3000);
    a.lapse03 = lapseRateAgl(p, 0, 3000);
    // SPC's printed 3-6 km lapse rate is between 3 and 6 km above sea level (its 0-3 km is above ground);
    // follow SPC so the numbers agree, and label it MSL on screen
    a.lapse36 = lapseRateMb(p, p.interpPresAtHght(3000.0), p.interpPresAtHght(6000.0));
    a.lapse700500 = lapseRateMb(p, 700, 500);
    a.lapse850500 = lapseRateMb(p, 850, 500);
    a.dcape = SoundingIndices::dcape(p);
    a.convectiveTemp = convectiveTemperature(p);
    // Freezing / wet-bulb-zero heights. As on SPC's soundings: a surface already at or below 0 C has no
    // melting level (0 m when the surface is exactly 0 C), even if a shallow warm layer sits above it.
    const size_t s0 = static_cast<size_t>(p.sfc);
    auto zeroLevel = [&] (double surfaceValue, bool wetBulb) {
        if (!have(surfaceValue)) return SoundingThermo::missing;
        if (surfaceValue == 0.0) return 0.0;
        if (surfaceValue < 0.0) return SoundingThermo::missing;
        const double pr = temperatureLevel(p, 0.0, wetBulb);
        return have(pr) ? p.toAgl(p.interpHght(pr)) : SoundingThermo::missing;
    };
    a.freezingLevelAgl = zeroLevel(p.tmpc[s0], false);
    a.wetBulbZeroAgl = zeroLevel(p.wetbulb[s0], true);
    a.surfaceRh = surfaceRelativeHumidity(p);

    if (a.shear06.valid() && have(a.srh01) && a.sb.valid) {
        a.stpFixed = SoundingIndices::stpFixed(a.sb.cape, a.sb.lclHght, a.srh01, a.shear06.speed() * knotsToMs);
    }
    if (a.effective.valid && have(a.effectiveSrh) && a.ml.valid) {
        a.stpEffective = SoundingIndices::stpEffective(a.ml.cape, a.effectiveSrh, effShearMs, a.ml.lclHght, a.ml.cin);
        a.supercell = supercellComposite(a.mu.cape, a.effectiveSrh, effShearMs);
    } else {
        a.stpEffective = 0.0;
        a.supercell = 0.0;
    }
    a.hail = significantHail(p, a.mu);

    // the rest of SPC's index list, from SHARPpy's params
    a.kIndex = SoundingIndices::kIndex(p);
    a.totalTotals = SoundingIndices::totalTotals(p);
    if (!SoundingThermo::isMissing(p.sfcPres())) {
        a.lowRh = SoundingIndices::meanRelativeHumidity(p, p.sfcPres(), p.sfcPres() - 100.0);
        a.midRh = SoundingIndices::meanRelativeHumidity(p, p.sfcPres() - 150.0, p.sfcPres() - 350.0);
    }
    if (a.ml.valid) {
        a.esp = SoundingIndices::esp(a.ml.cape3km, a.ml.cape, a.lapse03);
        a.wndg = SoundingIndices::wndg(p, a.ml.cape, a.ml.cin, a.lapse03);
        if (a.shear06.valid()) a.sigSevere = SoundingIndices::sigSevere(a.ml.cape, a.shear06.speed());
    }
    if (a.mu.valid) a.mmp = SoundingIndices::mmp(p, a.mu.cape);

    // SARS analogues (SHARPpy's profile.get_sars): needs the mixed-layer and most-unstable parcels and the shears / SRH
    const auto shear03 = bulkShear(p, 0, 3000);
    const auto shear09 = bulkShear(p, 0, 9000);
    const double temp500 = p.interpTemp(500.0);
    if (a.ml.valid && a.mu.valid && shear03.valid() && shear09.valid() && a.shear06.valid() && have(a.srh01) && have(a.srh03) &&
        have(temp500) && have(a.lapse700500) && have(a.mu.lplPres) && have(a.mu.lplDwpt)) {
        const double mumr = SoundingThermo::mixingRatio(a.mu.lplPres, a.mu.lplDwpt);
        a.sarsHail = SoundingSars::hail(mumr, a.mu.cape, temp500, a.lapse700500, a.shear06.speed() * knotsToMs,
                                        shear09.speed() * knotsToMs, shear03.speed() * knotsToMs, a.srh03);
        a.sarsSupercell = SoundingSars::supercell(a.ml.cape, a.ml.lclHght, temp500, a.lapse700500, a.shear06.speed(), a.srh01,
                                                  shear03.speed(), shear09.speed(), a.srh03);
    }
    return a;
}
