// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "gfs/Cfs30Loader.h"
#include <algorithm>
#include <cmath>
#include <future>
#include <semaphore>
#include <vector>
#include <QDateTime>
#include <QTimeZone>
#include "cfs/UtilityCfs.h"
#include "gfs/GfsData.h"
#include "util/Utility.h"

namespace {
    bool loadDays(int length, const std::string& key, GfsGrid::Grid& out, std::string& error) {
        const auto * variable = UtilityCfs::find(key);
        if (variable == nullptr) {
            error = "the CFS has no field " + key;
            return false;
        }
        length = std::clamp(length, 3, 180);
        UtilityCfs::Run run;
        if (!UtilityCfs::latestRun(run)) {
            error = "no CFSv2 run found";
            return false;
        }
        const int baseline = Utility::readPrefInt("MM_CFS_BASE", 1999) == 1982 ? 1982 : 1999;
        const auto tomorrow = QDateTime{QDateTime::currentDateTimeUtc().date().addDays(1), QTime{0, 0}, QTimeZone::utc()};
        // about ten days spread over the stretch (every day of a week, every third of 30 days, every ninth of 90), four times a day: the 30 days in 40 fields
        const int every = std::max(1, length / 10);
        std::vector<QDateTime> days;
        for (int d = 0; d * every < length; d++) {
            days.push_back(tomorrow.addDays(static_cast<qint64>(d) * every));
        }
        // the four newest runs' member 1 (whole nine month forecasts six hours apart): their mean is steadier than one run's
        const auto runs = UtilityCfs::recentRuns(run, 4);
        struct Job {
            UtilityCfs::Run run;
            int hour;
        };
        std::vector<Job> wanted;
        for (const auto& past : runs) {
            const auto pastStart = UtilityCfs::runTime(past);
            for (const auto& day : days) {
                for (const int h : {0, 6, 12, 18}) {
                    const auto hour = static_cast<int>((day.addSecs(h * 3600).toSecsSinceEpoch() - pastStart.toSecsSinceEpoch()) / 3600);
                    if (hour >= 6 && hour <= 6552) {
                        wanted.push_back({past, hour});
                    }
                }
            }
        }
        if (wanted.size() < days.size() * 4) {
            error = "the CFSv2 run does not reach " + std::to_string(length) + " days";
            return false;
        }
        std::counting_semaphore<8> gate{6};
        std::vector<std::future<GfsGrid::Grid>> jobs;
        for (const auto& job : wanted) {
            jobs.push_back(std::async(std::launch::async, [&gate, variable, job] {
                gate.acquire();
                GfsGrid::Grid g;
                std::string message;
                if (!UtilityCfs::daily(job.run, 1, *variable, job.hour, g, message)) {
                    g = {};
                }
                gate.release();
                return g;
            }));
        }
        GfsGrid::Grid sum;
        int count = 0;
        for (auto& job : jobs) {
            const auto g = job.get();
            if (g.empty()) {
                continue;
            }
            if (sum.empty()) {
                sum = g;
            } else if (g.values.size() == sum.values.size()) {
                for (size_t i = 0; i < g.values.size(); i++) {
                    sum.values[i] += g.values[i];
                }
            } else {
                continue;
            }
            count++;
        }
        if (count < static_cast<int>(wanted.size()) / 2) {
            error = "only " + std::to_string(count) + " of " + std::to_string(wanted.size()) + " CFSv2 fields could be read";
            return false;
        }
        for (auto& v : sum.values) {
            v /= static_cast<float>(count);
        }
        // the normal of the same days (the monthly normals blended by the day)
        GfsGrid::Grid normal;
        int normals = 0;
        for (const auto& day : days) {
            GfsGrid::Grid n;
            std::string message;
            if (!UtilityCfs::normalAt(*variable, day.addSecs(12 * 3600), &run, baseline, n, message) || n.values.size() != sum.values.size()) {
                continue;
            }
            if (normal.empty()) {
                normal = n;
            } else {
                for (size_t i = 0; i < n.values.size(); i++) {
                    normal.values[i] += n.values[i];
                }
            }
            normals++;
        }
        if (normals == 0) {
            error = "the CFSv2 normal of these days could not be read";
            return false;
        }
        out = sum;
        for (size_t i = 0; i < out.values.size(); i++) {
            out.values[i] = sum.values[i] - normal.values[i] / static_cast<float>(normals);
        }
        return true;
    }

    // the model's forecast hour is the stretch: 24 hours a day
    bool load(const GfsData::Run&, int hour, const GfsData::Want& want, GfsGrid::Grid& out, std::string& error) {
        return loadDays(std::max(hour, 72) / 24, want.variable, out, error);
    }
}

bool Cfs30Loader::period(int days, const std::string& key, GfsGrid::Grid& out, std::string& error) {
    return loadDays(days, key, out, error);
}

void Cfs30Loader::registerLoader() {
    GfsData::setLoader("CFS30", load);
}
