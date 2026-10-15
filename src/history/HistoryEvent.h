// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef HISTORYEVENT_H
#define HISTORYEVENT_H

#include <string>
#include <vector>
#include <QDateTime>

// An event of the history page: a time span (an outbreak can last days), any point on the map with a radius, a name and a note, and the moments worth noting inside it.
// Made by the person (dates and a point) or taken from a catalog (the catalog id is kept so it can be read again).
struct HistoryEvent {
    struct Moment {
        QDateTime time;      // UTC
        std::string label;   // "the heaviest snow", "first warning"
    };
    std::string id;          // stable: the time it was made
    std::string name;        // empty: the default name is shown
    std::string source{"user"};   // "user" or a catalog ("tornado-outbreak", "storm")
    std::string catalogId;
    QDateTime start;         // UTC
    QDateTime end;           // UTC
    double lat{0.0};
    double lon{0.0};         // east positive
    double radiusKm{100.0};
    std::string note;
    std::vector<Moment> moments;

    // "2026-01-14 12:00Z  37.24N 93.39W": what a name defaults to (datetime, latitude, longitude)
    std::string defaultName() const;
    std::string shownName() const { return name.empty() ? defaultName() : name; }
};

namespace HistoryEvents {
    std::vector<HistoryEvent> load();
    void save(const std::vector<HistoryEvent>&);
}

#endif  // HISTORYEVENT_H
