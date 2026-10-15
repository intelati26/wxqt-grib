// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef UTILITYBUFKIT_H
#define UTILITYBUFKIT_H

#include <string>
#include <vector>
#include "sounding/SoundingProfile.h"

// BUFKIT model sounding files: one small text file per station and run holding every forecast hour of the model's column
// (Penn State's feed: https://www.meteo.psu.edu/bufkit/data/<MODEL>/<cycle>/<model>_<station>.buf, about 0.4 MB for RRFS).
namespace UtilityBufkit {
    struct Run {
        std::string model;                     // "RRFS", "HRRR", "RAP", "NAM", "NAMNEST", "GFS"
        std::string station;                   // "KSGF"
        double latitude{0.0};
        double longitude{0.0};
        double elevation{0.0};                 // m
        std::string cycle;                     // "12": the run's hour (UTC)
        std::string runTime;                   // "261015/1200"
        std::vector<int> hours;                // forecast hour of each profile
        std::vector<std::string> validTimes;   // "261015/1800" per profile
        std::vector<SoundingProfile> profiles;
    };
    // the text of one BUFKIT file
    bool parse(const std::string& text, Run& out, std::string& error);
    // the newest run published for the station: the file of each of the most recent cycles is read and the later run kept
    bool fetchLatest(const std::string& model, const std::string& station, Run& out, std::string& error);
}

#endif  // UTILITYBUFKIT_H
