// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "gfs/AirNowLoader.h"
#include <algorithm>
#include <map>
#include <mutex>
#include <QByteArray>
#include <QDateTime>
#include <QString>
#include <QStringList>
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

std::vector<AirNow::Point> AirNow::points(const QDate& date) {
    std::lock_guard<std::mutex> guard{lock};
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    if (rows.empty() || now - loadedAt > 1200) {
        const auto bytes = UtilityIO::downloadAsByteArray("https://files.airnowtech.org/airnow/today/reportingarea.dat");
        if (bytes.size() > 1000) {
            rows.clear();
            rowDates.clear();
            loadedAt = now;
            for (const auto& line : bytes.split('\n')) {
                const auto f = QString::fromLatin1(line).split('|');
                if (f.size() < 14 || (f[5] != "F" && f[5] != "O")) {   // the forecasts and the observations of today (not yesterday's)
                    continue;
                }
                const auto day = QDate::fromString(f[1], "MM/dd/yy").addYears(100);   // the two digit year reads as 19xx
                bool ok = false;
                const double aqi = f[12].toDouble(&ok);
                const int category = ok && aqi >= 0.0 ? categoryOf(aqi) : categoryOfName(f[13]);
                if (!day.isValid() || category < 0) {
                    continue;
                }
                Point point;
                point.lat = f[9].toDouble();
                point.lon = f[10].toDouble();
                point.category = category;
                point.aqi = ok ? aqi : -1.0;
                rows.push_back(point);
                rowDates.push_back(day);
            }
        }
    }
    // the worst of an area's rows for the date (an area is a place: its latitude and longitude)
    std::map<std::pair<long, long>, Point> worst;
    for (size_t i = 0; i < rows.size(); i++) {
        if (rowDates[i] != date) {
            continue;
        }
        const auto key = std::make_pair(std::lround(rows[i].lat * 1000.0), std::lround(rows[i].lon * 1000.0));
        auto [found, added] = worst.emplace(key, rows[i]);
        if (!added) {
            found->second.category = std::max(found->second.category, rows[i].category);
            found->second.aqi = std::max(found->second.aqi, rows[i].aqi);
        }
    }
    std::vector<Point> out;
    for (const auto& [key, point] : worst) {
        out.push_back(point);
    }
    return out;
}
