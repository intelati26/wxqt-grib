// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef UTILITYWATCHPROBABILITIES_H
#define UTILITYWATCHPROBABILITIES_H

#include <QString>
#include <vector>

// The SPC's watch hazard probabilities (the page "Watch Hazard Probabilities" of each watch, ww####_prob.html): the forecaster's chance of 2 or more tornadoes, of 1 or more strong (EF2-EF5)
// tornadoes, of 10 or more severe wind events, of 1 or more wind events over 65 kt, of 10 or more severe hail events, of 1 or more hailstones over 2 inches, and of 6 or more combined severe hail /
// wind events, each as a level (Low, Mod, High) and a per cent.
namespace UtilityWatchProbabilities {
    struct Hazard {
        QString group;      // "Tornadoes", "Wind", "Hail", "Combined Severe Hail/Wind"
        QString event;      // "2 or more tornadoes"
        QString level;      // "Low", "Mod", "High"
        QString percent;    // "40", "<5", ">95"
        int value{-1};      // the number in it
    };
    // the hazards of watch number `number`, in the order the page lists them; empty when the page could not be read (or the watch has none)
    std::vector<Hazard> fetch(int number);
    // the same from the text of the page (for the tests)
    std::vector<Hazard> parse(const QString& html);
}

#endif  // UTILITYWATCHPROBABILITIES_H
