// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef SOUNDINGPRECIP_H
#define SOUNDINGPRECIP_H

#include <string>
#include "sounding/SoundingProfile.h"

// The "best guess" surface precipitation type SPC prints on its sounding graphic, from a port of SHARPpy's
// sharptab/watch_type.py (init_phase, posneg_temperature, best_guess_precip; adapted there from SHARP code donated by
// Rich Thompson, SPC): find the precipitation source layer (the highest 50 mb layer below 5 km AGL saturated at top and
// bottom), take its phase from its temperature, integrate the warm and cold areas of the temperature profile below it, and
// decide among rain, snow, sleet, freezing rain and mixtures. See docs/sharppy-notice.md.
namespace SoundingPrecip {
    struct Result {
        std::string type;               // "Rain", "Snow", "Sleet", "Freezing Rain", "Freezing Rain/Drizzle", "Sleet and Snow", "None", "Unknown"
        int phase{-1};                  // of the source layer: 0 rain, 1 freezing rain / mix, 3 snow, -1 none found
        double sourcePressure{-9999.0}; // mb, mid-level of the source layer
        double sourceTemp{-9999.0};     // C
        double positiveArea{0.0};       // J/kg, warm layer below the source
        double negativeArea{0.0};       // J/kg, cold layer below that
        double surfaceTempC{-9999.0};
    };

    Result bestGuess(const SoundingProfile& profile);
}

#endif  // SOUNDINGPRECIP_H
