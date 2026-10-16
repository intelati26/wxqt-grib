// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef HISTORYLAYERSETS_H
#define HISTORYLAYERSETS_H

#include <QString>
#include <string>
#include <vector>
#include "history/HistoryEvent.h"

// The sets of master-map layers the History page opens an event with: what suits a storm day, a tropical cyclone, a climate day. The event's kind (its catalog) picks one to begin with and
// the person can pick another. A new set is one more line in all().
namespace HistoryLayerSets {
    struct Set {
        std::string id;
        QString label;
        std::vector<std::string> layers;     // master map layer ids
        double minimumRadiusKm;              // the view is at least this wide
    };
    const std::vector<Set>& all();
    int defaultFor(const HistoryEvent&);     // the index of the set the event's kind starts with
}

#endif  // HISTORYLAYERSETS_H
