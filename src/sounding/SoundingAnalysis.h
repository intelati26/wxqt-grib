// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef SOUNDINGANALYSIS_H
#define SOUNDINGANALYSIS_H

#include "sounding/SoundingIndices.h"
#include "sounding/SoundingParcel.h"
#include "sounding/SoundingProfile.h"

// Everything a sounding display shows, computed once from a profile. -9999 = unavailable.
struct SoundingAnalysis {
    SoundingParcel::Parcel sb;
    SoundingParcel::Parcel ml;
    SoundingParcel::Parcel mu;
    SoundingIndices::EffectiveLayer effective;

    SoundingIndices::Wind rightMover;
    SoundingIndices::Wind leftMover;
    SoundingIndices::Wind meanWind06;
    SoundingIndices::Wind shear01, shear03, shear06;
    double effectiveShearKt{-9999.0};
    double srh01{-9999.0};
    double srh03{-9999.0};
    double effectiveSrh{-9999.0};

    double precipitableWaterIn{-9999.0};
    double meanMixingLow100{-9999.0};   // g/kg, lowest 100 mb (what SPC prints as "0-1 km mean W")
    double meanMixing03{-9999.0};       // g/kg, 0-3 km
    double lapse03{-9999.0};
    double lapse36{-9999.0};    // 3-6 km above MEAN SEA LEVEL, as SPC prints it
    double lapse700500{-9999.0};
    double lapse850500{-9999.0};
    double dcape{-9999.0};
    double convectiveTemp{-9999.0};    // C
    double freezingLevelAgl{-9999.0};   // m
    double wetBulbZeroAgl{-9999.0};     // m
    double surfaceRh{-9999.0};

    double stpFixed{-9999.0};
    double stpEffective{-9999.0};
    double supercell{-9999.0};
    double hail{-9999.0};

    static SoundingAnalysis compute(const SoundingProfile& profile);
};

#endif  // SOUNDINGANALYSIS_H
