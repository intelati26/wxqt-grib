// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef CHARTHOURS_H
#define CHARTHOURS_H

#include <map>
#include <string>
#include <vector>
#include <QtGlobal>
#include "gfs/GfsRender.h"

// Which forecast hours a model chart has for a run: one answer for every screen that shows model charts (the Model Viewer, the master map's chart layer), so the hours are the same on both.
// Three parts: the rules of the model and the chart (a run's length, a chart of a period that ends at the hour), what the run's data really has (the run's indexes, or SPC's store time
// axes), and a book that keeps the answers (and asks again while hours are still missing).
namespace ChartHours {
    struct Rules {
        std::vector<int> hours;      // from the list given: the ones this run of the model has and this chart can be of
        bool periodCut{false};       // the chart is of a period that ends at the hour, and some hours were left out for that
        int period{0}, endHourUtc{-1};
    };
    // `hours` is the model's own list; `cycle` the hour of the run (0-23)
    Rules rules(const std::string& model, const std::string& param, int cycle, const std::vector<int>& hours);
    // the model's own hours, cut at the length of the run of this cycle
    std::vector<int> modelHours(const std::string& model, int cycle);

    // Of `hours`, the ones the run's data has the chart for (blocking: call it from a thread). SPC's store: its fields' time axes; the other GRIB models: every record the chart needs is in the
    // index of the hour. Empty: nothing could be told (no connection, the run not there), so the caller keeps the hours it has.
    std::vector<int> present(GfsRender::Session& session, const std::string& model, const std::string& param, const std::string& runId, const std::vector<int>& hours, const std::string& storm = "");

    // the answers kept: by model, run, chart (and storm); an answer that left hours out is asked again after five minutes (a run still being uploaded)
    class Book {
    public:
        struct Entry {
            std::vector<int> hours;   // empty: nothing could be told
            qint64 when{0};
            bool complete{false};     // every hour asked about is there: no need to ask again
        };
        static std::string key(const std::string& model, const std::string& runId, const std::string& param, const std::string& storm = "");
        const Entry * find(const std::string& key) const;
        bool wantsAsking(const std::string& key) const;   // not known, or incomplete and old
        bool begin(const std::string& key);                // false when it is being asked already
        void put(const std::string& key, const std::vector<int>& present, size_t asked);
    private:
        std::map<std::string, Entry> entries;
        std::map<std::string, bool> busy;
    };
}

#endif  // CHARTHOURS_H
