// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "tornado/TornadoData.h"
#include <algorithm>
#include <mutex>
#include <QByteArray>
#include <QDateTime>
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
