// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef SOUNDINGADVECTION_H
#define SOUNDINGADVECTION_H

#include <vector>
#include "sounding/SoundingProfile.h"

// Inferred temperature advection, a port of SHARPpy's params.inferred_temp_adv (BSD licence, see docs/sharppy-notice.md):
// the thermal-wind form of the geostrophic temperature advection (Bluestein, Synoptic-Dynamic Meteorology in
// Midlatitudes, eq. 4.1.139), for 100 mb layers from the surface up. SHARPpy notes that its values are much smaller than
// the ones on SPC's sounding graphic.
namespace SoundingAdvection {
    struct Layer {
        double pBottom{0.0};   // mb
        double pTop{0.0};
        double advection{0.0};   // C per hour; positive = warm advection (veering winds with height)
    };

    // `latitude` in degrees north (SHARPpy's default is 35); empty when the profile has no winds
    std::vector<Layer> inferred(const SoundingProfile& profile, double latitude = 35.0);
}

#endif  // SOUNDINGADVECTION_H
