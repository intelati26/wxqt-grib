// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "tornado/TornadoData.h"
#include <algorithm>
#include <cstdio>
#include <future>
#include <mutex>
#include <QByteArray>
#include <QDateTime>
#include <QDate>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include "util/UtilityIO.h"

long TornadoData::ordinal(int year, int month, int day) {
    const long long yy = month <= 2 ? year - 1 : year;
    const long long era = (yy >= 0 ? yy : yy - 399) / 400;
    const long long yoe = yy - era * 400;
    const long long doy = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
    const long long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return static_cast<long>(era * 146097 + doe - 719468);
}

std::shared_ptr<const TornadoData::Database> TornadoData::load() {
    static std::mutex mutex;
    static std::shared_ptr<const Database> cached;
    {
        std::lock_guard lock{mutex};
        if (cached) {
            return cached;
        }
    }
    auto db = std::make_shared<Database>();
    const std::string base = "https://www.spc.noaa.gov/wcm/";
    // the newest file on the page; if the page cannot be read, the newest one on disk
    std::string name = UtilityTornado::newestFile(UtilityIO::downloadAsByteArray(base).toStdString());
    const auto folder = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/tornado";
    QDir{}.mkpath(folder);
    if (name.empty()) {
        for (const auto& old : QDir{folder}.entryList({"1950-*_actual_tornadoes.csv"}, QDir::Files, QDir::Name | QDir::Reversed)) {
            name = old.toStdString();
            break;
        }
    }
    if (name.empty()) {
        db->error = "Could not find the SPC tornado database.";
        return db;
    }
    const auto path = folder + "/" + QString::fromStdString(name);
    std::string text;
    QFile file{path};
    const bool fresh = QFileInfo{path}.exists() && QFileInfo{path}.lastModified().secsTo(QDateTime::currentDateTime()) < 7 * 86400;
    if (fresh && file.open(QIODevice::ReadOnly)) {
        text = file.readAll().toStdString();
        file.close();
    }
    if (text.empty()) {
        text = UtilityIO::downloadAsByteArray(base + "data/" + name).toStdString();
        if (text.size() > 100000 && file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            file.write(QByteArray::fromStdString(text));
            file.close();
            for (const auto& old : QDir{folder}.entryList({"1950-*_actual_tornadoes.csv"}, QDir::Files)) {
                if (old != QString::fromStdString(name)) {
                    QFile::remove(folder + "/" + old);
                }
            }
        } else if (text.size() <= 100000 && file.open(QIODevice::ReadOnly)) {   // the download failed: an old copy is better than none
            text = file.readAll().toStdString();
            file.close();
        }
    }
    db->tornadoes = UtilityTornado::parse(text);
    db->file = name;
    if (!db->tornadoes.empty()) {
        // the official file stops at the end of a year: the years since come from the daily reports (the day files are kept; the last few days are read again)
        int officialLast = 0;
        for (const auto& t : db->tornadoes) {
            officialLast = std::max(officialLast, t.year);
        }
        const auto now = QDateTime::currentDateTimeUtc().date();
        if (now.year() > officialLast) {
            db->preliminaryFrom = officialLast + 1;
            struct Day { int y, m, d; };
            std::vector<Day> days;
            for (int y = officialLast + 1; y <= now.year(); y++) {
                for (int m = 1; m <= 12; m++) {
                    for (int d = 1; d <= QDate{y, m, 1}.daysInMonth(); d++) {
                        if (QDate{y, m, d} <= now) {
                            days.push_back({y, m, d});
                        }
                    }
                }
            }
            QDir{}.mkpath(folder + "/daily");
            for (size_t start = 0; start < days.size(); start += 8) {
                std::vector<std::future<std::pair<Day, std::string>>> jobs;
                for (size_t i = start; i < std::min(days.size(), start + 8); i++) {
                    jobs.push_back(std::async(std::launch::async, [day = days[i], folder, now] {
                        char stamp[16];
                        std::snprintf(stamp, sizeof stamp, "%02d%02d%02d", day.y % 100, day.m, day.d);
                        const QString path = folder + "/daily/" + stamp + ".csv";
                        const bool recent = QDate{day.y, day.m, day.d}.daysTo(now) <= 5;
                        QFile cache{path};
                        if (!recent && cache.open(QIODevice::ReadOnly)) {
                            const auto bytes = cache.readAll();
                            return std::make_pair(day, bytes.toStdString());
                        }
                        const auto bytes = UtilityIO::downloadAsByteArray(std::string{"https://www.spc.noaa.gov/climo/reports/"} + stamp + "_rpts_torn.csv").toStdString();
                        if (bytes.rfind("Time", 0) == 0 && cache.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                            cache.write(QByteArray::fromStdString(bytes));
                        }
                        return std::make_pair(day, bytes);
                    }));
                }
                for (auto& job : jobs) {
                    auto [day, bytes] = job.get();
                    auto reports = UtilityTornado::parseDailyReport(bytes, day.y, day.m, day.d);
                    db->preliminaryCount += static_cast<int>(reports.size());
                    for (auto& r : reports) {
                        db->tornadoes.push_back(std::move(r));
                    }
                }
            }
        }
    }
    if (db->tornadoes.empty()) {
        db->error = "Could not read the SPC tornado database " + name + ".";
        return db;
    }
    long newest = 0;
    db->firstYear = 9999;
    for (const auto& t : db->tornadoes) {
        db->firstYear = std::min(db->firstYear, t.year);
        db->lastYear = std::max(db->lastYear, t.year);
        const long o = ordinal(t.year, t.month, t.day);
        if (o > newest) {
            newest = o;
            db->lastMonth = t.month;
            db->lastDay = t.day;
        }
    }
    std::lock_guard lock{mutex};
    cached = db;
    return cached;
}
