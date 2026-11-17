// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "gfs/AirNowLoader.h"
#include <algorithm>
#include <map>
#include <mutex>
#include <QDir>
#include <QFile>
#include <QByteArray>
#include <QDateTime>
#include <QString>
#include <QStringList>
#include "gfs/GfsCache.h"
#include "util/UtilityIO.h"

namespace {
    std::mutex lock;
    qint64 loadedAt = 0;
    std::vector<AirNow::Point> rows;
    std::vector<QDate> rowDates;

    int categoryOfName(const QString& name) {
        const auto n = name.trimmed().toLower();
        if (n == "good") {
            return 0;
        }
        if (n == "moderate") {
            return 1;
        }
        if (n.startsWith("unhealthy for")) {
            return 2;
        }
        if (n == "unhealthy") {
            return 3;
        }
        if (n.startsWith("very")) {
            return 4;
        }
        if (n.startsWith("hazard")) {
            return 5;
        }
        return -1;
    }
}

int AirNow::categoryOf(double aqi) {
    return aqi <= 50.0 ? 0 : aqi <= 100.0 ? 1 : aqi <= 150.0 ? 2 : aqi <= 200.0 ? 3 : aqi <= 300.0 ? 4 : 5;
}

namespace {
    struct Parsed {
        std::vector<AirNow::Point> rows;
        std::vector<QDate> dates;
    };
    std::map<QDate, Parsed> archived;   // the files of past days, by the day of the file

    // the file's forecasts and observations (not yesterday's) as points with their dates
    Parsed parse(const QByteArray& bytes) {
        Parsed out;
        for (const auto& line : bytes.split('\n')) {
            const auto f = QString::fromLatin1(line).split('|');
            if (f.size() < 14 || (f[5] != "F" && f[5] != "O")) {
                continue;
            }
            const auto day = QDate::fromString(f[1], "MM/dd/yy").addYears(100);   // the two digit year reads as 19xx
            bool ok = false;
            const double aqi = f[12].toDouble(&ok);
            const int category = ok && aqi >= 0.0 ? AirNow::categoryOf(aqi) : categoryOfName(f[13]);
            if (!day.isValid() || category < 0) {
                continue;
            }
            AirNow::Point point;
            point.lat = f[9].toDouble();
            point.lon = f[10].toDouble();
            point.category = category;
            point.aqi = ok ? aqi : -1.0;
            out.rows.push_back(point);
            out.dates.push_back(day);
        }
        return out;
    }

    // The file of a past day, kept on disk once read (AirNow keeps its files by day, https://files.airnowtech.org/airnow/<year>/<yyyymmdd>/reportingarea.dat, and the file called "today"
    // is rewritten through the day): a chart of a past run reads the air quality of its own day, the same every time, and offline.
    const Parsed * archive(const QDate& date) {
        if (const auto found = archived.find(date); found != archived.end()) {
            return &found->second;
        }
        const QString path = GfsCache::folder() + "/airnow/" + date.toString("yyyyMMdd") + ".dat.z";
        QByteArray bytes;
        if (QFile file{path}; file.open(QIODevice::ReadOnly)) {
            bytes = qUncompress(file.readAll());
        }
        if (bytes.size() <= 1000) {
            bytes = UtilityIO::downloadAsByteArray("https://files.airnowtech.org/airnow/" + date.toString("yyyy").toStdString() + "/" + date.toString("yyyyMMdd").toStdString() + "/reportingarea.dat");
            if (bytes.size() <= 1000) {
                return nullptr;
            }
            QDir{}.mkpath(GfsCache::folder() + "/airnow");
            if (QFile file{path}; file.open(QIODevice::WriteOnly)) {
                file.write(qCompress(bytes, 9));
            }
        }
        return &archived.emplace(date, parse(bytes)).first->second;
    }
}

std::vector<AirNow::Point> AirNow::points(const QDate& date) {
    std::lock_guard<std::mutex> guard{lock};
    const std::vector<Point> * useRows = &rows;
    const std::vector<QDate> * useDates = &rowDates;
    if (date < QDate::currentDate()) {   // a day that has passed: its own file, kept (the file of today is rewritten through the day)
        if (const auto * past = archive(date)) {
            useRows = &past->rows;
            useDates = &past->dates;
        }
    }
    if (useRows == &rows) {
        const qint64 now = QDateTime::currentSecsSinceEpoch();
        if (rows.empty() || now - loadedAt > 1200) {
            const auto bytes = UtilityIO::downloadAsByteArray("https://files.airnowtech.org/airnow/today/reportingarea.dat");
            if (bytes.size() > 1000) {
                auto parsed = parse(bytes);
                rows = std::move(parsed.rows);
                rowDates = std::move(parsed.dates);
                loadedAt = now;
            }
        }
    }
    // the worst of an area's rows for the date (an area is a place: its latitude and longitude)
    std::map<std::pair<long, long>, Point> worst;
    for (size_t i = 0; i < useRows->size(); i++) {
        if ((*useDates)[i] != date) {
            continue;
        }
        const auto key = std::make_pair(std::lround((*useRows)[i].lat * 1000.0), std::lround((*useRows)[i].lon * 1000.0));
        auto [found, added] = worst.emplace(key, (*useRows)[i]);
        if (!added) {
            found->second.category = std::max(found->second.category, (*useRows)[i].category);
            found->second.aqi = std::max(found->second.aqi, (*useRows)[i].aqi);
        }
    }
    std::vector<Point> out;
    for (const auto& [key, point] : worst) {
        out.push_back(point);
    }
    return out;
}
