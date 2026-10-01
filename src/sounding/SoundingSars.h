// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef SOUNDINGSARS_H
#define SOUNDINGSARS_H

#include <string>
#include <vector>

// SARS, the Sounding Analogue System: a port of SHARPpy's databases/sars.py (BSD licence, docs/sharppy-notice.md). The
// databases (supercell: Rich Thompson, hail: Ryan Jewell, both NOAA SPC) are compiled in (SoundingSarsData.cpp, made by
// resourceCreation/sars/gen_sars_data.py). A sounding is matched on a few parameters; loose matches give the probability,
// tighter "quality" matches are listed.
namespace SoundingSars {
    struct SupercellRecord {
        const char * id;   // "yymmddhh.SITE"
        int category;      // 0 non-tornadic, 1 weak, 2 significant tornado
        float mlcape;
        float mllcl;
        float srh1;
        float shear6;      // kt
        float temp500;
        float lapse75;
        float shear3;      // kt
        float shear9;      // kt
        float srh3;
    };

    struct HailRecord {
        const char * id;
        float size;        // inches
        float mucape;
        float mumr;
        float temp500;
        float lapse75;
        float shear3;      // m/s
        float shear6;
        float shear9;
        float srh3;
    };

    const std::vector<SupercellRecord>& supercellDatabase();
    const std::vector<HailRecord>& hailDatabase();

    struct Match {
        std::string id;
        std::string label;   // "SIGTOR" / "WEAKTOR" / "NONTOR" or the hail size
        double size{0.0};
        int category{0};
    };

    struct Result {
        std::vector<Match> quality;   // at most 15 for hail, as SHARPpy
        int looseMatches{0};
        int significant{0};           // significant tornadoes (weak + sig, as SHARPpy's count) / hail reports >= 2 in
        double probability{0.0};
        bool valid{false};
    };

    // shear in kt; SRH in m2/s2; temperature C; lapse rate C/km; CAPE J/kg; LCL m AGL
    Result supercell(double mlcape, double mllcl, double temp500, double lapse, double shear6, double srh1, double shear3,
                     double shear9, double srh3);
    // shear in m/s; mixing ratio g/kg
    Result hail(double mumr, double mucape, double temp500, double lapse, double shear6, double shear9, double shear3, double srh3);
}

#endif  // SOUNDINGSARS_H
