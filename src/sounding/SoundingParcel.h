// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef SOUNDINGPARCEL_H
#define SOUNDINGPARCEL_H

#include "sounding/SoundingProfile.h"

namespace SoundingParcel {
    enum class Kind { SurfaceBased, MixedLayer, MostUnstable };

    // One lifted parcel and what it does. Heights are metres above ground level,
    // pressures millibars; -9999 = not found / not applicable.
    struct Parcel {
        double lplPres{-9999.0};     // where the parcel starts
        double lplTemp{-9999.0};
        double lplDwpt{-9999.0};
        double lclPres{-9999.0};
        double lclHght{-9999.0};
        double lfcPres{-9999.0};
        double lfcHght{-9999.0};
        double elPres{-9999.0};
        double elHght{-9999.0};
        double cape{0.0};            // J/kg
        double cin{0.0};             // J/kg (negative or zero)
        double cape3km{0.0};         // CAPE within the lowest 3 km AGL
        double liftedIndex500{-9999.0};   // C: environment minus parcel at 500 mb
        double liftedIndex300{-9999.0};
        bool valid{false};
    };

    Parcel lift(const SoundingProfile& profile, Kind kind);
    // lifts a parcel of the given starting pressure / temperature / dewpoint (any level)
    Parcel liftFrom(const SoundingProfile& profile, double pres, double tmpc, double dwpc);
    // the lowest-`depthMb` mean-layer parcel start values, shared with other code
    void mixedLayerStart(const SoundingProfile& profile, double depthMb, double& pres, double& temp, double& dwpt);
}

#endif  // SOUNDINGPARCEL_H
