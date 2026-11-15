// Tests of how a picture's run and forecast hour are chosen (GfsRender::runForCycle, GfsRender::hourForValidTime): from the newest run and the cycle asked for, and from a valid time, with no
// network. The newest run stepped back to a cycle, an analysis (its own run for each hour), and the messages when a valid time cannot be had.
#include <QGuiApplication>
#include <QSettings>
#include "util/MyApplication.h"
#include <cstdio>
#include "gfs/GfsModels.h"
#include "gfs/GfsRender.h"

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL line %d: %s\n", __LINE__, #c); failures++; } } while (0)

int main(int argc, char ** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app{argc, argv};
    MyApplication::preferences = new QSettings{"/tmp/wxqt_runselect_test_prefs.ini", QSettings::IniFormat};
    MyApplication::preferences->clear();
    using GfsRender::hourForValidTime;
    using GfsRender::runForCycle;
    // the cycle: the newest run itself, earlier ones the same day, and back over midnight
    {
        const GfsData::Run newest{"20261114", "12"};
        CHECK(runForCycle(newest, "12", 6).id() == "2026111412");
        CHECK(runForCycle(newest, "06", 6).id() == "2026111406");
        CHECK(runForCycle(newest, "00", 6).id() == "2026111400");
        CHECK(runForCycle(newest, "18", 6).id() == "2026111318");   // the 18Z of the day before
        CHECK(runForCycle(newest, "06", 12).id() == "2026111312" || runForCycle(newest, "06", 12).id() == "2026111412");   // a 12 hour cycle has no 06Z: nothing is found, the walk ends
        CHECK(runForCycle({"20261101", "00"}, "18", 6).id() == "2026103118");   // over a month's end
        CHECK(runForCycle(newest, "03", 3).id() == "2026111403");
        CHECK(runForCycle(newest, "12", 0).id() == "2026111412");   // a model with no cycle length given does not divide by zero
    }
    // a forecast model: the hour is the difference, and it must be one the model makes
    {
        const auto * gfs = GfsModels::find("GFS");
        CHECK(gfs != nullptr && !gfs->hours.empty());
        GfsData::Run run{"20261114", "12"};
        int hour = -1;
        std::string error;
        CHECK(hourForValidTime("GFS", run, "2026111512", hour, error) && hour == 24 && error.empty() && run.id() == "2026111412");
        CHECK(hourForValidTime("GFS", run, "2026111412", hour, error) && hour == 0);
        // before the run: no forecast, and it says so
        error.clear();
        CHECK(!hourForValidTime("GFS", run, "2026111411", hour, error) && error.find("starts after it") != std::string::npos && error.find("2026111412") != std::string::npos);
        // too far: a valid time past the end of the model's hours
        error.clear();
        CHECK(!hourForValidTime("GFS", run, "2027010100", hour, error) && !error.empty());
        // an hour the model does not make: the first range's step, off by one
        for (const auto& range : gfs->hours) {
            if (range.step > 1) {
                error.clear();
                const int off = range.from + 1;
                const auto valid = QDateTime::fromString("2026111412", "yyyyMMddHH").addSecs(static_cast<qint64>(off) * 3600).toString("yyyyMMddHH").toStdString();
                CHECK(!hourForValidTime("GFS", run, valid, hour, error) && error.find("does not make") != std::string::npos);
                break;
            }
        }
    }
    // a model the registry does not know: any hour from the run on
    {
        GfsData::Run run{"20261114", "00"};
        int hour = -1;
        std::string error;
        CHECK(hourForValidTime("NOT-A-MODEL", run, "2026111503", hour, error) && hour == 27);
    }
    // an analysis is its own run: the run becomes the run of the valid time
    {
        bool found = false;
        for (const auto& def : GfsModels::all()) {
            if (def.hourlyRuns && def.hours.size() == 1 && def.hours.front().to == 0) {
                GfsData::Run run{"20261101", "00"};
                int hour = -1;
                std::string error;
                CHECK(hourForValidTime(def.id, run, "2026111407", hour, error) && hour == 0 && run.id() == "2026111407");
                found = true;
                break;
            }
        }
        CHECK(found);
    }
    std::printf(failures == 0 ? "OK\n" : "FAILED: %d\n", failures);
    return failures == 0 ? 0 : 1;
}
