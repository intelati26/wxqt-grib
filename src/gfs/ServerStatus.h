// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef SERVERSTATUS_H
#define SERVERSTATUS_H

#include <string>
#include <QDateTime>

// What the checks of each model's source have found over time (section 20 of docs/plans.md): the Settings > Model Sources check (GfsRender::checkSource) writes each result here, and the page
// reads the state back, so a model is not only "OK" or "PROBLEM" now but Late, Down or Retired, and says when it was last OK. Kept in `servers-status.json` in the program's data folder.
//   OK       the last check found the newest run, and the run is not older than a run of this model should be (its cycle, its lag, three hours of grace)
//   Late     found, but the newest run is older than that: the model is behind (a delay on the server, a skipped cycle)
//   Failing  the last check failed (the first one: it may be the connection)
//   Down     the last two checks or more failed, ten minutes or more apart
//   Retired  down, and last OK more than seven days ago: the files moved or the product was retired (the sign of a server change, section 20)
//   Moved    found, but not at the first place of its server: a fallback answered (the first base failed; section 20)
//   Changed  found, but the last catalogue check listed fields the charts ask for that were in the files before and are not now (CatalogCheck: Gone)
//   Unknown  never checked
namespace ServerStatus {
    enum class State { Unknown, Ok, Late, Moved, Changed, Failing, Down, Retired };
    struct Entry {
        std::string model;
        std::string newestRun;       // "2026111500" at the last check that found one
        std::string detail;          // what the last check said
        QDateTime lastCheck, lastOk, firstFail;   // UTC; invalid when never
        int failsInRow{0};
        bool lastWasOk{false};
        int cycleHours{6}, lagHours{0};   // what the model's runs are like, as the check saw them
        std::string server;          // the name of the server (Servers) the model reads from, "" for one that has none
        std::string movedTo;         // the host that answered when the first place did not, "" when the first did
        int goneFields{0};           // fields at the last catalogue check that were found before and are gone
        std::string goneList;        // the first few of them, as "variable@level"
        bool expectRecent{true};          // false for an archive (its newest run is old by design: never Late)
    };
    // The state from what is kept: a pure function of the entry and the time.
    State stateOf(const Entry& entry, const QDateTime& now);
    std::string name(State state);
    // One check's result: a failure adds to the run of failures, a success ends it. `skipped` (a model that needs a storm) records nothing.
    void record(const std::string& model, bool ok, bool skipped, const std::string& run, const std::string& detail, int cycleHours, int lagHours, bool expectRecent, const QDateTime& now = QDateTime::currentDateTimeUtc());
    // What the check found out about the places (`server`, `movedTo`), kept with the next record; and what the catalogue check found gone.
    void setPlaces(const std::string& model, const std::string& server, const std::string& movedTo);
    void setChanged(const std::string& model, int gone, const std::string& list);
    // What is kept for a model (an entry with no checks when there is none).
    Entry entry(const std::string& model);
    // "3 h ago", "2 days ago", "never"
    std::string ago(const QDateTime& when, const QDateTime& now = QDateTime::currentDateTimeUtc());
    std::string file();
}

#endif  // SERVERSTATUS_H
