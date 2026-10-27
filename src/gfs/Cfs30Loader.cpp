// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "gfs/Cfs30Loader.h"
#include <algorithm>
#include <cmath>
#include <future>
#include <atomic>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>
#include <QDataStream>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTimeZone>
#include "cfs/UtilityCfs.h"
#include "gfs/GfsCache.h"
#include "gfs/GfsData.h"
#include "util/Utility.h"

namespace {
    // the day's result of a stretch in a small file: kept for the day, the next program start reads it
    QString cacheDir() {
        return GfsCache::folder() + "/cfs30/";
    }
    QString cacheFile(const QDate& day, int days, const std::string& key, int baseline) {
        return cacheDir() + day.toString("yyyyMMdd") + "_" + QString::number(days) + "_" + QString::fromStdString(key) + "_" + QString::number(baseline) + ".bin";
    }
    bool readCache(const QString& path, GfsGrid::Grid& out) {
        QFile file{path};
        if (!file.open(QIODevice::ReadOnly)) {
            return false;
        }
        QByteArray raw = qUncompress(file.readAll());
        QDataStream in{&raw, QIODevice::ReadOnly};
        qint32 columns = 0, rows = 0;
        double lon0 = 0, lat0 = 0, step = 0;
        in >> columns >> rows >> lon0 >> lat0 >> step;
        if (in.status() != QDataStream::Ok || columns <= 0 || rows <= 0 || raw.size() < 28 + static_cast<qsizetype>(columns) * rows * 4) {
            return false;
        }
        out = {};
        out.columns = columns;
        out.rows = rows;
        out.lon0 = lon0;
        out.lat0 = lat0;
        out.step = step;
        out.values.resize(static_cast<size_t>(columns) * static_cast<size_t>(rows));
        in.readRawData(reinterpret_cast<char *>(out.values.data()), static_cast<int>(out.values.size() * sizeof(float)));
        return true;
    }
    void writeCache(const QString& path, const GfsGrid::Grid& grid) {
        QByteArray raw;
        QDataStream out{&raw, QIODevice::WriteOnly};
        out << static_cast<qint32>(grid.columns) << static_cast<qint32>(grid.rows) << grid.lon0 << grid.lat0 << grid.step;
        out.writeRawData(reinterpret_cast<const char *>(grid.values.data()), static_cast<int>(grid.values.size() * sizeof(float)));
        QDir{}.mkpath(cacheDir());
        QFile file{path};
        if (file.open(QIODevice::WriteOnly)) {
            file.write(qCompress(raw, 6));
        }
        for (const auto& old : QDir{cacheDir()}.entryInfoList(QDir::Files)) {   // a week's files are enough
            if (old.lastModified().daysTo(QDateTime::currentDateTime()) > 7) {
                QFile::remove(old.absoluteFilePath());
            }
        }
    }
    int baselineYears() {
        return Utility::readPrefInt("MM_CFS_BASE", 1999) == 1982 ? 1982 : 1999;
    }

    // a few threads for all the reads there are (the program's other work and the machine's disk and processors get the rest: each field is a download and a GDAL run)
    void runAll(std::vector<std::function<void()>>& tasks) {
        std::atomic<size_t> next{0};
        std::vector<std::thread> workers;
        for (int w = 0; w < 4; w++) {
            workers.emplace_back([&] {
                for (size_t i = next++; i < tasks.size(); i = next++) {
                    tasks[i]();
                }
            });
        }
        for (auto& worker : workers) {
            worker.join();
        }
    }

    bool loadDays(int length, const std::string& key, GfsGrid::Grid& out, std::string& error) {
        const auto * variable = UtilityCfs::find(key);
        if (variable == nullptr) {
            error = "the CFS has no field " + key;
            return false;
        }
        length = std::clamp(length, 3, 180);
        const int baseline = baselineYears();
        const auto today = QDateTime::currentDateTimeUtc().date();
        if (readCache(cacheFile(today, length, key, baseline), out)) {
            return true;   // already worked out today
        }
        UtilityCfs::Run run;
        if (!UtilityCfs::latestRun(run)) {
            error = "no CFSv2 run found";
            return false;
        }
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
        std::vector<GfsGrid::Grid> fields(wanted.size());
        std::vector<std::function<void()>> tasks;
        for (size_t i = 0; i < wanted.size(); i++) {
            tasks.push_back([&fields, &wanted, variable, i] {
                std::string message;
                if (!UtilityCfs::daily(wanted[i].run, 1, *variable, wanted[i].hour, fields[i], message)) {
                    fields[i] = {};
                }
            });
        }
        runAll(tasks);
        GfsGrid::Grid sum;
        int count = 0;
        for (const auto& g : fields) {
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
        writeCache(cacheFile(today, length, key, baseline), out);
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

bool Cfs30Loader::cached(int days, const std::string& key, GfsGrid::Grid& out, int& ageDays) {
    const auto today = QDateTime::currentDateTimeUtc().date();
    for (int age = 0; age <= 5; age++) {
        if (readCache(cacheFile(today.addDays(-age), std::clamp(days, 3, 180), key, baselineYears()), out)) {
            ageDays = age;
            return true;
        }
    }
    return false;
}

void Cfs30Loader::registerLoader() {
    GfsData::setLoader("CFS30", load);
}
