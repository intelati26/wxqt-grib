// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef UTILITYMODELSOUNDING_H
#define UTILITYMODELSOUNDING_H

#include <string>
#include "sounding/SoundingProfile.h"

// A model sounding: the RRFS column at one point and one forecast hour, built from
// the isobaric file (prslev: temperature, dewpoint, height and wind every 25 mb from
// 1000 to 100 mb) plus the surface fields of the 2-D file (2 m T/Td, 10 m wind,
// surface pressure, terrain height). Levels below the model ground are dropped and
// the surface becomes the lowest level, as in an observed sounding.
//
// Every field is a whole CONUS grid, so one forecast hour is roughly 265 MB. The
// fetched records are cached by UtilityGrib, so further points at the same run and
// hour cost no download.
namespace UtilityModelSounding {
    // `forecastHour` as a plain number string ("1", "06"); `dateStr`/`cycle` as UtilityGrib::getLatestRun.
    // On failure returns false and `status` says why (shown to the user, never silent).
    bool buildProfile(const std::string& dateStr, const std::string& cycle, const std::string& forecastHour,
                      double lon, double lat, SoundingProfile& out, std::string& status);
}

#endif  // UTILITYMODELSOUNDING_H
