// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "dams/DamData.h"
#include <cmath>
#include <ctime>
#include <future>
#include <map>
#include <mutex>
#include <QByteArray>
#include <QDateTime>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>
#include "rivers/UtilityRivers.h"
#include "util/UtilityIO.h"

const vector<UtilityDams::Project>& DamData::projects() {
    static const auto all = [] {
        // the Corps' own dams (CWMS time series), then the lock and dams and reservoirs of the other services: NWS gauges and USGS stations (office "NWS" / "USGS")
        vector<UtilityDams::Project> list;
        for (const char * resource : {":/res/dams_usace.txt", ":/res/dams_other.txt"}) {
            QFile file{resource};
            file.open(QIODevice::ReadOnly);
            auto part = UtilityDams::parseRegistry(file.readAll().toStdString());
            list.insert(list.end(), part.begin(), part.end());
        }
        return list;
    }();
    return all;
}

namespace {
    // an NWS gauge's observed stage (or its flow, "LID#flow", kcfs shown in cfs) of the last `hours`
    UtilityDams::Series nwsSeries(const string& name, int hours) {
        UtilityDams::Series out;
        const bool flow = name.size() > 5 && name.compare(name.size() - 5, 5, "#flow") == 0;
        const string lid = flow ? name.substr(0, name.size() - 5) : name;
        // the stage and the flow are two series of one answer: kept for a minute, so a dam asks the service once
        static std::mutex cacheMutex;
        static std::map<string, std::pair<std::time_t, QByteArray>> cache;
        QByteArray bytes;
        {
            std::lock_guard lock{cacheMutex};
            const auto found = cache.find(lid);
            if (found != cache.end() && std::time(nullptr) - found->second.first < 60) {
                bytes = found->second.second;
            }
        }
        if (bytes.isEmpty()) {
            bytes = UtilityIO::downloadAsByteArray("https://api.water.noaa.gov/nwps/v1/gauges/" + lid + "/stageflow/observed");
            std::lock_guard lock{cacheMutex};
            cache[lid] = {std::time(nullptr), bytes};
        }
        const auto document = QJsonDocument::fromJson(bytes);
        const auto root = document.object();
        const auto data = root.value("data").toArray();
        const QString units = root.value(flow ? "secondaryUnits" : "primaryUnits").toString();
        out.units = flow ? "cfs" : units.toStdString();
        const auto since = QDateTime::currentSecsSinceEpoch() - static_cast<qint64>(hours) * 3600;
        for (const auto& item : data) {
            const auto o = item.toObject();
            const auto at = QDateTime::fromString(o.value("validTime").toString(), Qt::ISODate).toSecsSinceEpoch();
            double v = o.value(flow ? "secondary" : "primary").toDouble(-999.0);
            if (v <= -990.0 || at < since) {
                continue;
            }
            if (flow && units == "kcfs") {
                v *= 1000.0;
            }
            out.points.push_back({static_cast<long>(at), v});
        }
        return out;
    }

    // a Reclamation RISE item (an elevation, a release ...): its newest daily or hourly results of the last `hours`
    UtilityDams::Series usbrSeries(const string& item, int hours) {
        UtilityDams::Series out;
        const int count = std::max(4, hours / 24 + 4);
        const string after = QDateTime::currentDateTimeUtc().addSecs(-static_cast<qint64>(hours) * 3600 - 86400).toString("yyyy-MM-dd'T'HH:mm:ss").toStdString();
        const string url = "https://data.usbr.gov/rise/api/result?itemId=" + item + "&itemsPerPage=" + std::to_string(std::min(2000, hours * 2 + 8)) +
            "&order%5BdateTime%5D=DESC&dateTime%5Bafter%5D=" + after;
        const auto document = QJsonDocument::fromJson(UtilityIO::downloadAsByteArray(url));
        const auto data = document.object().value("data").toArray();
        const auto since = QDateTime::currentSecsSinceEpoch() - static_cast<qint64>(hours) * 3600 - 86400;
        (void)count;
        for (auto it = data.end(); it != data.begin();) {   // newest first in the answer: oldest first here
            --it;
            const auto a = it->toObject().value("attributes").toObject();
            const auto at = QDateTime::fromString(a.value("dateTime").toString(), Qt::ISODate).toSecsSinceEpoch();
            if (a.value("result").isNull() || at < since) {
                continue;
            }
            out.points.push_back({static_cast<long>(at), a.value("result").toDouble()});
        }
        return out;
    }

    // a USGS instantaneous value series, "site:parameter" (62614 lake elevation, 00060 flow, 00054 reservoir storage)
    UtilityDams::Series usgsSeries(const string& name, int hours) {
        UtilityDams::Series out;
        const auto colon = name.find(':');
        if (colon == string::npos) {
            return out;
        }
        const string url = "https://waterservices.usgs.gov/nwis/iv/?format=json&sites=" + name.substr(0, colon) + "&parameterCd=" + name.substr(colon + 1) +
            "&period=PT" + std::to_string(std::max(1, hours)) + "H";
        const auto document = QJsonDocument::fromJson(UtilityIO::downloadAsByteArray(url));
        const auto series = document.object().value("value").toObject().value("timeSeries").toArray();
        if (series.isEmpty()) {
            return out;
        }
        const auto first = series.first().toObject();
        out.units = first.value("variable").toObject().value("unit").toObject().value("unitCode").toString().toStdString();
        const auto values = first.value("values").toArray().first().toObject().value("value").toArray();
        for (const auto& item : values) {
            const auto o = item.toObject();
            bool ok = false;
            const double v = o.value("value").toString().toDouble(&ok);
            if (!ok || v <= -990.0) {
                continue;
            }
            out.points.push_back({static_cast<long>(QDateTime::fromString(o.value("dateTime").toString(), Qt::ISODate).toSecsSinceEpoch()), v});
        }
        return out;
    }
}

UtilityDams::Series DamData::loadSeries(const UtilityDams::Project& project, const string& name, int hours) {
    if (name.empty()) {
        return {};
    }
    if (project.office == "NWS") {
        return nwsSeries(name, hours);
    }
    if (project.office == "USGS") {
        return usgsSeries(name, hours);
    }
    if (project.office == "USBR") {
        return usbrSeries(name, hours);
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
        data.error = "The data service returned nothing for " + project.name + " just now.";
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
    // the NWS gauges' newest stage is in the river gauge table (one request for all of them, which the gauge layer shares); the rest are a few requests each,
    // a dozen dams at a time
    std::map<string, UtilityRivers::Gauge> gauges;
    {
        vector<UtilityRivers::Gauge> table;
        string ignored;
        UtilityRivers::loadGauges(table, ignored);
        for (auto& g : table) {
            gauges[g.lid] = std::move(g);
        }
    }
    // Reclamation's reservoirs: the newest results of all their items in one request (the service takes a list of item ids)
    std::map<string, std::pair<long, double>> usbr;   // item id -> time, value
    {
        string ids;
        for (const auto& project : list) {
            if (project.office == "USBR") {
                for (const auto& item : {project.pool, project.outflow, project.power}) {
                    if (!item.empty()) {
                        ids += (ids.empty() ? "" : ",") + item;
                    }
                }
            }
        }
        // a bounded time window, in batches of a hundred items: the whole table without one takes the service a minute and times out
        const string after = QDateTime::currentDateTimeUtc().addDays(-4).toString("yyyy-MM-dd'T'00:00:00").toStdString();
        vector<string> batch;
        size_t start = 0;
        while (start <= ids.size() && !ids.empty()) {
            size_t end = start;
            int count = 0;
            while (end < ids.size() && count < 100) {
                const auto comma = ids.find(',', end);
                end = comma == string::npos ? ids.size() : comma + 1;
                count++;
            }
            batch.push_back(ids.substr(start, end - start));
            if (!batch.back().empty() && batch.back().back() == ',') {
                batch.back().pop_back();
            }
            if (end >= ids.size()) {
                break;
            }
            start = end;
        }
        vector<std::future<QByteArray>> answers;
        for (const auto& list100 : batch) {
            answers.push_back(std::async(std::launch::async, [list100, after] {
                return UtilityIO::downloadAsByteArray("https://data.usbr.gov/rise/api/result?itemId=" + list100 + "&itemsPerPage=1000&order%5BdateTime%5D=DESC&dateTime%5Bafter%5D=" + after);
            }));
        }
        for (auto& answer : answers) {
            const auto data = QJsonDocument::fromJson(answer.get()).object().value("data").toArray();
            for (const auto& item : data) {   // newest first: the first of each item is its latest
                const auto a = item.toObject().value("attributes").toObject();
                const string id = std::to_string(a.value("itemId").toInt());
                if (usbr.count(id) > 0 || a.value("result").isNull()) {
                    continue;
                }
                usbr[id] = {static_cast<long>(QDateTime::fromString(a.value("dateTime").toString(), Qt::ISODate).toSecsSinceEpoch()), a.value("result").toDouble()};
            }
        }
    }
    all.assign(list.size(), Latest{});
    vector<size_t> remote;
    for (size_t i = 0; i < list.size(); i++) {
        const auto& project = list[i];
        if (project.office == "USBR") {
            Latest latest;
            latest.project = &project;
            long newest = 0;
            const auto take = [&usbr, &newest] (const string& item, double& into) {
                const auto found = usbr.find(item);
                if (found != usbr.end()) {
                    into = found->second.second;
                    newest = std::max(newest, found->second.first);
                }
            };
            take(project.pool, latest.pool);
            take(project.outflow, latest.outflow);
            take(project.power, latest.power);
            latest.seconds = newest;
            latest.ok = newest > 0 && std::time(nullptr) - newest < 10 * 86400;
            all[i] = latest;
            continue;
        }
        if (project.office != "NWS") {
            remote.push_back(i);
            continue;
        }
        Latest latest;
        latest.project = &project;
        const auto found = gauges.find(project.pool);
        if (found != gauges.end() && !std::isnan(found->second.observed)) {
            latest.pool = found->second.observed;
            auto when = QDateTime::fromString(QString::fromStdString(found->second.obsTime), Qt::ISODate);
            if (!when.isValid()) {
                when = QDateTime::fromString(QString::fromStdString(found->second.obsTime), "yyyy-MM-dd HH:mm:ss");
            }
            when.setTimeSpec(Qt::UTC);
            latest.seconds = when.isValid() ? static_cast<long>(when.toSecsSinceEpoch()) : static_cast<long>(std::time(nullptr));
            latest.ok = found->second.status != "obs_not_current" && found->second.status != "out_of_service";
        }
        all[i] = latest;
    }
    for (size_t first = 0; first < remote.size(); first += 12) {
        vector<std::pair<size_t, std::future<Latest>>> jobs;
        for (size_t k = first; k < std::min(remote.size(), first + 12); k++) {
            const auto& project = list[remote[k]];
            jobs.emplace_back(remote[k], std::async(std::launch::async, [&project] { return loadLatestOne(project, false); }));
        }
        for (auto& job : jobs) {
            all[job.first] = job.second.get();
        }
    }
    std::lock_guard lock{mutex};
    cache = all;
    cachedAt = std::time(nullptr);
}
