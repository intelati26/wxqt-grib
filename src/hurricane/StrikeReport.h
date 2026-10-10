// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef STRIKEREPORT_H
#define STRIKEREPORT_H

#include <vector>
#include <QString>
#include "hurricane/HurricaneData.h"

// The chance of tropical storm and hurricane winds at the user's saved locations, in two ways: NHC's own five-day wind speed probabilities (a band for
// 34, 50 and 64 kt, the cycle's every storm together) and, for the storm on show, how many members of each ensemble pass within 100 km of the place at
// 34 kt or more. Words and tables only (the numbers come from UtilityWindProbability and UtilityEnsembleStats).
class StrikeReport {
public:
    // the saved locations with NHC's bands, as an HTML table; empty text when there are no locations or no probability map
    static QString nhcTable(const UtilityWindProbability::Map&);
    // the ensemble view of the saved locations for one storm (one line per location and ensemble)
    static QString ensembleLines(const std::vector<HurricaneData::EnsembleSet>&, double radiusKm = 100.0, double minWindKt = 34.0);
};

#endif  // STRIKEREPORT_H
