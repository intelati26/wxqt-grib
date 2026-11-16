// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

// The state of each model's source over time (section 20 of the plan): OK, Late, Failing, Down, Retired from a run of checks at made-up times, and what is kept between them.
#include <cstdio>
#include <QCoreApplication>
#include <QDir>
#include <QStandardPaths>
#include "gfs/ServerStatus.h"

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL line %d: %s\n", __LINE__, #c); failures++; } } while (0)

namespace {
    QDateTime at(const char * iso) {
        return QDateTime::fromString(iso, Qt::ISODate).toUTC();
    }
    using S = ServerStatus::State;
}

int main(int argc, char ** argv) {
    QCoreApplication app{argc, argv};
    QStandardPaths::setTestModeEnabled(true);
    QDir{QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)}.removeRecursively();

    CHECK(ServerStatus::stateOf(ServerStatus::entry("GFS"), at("2026-11-15T12:00:00Z")) == S::Unknown);
    // a model with a 6 hour cycle and 4 hours of lag: its newest run may be 13 hours old (6 + 4 + 3) before it is late
    ServerStatus::record("GFS", true, false, "2026111506", "found", 6, 4, true, at("2026-11-15T12:00:00Z"));
    auto e = ServerStatus::entry("GFS");
    CHECK(e.lastWasOk && e.newestRun == "2026111506" && e.failsInRow == 0 && e.cycleHours == 6 && e.lagHours == 4);
    CHECK(ServerStatus::stateOf(e, at("2026-11-15T12:00:00Z")) == S::Ok);
    CHECK(ServerStatus::stateOf(e, at("2026-11-15T18:30:00Z")) == S::Ok);      // 12.5 hours after the run
    CHECK(ServerStatus::stateOf(e, at("2026-11-15T20:00:00Z")) == S::Late);    // 14 hours
    // an archive's newest run is old by design: never late
    ServerStatus::record("GFS-ANL", true, false, "2014010100", "found", 6, 0, false, at("2026-11-15T12:00:00Z"));
    CHECK(ServerStatus::stateOf(ServerStatus::entry("GFS-ANL"), at("2026-11-15T12:00:00Z")) == S::Ok);
    // failures: one is Failing, two close together still Failing, two ten minutes apart Down
    ServerStatus::record("GFS", false, false, "", "no run found", 6, 4, true, at("2026-11-15T13:00:00Z"));
    e = ServerStatus::entry("GFS");
    CHECK(!e.lastWasOk && e.failsInRow == 1 && ServerStatus::stateOf(e, at("2026-11-15T13:00:00Z")) == S::Failing && e.newestRun == "2026111506");   // (the last run seen is kept)
    ServerStatus::record("GFS", false, false, "", "no run found", 6, 4, true, at("2026-11-15T13:05:00Z"));
    CHECK(ServerStatus::stateOf(ServerStatus::entry("GFS"), at("2026-11-15T13:05:00Z")) == S::Failing);
    ServerStatus::record("GFS", false, false, "", "no run found", 6, 4, true, at("2026-11-15T13:20:00Z"));
    e = ServerStatus::entry("GFS");
    CHECK(e.failsInRow == 3 && ServerStatus::stateOf(e, at("2026-11-15T13:20:00Z")) == S::Down);
    // down for a week since the last OK: retired
    CHECK(ServerStatus::stateOf(e, at("2026-11-23T13:20:00Z")) == S::Retired);
    CHECK(ServerStatus::stateOf(e, at("2026-11-20T13:20:00Z")) == S::Down);
    // a success ends it
    ServerStatus::record("GFS", true, false, "2026112400", "found", 6, 4, true, at("2026-11-24T04:00:00Z"));
    e = ServerStatus::entry("GFS");
    CHECK(e.failsInRow == 0 && e.lastWasOk && !e.firstFail.isValid() && ServerStatus::stateOf(e, at("2026-11-24T05:00:00Z")) == S::Ok);
    // Moved: found, but a fallback answered; ends when the first place answers again. Changed: the catalogue check found fields gone; ends when none is
    ServerStatus::setPlaces("GFS", "gfs-aws", "noaa-gfs-bdp-pds.s3.us-east-1.amazonaws.com");
    e = ServerStatus::entry("GFS");
    CHECK(e.server == "gfs-aws" && ServerStatus::stateOf(e, at("2026-11-24T05:00:00Z")) == S::Moved);
    ServerStatus::setPlaces("GFS", "gfs-aws", "");
    ServerStatus::setChanged("GFS", 2, "TMP@2 m, RH@300 mb");
    e = ServerStatus::entry("GFS");
    CHECK(e.goneFields == 2 && e.goneList.find("RH@300 mb") != std::string::npos && ServerStatus::stateOf(e, at("2026-11-24T05:00:00Z")) == S::Changed);
    ServerStatus::setPlaces("GFS", "gfs-aws", "other.example.org");
    CHECK(ServerStatus::stateOf(ServerStatus::entry("GFS"), at("2026-11-24T05:00:00Z")) == S::Moved);   // (moved comes before changed: the server is the thing to look at first)
    ServerStatus::setPlaces("GFS", "gfs-aws", "");
    ServerStatus::setChanged("GFS", 0, "");
    CHECK(ServerStatus::stateOf(ServerStatus::entry("GFS"), at("2026-11-24T05:00:00Z")) == S::Ok);
    // a failure comes before both
    ServerStatus::setChanged("GFS", 1, "X@y");
    ServerStatus::record("GFS", false, false, "", "no run", 6, 4, true, at("2026-11-24T06:00:00Z"));
    CHECK(ServerStatus::stateOf(ServerStatus::entry("GFS"), at("2026-11-24T06:00:00Z")) == S::Failing);
    ServerStatus::record("GFS", true, false, "2026112406", "found", 6, 4, true, at("2026-11-24T07:00:00Z"));
    ServerStatus::setChanged("GFS", 0, "");
    // a skipped check (a model that needs a storm) records nothing
    ServerStatus::record("HAFSA", false, true, "", "needs a storm", 6, 4, true, at("2026-11-15T13:00:00Z"));
    CHECK(ServerStatus::stateOf(ServerStatus::entry("HAFSA"), at("2026-11-15T13:00:00Z")) == S::Unknown);
    // the words
    CHECK(ServerStatus::ago(at("2026-11-15T12:00:00Z"), at("2026-11-15T12:00:30Z")) == "just now" && ServerStatus::ago(at("2026-11-15T12:00:00Z"), at("2026-11-15T12:30:00Z")) == "30 min ago");
    CHECK(ServerStatus::ago(at("2026-11-15T12:00:00Z"), at("2026-11-15T20:00:00Z")) == "8 h ago" && ServerStatus::ago(at("2026-11-10T12:00:00Z"), at("2026-11-15T12:00:00Z")) == "5 days ago" && ServerStatus::ago(QDateTime{}, at("2026-11-15T12:00:00Z")) == "never");
    std::printf(failures == 0 ? "all server status tests passed\n" : "%d failures\n", failures);
    return failures == 0 ? 0 : 1;
}
