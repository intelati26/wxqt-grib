// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef SOUNDINGINDICES_H
#define SOUNDINGINDICES_H

#include "sounding/SoundingParcel.h"
#include "sounding/SoundingProfile.h"

// Non-parcel sounding parameters: wind shear, storm motion, helicity, moisture and
// lapse rates. Heights are metres above ground level, winds knots, -9999 = missing.
namespace SoundingIndices {
    struct Wind {
        double u{-9999.0};
        double v{-9999.0};
        bool valid() const { return u > -9998.0 && v > -9998.0; }
        double speed() const;
        double direction() const;   // meteorological: degrees the wind blows FROM
    };

    // pressure at a height above ground level
    double presAtAgl(const SoundingProfile& profile, double hAgl);

    Wind windAtAgl(const SoundingProfile& profile, double hAgl);
    // vector wind difference (top minus bottom) between two heights AGL
    Wind bulkShear(const SoundingProfile& profile, double fromAgl, double toAgl);
    // pressure-weighted mean wind between two heights AGL
    Wind meanWind(const SoundingProfile& profile, double fromAgl, double toAgl);
    // Bunkers ID storm motion for the right (and, via `left`, left) moving supercell
    Wind bunkersRight(const SoundingProfile& profile);
    Wind bunkersLeft(const SoundingProfile& profile);
    // storm-relative helicity in m2/s2 between two heights AGL for storm motion (stormU, stormV) in knots
    double helicity(const SoundingProfile& profile, double fromAgl, double toAgl, const Wind& storm);

    // precipitable water, inches, from the surface up to `topMb`
    double precipitableWater(const SoundingProfile& profile, double topMb = 400.0);
    // mean mixing ratio (g/kg) between two heights AGL / two pressures, SHARPpy's exact form
    double meanMixingRatio(const SoundingProfile& profile, double fromAgl, double toAgl);
    double meanMixingRatioMb(const SoundingProfile& profile, double bottomMb, double topMb);
    double mixingRatioAt(const SoundingProfile& profile, double pMb);
    // temperature change (top minus bottom, C) and lapse rate (C/km, positive = cooling with height)
    double deltaT(const SoundingProfile& profile, double fromMb, double toMb);
    double lapseRateMb(const SoundingProfile& profile, double fromMb, double toMb);
    double lapseRateAgl(const SoundingProfile& profile, double fromAgl, double toAgl);
    double surfaceRelativeHumidity(const SoundingProfile& profile);
    // SHARPpy params.k_index, t_totals (vertical + cross totals), mean_relh (pressure weighted, 1 mb steps), esp, wndg,
    // sig_severe (Craven and Brooks 2004) and mmp (Coniglio et al. 2006)
    double kIndex(const SoundingProfile& profile);
    double totalTotals(const SoundingProfile& profile);
    double meanRelativeHumidity(const SoundingProfile& profile, double bottomMb, double topMb);
    double esp(double mlCape3km, double mlCape, double lapse03);
    double wndg(const SoundingProfile& profile, double mlCape, double mlCin, double lapse03);
    double sigSevere(double mlCape, double shear06Kt);
    double mmp(const SoundingProfile& profile, double muCape);
    // SHARPpy winds.corfidi_mcs_motion: the Corfidi (meso-beta element) upshear and downshear vectors, knots
    struct Corfidi {
        Wind upshear;
        Wind downshear;
        bool valid() const { return upshear.valid() && downshear.valid(); }
    };
    Corfidi corfidi(const SoundingProfile& profile);
    // SHARPpy winds.critical_angle (Esterheld and Giuliano 2008): the angle, degrees, between the 0-500 m shear vector and
    // the vector from the surface wind to the storm motion
    double criticalAngle(const SoundingProfile& profile, const Wind& stormMotion);
    // Theta-E Index as SHARPpy's params.tei: the maximum minus the minimum theta-e (K) in the lowest 400 mb
    double thetaEIndex(const SoundingProfile& profile);

    // ---- effective inflow layer and what depends on it (Thompson et al. 2007) ----
    struct EffectiveLayer {
        bool valid{false};
        double pBot{-9999.0};
        double pTop{-9999.0};
        double botAgl{-9999.0};
        double topAgl{-9999.0};
    };
    EffectiveLayer effectiveInflowLayer(const SoundingProfile& profile, const SoundingParcel::Parcel& mostUnstable,
                                        double minCape = 100.0, double maxCin = -250.0);
    // Bunkers storm motion: built on the effective inflow layer when there is one, else the 0-6 km method
    Wind stormMotion(const SoundingProfile& profile, const SoundingParcel::Parcel& mostUnstable,
                     const EffectiveLayer& layer, bool leftMover = false);
    // bulk wind difference from the effective-layer base to half the depth up to the MU equilibrium level, kt
    double effectiveBulkShear(const SoundingProfile& profile, const SoundingParcel::Parcel& mostUnstable,
                              const EffectiveLayer& layer);

    // pressure (mb) of the first level where the temperature (or wet bulb) reaches `tempC`; -9999 if never
    double temperatureLevel(const SoundingProfile& profile, double tempC, bool wetBulb = false);
    double dcape(const SoundingProfile& profile);
    // convective temperature (C): the surface temperature at which a parcel with the lowest-100 mb mean
    // moisture has no CIN left; -9999 when it would need more than 25 C of heating
    double convectiveTemperature(const SoundingProfile& profile);

    // composites, each from the published definitions (inputs in m/s and metres as SPC defines them)
    double stpFixed(double sbCape, double sbLclM, double srh01, double bwd6Ms);
    double stpEffective(double mlCape, double effSrh, double effBwdMs, double mlLclM, double mlCin);
    double supercellComposite(double muCape, double effSrh, double effBwdMs);
    double significantHail(const SoundingProfile& profile, const SoundingParcel::Parcel& mostUnstable);
}

#endif  // SOUNDINGINDICES_H
