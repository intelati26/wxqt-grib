// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "history/EventCase.h"
#include <algorithm>
#include <atomic>
#include <thread>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPixmap>
#include <QStandardPaths>
#include <QTimeZone>
#include "objects/NetManager.h"
#include "objects/URL.h"

QString EventCase::folder(const HistoryEvent& e) {
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/archive/cases/" + QString::fromStdString(e.id);
}

QString EventCase::outlookName(const QDate& day, const QString& kind) {
    return "spc_outlook_" + day.toString("yyyyMMdd") + "_1630_" + kind + ".png";
}

QString EventCase::surfaceName(const QString& kind, const QDateTime& utc) {
    return "sfc_" + kind + "_" + utc.toUTC().toString("yyyyMMddHH") + ".gif";
}

std::vector<EventCase::Item> EventCase::items(const HistoryEvent& e) {
    std::vector<Item> out;
    if (!e.start.isValid()) {
        return out;
    }
    const QDateTime start = e.start.toUTC(), end = std::max(e.end.toUTC(), start);
    // the convective days (12Z to 12Z) the event touches, at most four
    const QDate firstDay = start.addSecs(-12 * 3600).date();
    const QDate lastDay = std::min(end.addSecs(-12 * 3600).date(), firstDay.addDays(3));
    static const char * suffixes[] = {"_prt.png", ".gif", "_prt.gif", ".png"};
    for (QDate day = firstDay; day <= lastDay; day = day.addDays(1)) {
        const QString stem = QString{"https://www.spc.noaa.gov/products/outlook/archive/%1/"}.arg(day.toString("yyyy"));
        const QString date = day.toString("yyyyMMdd");
        const std::vector<std::pair<QString, QString>> kinds{{"cat", "day1otlk_" + date + "_1630"}, {"torn", "day1probotlk_" + date + "_1630_torn"}, {"wind", "day1probotlk_" + date + "_1630_wind"},
                                                             {"hail", "day1probotlk_" + date + "_1630_hail"}};
        for (const auto& [kind, file] : kinds) {
            Item item{outlookName(day, kind), {}, "SPC outlook, " + kind + ", " + day.toString("yyyy-MM-dd")};
            for (const char * suffix : suffixes) {
                item.urls << stem + file + suffix;
            }
            out.push_back(std::move(item));
        }
        // the storm reports of the day: the list, and its picture
        const QString yymmdd = day.toString("yyMMdd");
        out.push_back({"storm_reports_" + date + ".csv", {"https://www.spc.noaa.gov/climo/reports/" + yymmdd + "_rpts.csv"}, "SPC storm reports, " + day.toString("yyyy-MM-dd")});
        out.push_back({"storm_reports_" + date + ".gif", {"https://www.spc.noaa.gov/climo/reports/" + yymmdd + "_rpts.gif"}, "SPC storm report map, " + day.toString("yyyy-MM-dd")});
    }
    // the surface analyses every 3 hours (from 2006-03-29 18Z), at most three days of them
    if (end.date() >= QDate{2006, 3, 29}) {
        QDateTime t{start.date(), QTime{start.time().hour() / 3 * 3, 0}, QTimeZone::utc()};
        const QDateTime last = std::min(end, t.addDays(3));
        for (int n = 0; t <= last && n < 25; t = t.addSecs(3 * 3600), n++) {
            if (t < QDateTime{QDate{2006, 3, 29}, QTime{18, 0}, QTimeZone::utc()}) {
                continue;
            }
            out.push_back({surfaceName("namussfc", t), {QString{"https://www.wpc.ncep.noaa.gov/archives/sfc/%1/namussfc%2.gif"}.arg(t.toString("yyyy"), t.toString("yyyyMMddHH"))},
                           "WPC surface analysis " + t.toString("yyyy-MM-dd HH") + "Z"});
        }
    }
    return out;
}

EventCase::Summary EventCase::summary(const HistoryEvent& e) {
    Summary s;
    s.wanted = static_cast<int>(items(e).size());
    for (const auto& info : QDir{folder(e)}.entryInfoList(QDir::Files)) {
        if (info.fileName() != "event.json") {
            s.files++;
            s.bytes += info.size();
        }
    }
    return s;
}

int EventCase::keep(const HistoryEvent& e, const std::function<void(int, int)>& progress) {
    QDir{}.mkpath(folder(e));
    {   // the event itself, so that the folder says what it is
        QJsonObject o{{"id", QString::fromStdString(e.id)}, {"name", QString::fromStdString(e.shownName())}, {"start", e.start.toUTC().toString(Qt::ISODate)}, {"end", e.end.toUTC().toString(Qt::ISODate)},
                      {"lat", e.lat}, {"lon", e.lon}, {"radiusKm", e.radiusKm}, {"note", QString::fromStdString(e.note)}, {"source", QString::fromStdString(e.source)}, {"catalogId", QString::fromStdString(e.catalogId)}};
        QFile file{folder(e) + "/event.json"};
        if (file.open(QIODevice::WriteOnly)) {
            file.write(QJsonDocument{o}.toJson());
        }
    }
    const auto all = items(e);
    std::atomic<size_t> next{0};
    std::atomic<int> done{0}, added{0};
    const int total = static_cast<int>(all.size());
    const QString dir = folder(e);
    const auto work = [&] {
        const NetManager::Scope background{NetManager::Priority::Ahead};
        for (;;) {
            const size_t i = next++;
            if (i >= all.size()) {
                return;
            }
            const auto& item = all[i];
            if (!QFile::exists(dir + "/" + item.name)) {
                for (const auto& url : item.urls) {
                    const auto bytes = URL::getBytes(url.toStdString());
                    bool good = bytes.size() > 200;
                    if (good && (item.name.endsWith(".png") || item.name.endsWith(".gif"))) {
                        QPixmap probe;
                        good = probe.loadFromData(bytes);   // a picture, not an error page
                    } else if (good && item.name.endsWith(".csv")) {
                        good = bytes.startsWith("Time") || bytes.contains(",");
                    }
                    if (good) {
                        QFile file{dir + "/" + item.name};
                        if (file.open(QIODevice::WriteOnly)) {
                            file.write(bytes);
                            added++;
                        }
                        break;
                    }
                }
            }
            const int now = ++done;
            if (progress) {
                progress(now, total);
            }
        }
    };
    std::vector<std::thread> pool;
    for (size_t k = 0; k < std::min<size_t>(6, all.size()); k++) {
        pool.emplace_back(work);
    }
    for (auto& thread : pool) {
        thread.join();
    }
    return added.load();
}

QByteArray EventCase::read(const HistoryEvent& e, const QString& name) {
    QFile file{folder(e) + "/" + name};
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray{};
}

void EventCase::forget(const HistoryEvent& e) {
    QDir{folder(e)}.removeRecursively();
}
