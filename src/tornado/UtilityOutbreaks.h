// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef UTILITYOUTBREAKS_H
#define UTILITYOUTBREAKS_H

#include <QDateTime>
#include <string>
#include <vector>
#include "tornado/UtilityTornado.h"

// Tornado outbreaks read out of the SPC tornado database: tornadoes joined into one group while the next one starts within a few hours and a few hundred kilometers of the group's
// recent ones, so an outbreak that goes on for days is one group (the chain is cut at a week). No network: the caller passes the database's rows.
namespace UtilityOutbreaks {
    struct Outbreak {
        QDateTime start;                      // UTC, the first tornado's start
        QDateTime end;                        // UTC, the last tornado's start
        int count{0};
        int significant{0};                   // rated 2 or more
        int strongest{-9};                    // the highest rating (0..5; -9 none rated)
        int fatalities{0};
        int injuries{0};
        double centerLat{0.0};
        double centerLon{0.0};
        double radiusKm{0.0};                 // from the centre to the farthest start
        double lengthMiles{0.0};              // all the tracks added up
        std::vector<std::string> states;      // by number of tornadoes, most first
        QDateTime strongestTime;              // when the strongest touched down
        std::string strongestState;
        QDateTime busiestHour;                // the hour with most touchdowns
        int busiestCount{0};
        std::string year() const { return start.toString("yyyy").toStdString(); }
        std::string id() const { return "tornado-outbreak:" + start.toString("yyyyMMddHHmm").toStdString(); }
        std::string title() const;            // "Apr 25-28 2011: 214 tornadoes, EF5"
        std::string summary() const;          // the note of an event made from it
    };
    // the groups of at least `minimumCount` tornadoes, oldest first
    std::vector<Outbreak> find(const std::vector<UtilityTornado::Tornado>& rows, int minimumCount = 10);
    QDateTime utcOf(const UtilityTornado::Tornado&);   // CST (3), GMT (9) or unknown (taken as CST)
}

#endif  // UTILITYOUTBREAKS_H
