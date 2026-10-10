// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef DROPPROFILE_H
#define DROPPROFILE_H

#include <string>
#include "hurricane/UtilityDropsonde.h"
#include "sounding/SoundingProfile.h"

// A decoded dropsonde as the sounding screen's profile: the levels lowest first, heights filled in between the reported ones (the mandatory levels
// carry heights, the significant levels do not: interpolated linearly in the log of pressure, the surface at 0 m MSL), levels above the highest
// known height left out.
class DropProfile {
public:
    static bool build(const UtilityDropsonde::Drop&, SoundingProfile& out, std::string& error);
    static std::string title(const UtilityDropsonde::Drop&);   // "Dropsonde NOAA9 01BBA SURV OB 32  07 Oct 00:30Z  28.1N 84.6W"
};

#endif  // DROPPROFILE_H
