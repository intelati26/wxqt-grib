// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "dams/DamData.h"
#include <ctime>
#include <future>
#include <mutex>
#include <QByteArray>
#include <QDateTime>
#include <QFile>
#include <QUrl>
#include "util/UtilityIO.h"

const vector<UtilityDams::Project>& DamData::projects() {
    static const auto all = [] {
        QFile file{":/res/dams_usace.txt"};
        file.open(QIODevice::ReadOnly);
        return UtilityDams::parseRegistry(file.readAll().toStdString());
    }();
    return all;
}

UtilityDams::Series DamData::loadSeries(const UtilityDams::Project& project, const string& name, int hours) {
    if (name.empty()) {
        return {};
    }
    const auto end = QDateTime::currentDateTimeUtc();
    const auto begin = end.addSecs(-static_cast<qint64>(hours) * 3600);
    const QString url = "https://cwms-data.usace.army.mil/cwms-data/timeseries?office=" + QString::fromStdString(project.office) + "&name=" +
        QString::fromUtf8(QUrl::toPercentEncoding(QString::fromStdString(name))) + "&begin=" + begin.toString("yyyy-MM-ddTHH:mm:ssZ") + "&end=" + end.toString("yyyy-MM-ddTHH:mm:ssZ") +
        "&unit=EN&format=json&page-size=2000";
    return UtilityDams::parseTimeSeries(UtilityIO::downloadAsByteArray(url.toStdString()).toStdString());
}

void DamData::loadProject(const UtilityDams::Project& project, int hours, Data& data) {
    data = Data{};
    data.pool = loadSeries(project, project.pool, hours);
    data.tailwater = loadSeries(project, project.tailwater, hours);
    data.outflow = loadSeries(project, project.outflow, hours);
    data.power = loadSeries(project, project.power, hours);
    data.inflow = loadSeries(project, project.inflow, hours);
    vector<UtilityDams::Series> units;
    for (const auto& name : project.generation) {
        units.push_back(loadSeries(project, name, hours));
    }
    data.generation = UtilityDams::sum(units);
    if (data.pool.points.empty() && data.outflow.points.empty() && data.generation.points.empty()) {
        data.error = "The Corps' data service returned nothing for " + project.name + " just now.";
    }
}

DamData::Latest DamData::loadLatestOne(const UtilityDams::Project& project, bool full) {
    Latest latest;
    latest.project = &project;
    const auto last = [] (const UtilityDams::Series& s, double& into, long& newest) {
        if (!s.points.empty()) {
            into = s.points.back().value;
            newest = std::max(newest, s.points.back().seconds);
        }
    };
    long newest = 0;
    // the last 8 hours: the newest hourly value of each quantity
    last(loadSeries(project, project.pool, 8), latest.pool, newest);
    last(loadSeries(project, project.outflow, 8), latest.outflow, newest);
    if (full) {   // the map needs only the pool, the release and the power; the dam's own page the rest
        last(loadSeries(project, project.tailwater, 8), latest.tailwater, newest);
        last(loadSeries(project, project.power, 8), latest.power, newest);
        last(loadSeries(project, project.inflow, 8), latest.inflow, newest);
    }
    vector<UtilityDams::Series> units;
    for (const auto& name : project.generation) {
        units.push_back(loadSeries(project, name, 8));
    }
    last(UtilityDams::sum(units), latest.generation, newest);
    latest.seconds = newest;
    latest.ok = newest > 0;
    return latest;
}

void DamData::loadLatest(vector<Latest>& all) {
    static std::mutex mutex;
    static vector<Latest> cache;
    static std::time_t cachedAt = 0;
    {
        std::lock_guard lock{mutex};
        if (!cache.empty() && std::time(nullptr) - cachedAt < 600) {
            all = cache;
            return;
        }
    }
    // each dam is several small requests: one thread each
    const auto& list = projects();
    vector<std::future<Latest>> jobs;
    for (const auto& project : list) {
        jobs.push_back(std::async(std::launch::async, [&project] { return loadLatestOne(project, false); }));
    }
    all.clear();
    for (auto& job : jobs) {
        all.push_back(job.get());
    }
    std::lock_guard lock{mutex};
    cache = all;
    cachedAt = std::time(nullptr);
}
