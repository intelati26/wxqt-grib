// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef SOUNDINGTORNADOPROB_H
#define SOUNDINGTORNADOPROB_H

// The look-up tables of SHARPpy's STP panel (viz/stp.py, BSD licence, docs/sharppy-notice.md): the chance that a tornado near a
// supercell sounding is EF2 or stronger (sample climatology 0.15), given one parameter at a time. Each returns the probability
// and the index (0..5) of SHARPpy's alert colour list; negative inputs / missing data give probability -1.
namespace SoundingTornadoProb {
    struct Result {
        double probability{-1.0};
        int colour{0};   // 0 brown, 1 dark yellow, 2 white, 3 yellow, 4 red, 5 magenta
    };
    Result fromMlCape(double capeJkg);
    Result fromMlLcl(double lclMetres);
    Result fromEffectiveSrh(double srh);
    Result fromEffectiveShear(double shearKt);
    Result fromStpEffective(double stp);
    Result fromStpFixed(double stp);
}

#endif  // SOUNDINGTORNADOPROB_H
