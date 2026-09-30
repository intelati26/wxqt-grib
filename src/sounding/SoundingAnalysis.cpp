// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "sounding/SoundingAnalysis.h"
#include <cmath>

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
    a.meanMixing01 = meanMixingRatio(p, 0, 1000);
    a.lapse03 = lapseRateAgl(p, 0, 3000);
    a.lapse36 = lapseRateAgl(p, 3000, 6000);
    a.lapse700500 = lapseRateMb(p, 700, 500);
    a.lapse850500 = lapseRateMb(p, 850, 500);
    a.dcape = SoundingIndices::dcape(p);
    const double frz = temperatureLevel(p, 0.0);
    if (have(frz)) a.freezingLevelAgl = p.toAgl(p.interpHght(frz));
    const double wbz = temperatureLevel(p, 0.0, true);
    if (have(wbz)) a.wetBulbZeroAgl = p.toAgl(p.interpHght(wbz));
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
    return a;
}
