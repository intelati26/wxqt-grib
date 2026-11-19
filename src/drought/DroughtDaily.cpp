// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "drought/DroughtDaily.h"
#include <algorithm>
#include <cmath>
#include <future>
#include <map>
#include <mutex>
#include <sstream>
#include <QString>
#include <functional>
#include "objects/URL.h"
#include "util/PermanentCache.h"

namespace {
    const std::string base{"https://www.ncei.noaa.gov/data/nclimgrid-daily/access/averages/"};
    constexpr int normalFrom = 1991, normalTo = 2020;
    constexpr double missing = -990.0;

    std::string element(bool maximum) { return maximum ? "tmax" : "tmin"; }

    // one month's file of a kind: every area's row, the values of the days. `finalOnly`: the scaled (final) file, or the preliminary one of the current month. Empty when there is none.
    std::string monthFile(const std::string& kind, bool maximum, int year, int month, bool keep = true) {   // keep: the final file is saved (the normals' 1991-2020 files are not: they are read once, for the month's normal)
        const std::string stem = element(maximum) + "-" + QString("%1%2").arg(year).arg(month, 2, 10, QChar{'0'}).toStdString() + "-" + kind + "-";
        const PermanentCache cache{"drought"};
        const std::string name = "daily/" + stem + "scaled.csv";
        if (cache.has(name)) {   // (one saved before is used)
            return cache.read(name);
        }
        const std::string folder = base + std::to_string(year) + "/";
        auto text = URL::getBytes(folder + stem + "scaled.csv").toStdString();
        if (text.size() > 200 && text.compare(0, kind.size(), kind) == 0) {
            if (keep) {
                cache.write(name, text);   // final: it does not change
            }
            return text;
        }
        text = URL::getBytes(folder + stem + "prelim.csv").toStdString();
        return text.size() > 200 && text.compare(0, kind.size(), kind) == 0 ? text : std::string{};
    }

    struct Row {
        std::string code, name;
        std::vector<double> days;   // NaN where there is none
    };
    std::vector<Row> parseMonth(const std::string& text) {
        std::vector<Row> rows;
        std::istringstream in{text};
        std::string line;
        while (std::getline(in, line)) {
            const auto fields = QString::fromStdString(line).split(',');
            if (fields.size() < 8) {
                continue;
            }
            Row row;
            row.code = fields[1].trimmed().toStdString();
            row.name = fields[2].trimmed().toStdString();
            for (int i = 6; i < fields.size(); i++) {
                bool ok = false;
                const double v = fields[i].trimmed().toDouble(&ok);
                row.days.push_back(ok && v > missing ? v : std::nan(""));
            }
            rows.push_back(row);
        }
        return rows;
    }

    bool same(const DroughtDaily::Key& key, const Row& row) {
        return key.kind == "ste" ? row.name == key.name : row.code == key.code;
    }

    // the day of the year in a leap year (so that the 29th of February has a day of its own)
    int dayIndex(int month, int day) {
        return QDate{2000, month, day}.dayOfYear() - 1;
    }

    // The normals of every area of a kind for ONE month: row key -> the 31 days' values (the mean of that day over 1991-2020). Made from that month's 30 files (one a year) the first time the month is
    // asked for, and kept for good (a normal of 1991-2020 never changes), so that only the months a chart needs are downloaded: a chart of the last weeks reads one or two months' files, not all 360.
    // A full year's file made by an earlier version, when there is one, answers without downloading.
    using MonthNormals = std::map<std::string, std::vector<double>>;
    MonthNormals normalsOfMonth(const std::string& kind, bool maximum, int month, std::string& error) {
        static std::mutex mutex;
        static std::map<std::string, MonthNormals> kept;
        const std::lock_guard lock{mutex};
        const std::string id = kind + element(maximum) + std::to_string(month);
        if (const auto it = kept.find(id); it != kept.end()) {
            return it->second;
        }
        const PermanentCache cache{"drought"};
        const std::string span = std::to_string(normalFrom) + "-" + std::to_string(normalTo);
        MonthNormals out;
        const auto parse = [] (const std::string& text, size_t width, const std::function<void(const std::string&, std::vector<double>)>& add) {
            std::istringstream in{text};
            std::string line;
            while (std::getline(in, line)) {
                const auto fields = QString::fromStdString(line).split('|');
                if (static_cast<size_t>(fields.size()) != width + 2) {   // the area's name, its code (the row's key is "name|code"), then the values
                    continue;
                }
                std::vector<double> values;
                for (int i = 2; i < fields.size(); i++) {
                    values.push_back(fields[i].isEmpty() ? std::nan("") : fields[i].toDouble());
                }
                add((fields[0] + "|" + fields[1]).toStdString(), std::move(values));
            }
        };
        const std::string monthName = "daily/normals-" + kind + "-" + element(maximum) + "-m" + QString("%1").arg(month, 2, 10, QChar{'0'}).toStdString() + "-" + span + ".csv";
        parse(cache.read(monthName), 31, [&out] (const std::string& key, std::vector<double> values) { out[key] = std::move(values); });
        if (out.empty()) {
            const std::string fullName = "daily/normals-" + kind + "-" + element(maximum) + "-" + span + ".csv";
            if (cache.has(fullName)) {   // (all 366 days, from the version that read every month)
                parse(cache.read(fullName), 366, [&out, month] (const std::string& key, std::vector<double> values) {
                    std::vector<double> days(31, std::nan(""));
                    for (int d = 1; d <= 31; d++) {
                        if (QDate::isValid(2000, month, d)) {
                            days[static_cast<size_t>(d - 1)] = values[static_cast<size_t>(dayIndex(month, d))];
                        }
                    }
                    out[key] = std::move(days);
                });
            }
        }
        if (out.empty()) {   // this month's 30 files, a few at a time
            std::map<std::string, std::vector<double>> sum, count;
            std::vector<std::future<std::string>> years;
            for (int year = normalFrom; year <= normalTo; year++) {
                years.push_back(std::async(std::launch::async, [kind, maximum, year, month] { return monthFile(kind, maximum, year, month, false); }));
            }
            for (auto& file : years) {
                for (const auto& row : parseMonth(file.get())) {
                    const std::string key = row.name + "|" + row.code;
                    auto& s = sum[key];
                    auto& c = count[key];
                    s.resize(31, 0.0);
                    c.resize(31, 0.0);
                    for (size_t d = 0; d < row.days.size() && d < 31; d++) {
                        if (!std::isnan(row.days[d]) && QDate::isValid(2000, month, static_cast<int>(d) + 1)) {
                            s[d] += row.days[d];
                            c[d] += 1.0;
                        }
                    }
                }
            }
            std::string text;
            for (const auto& [key, s] : sum) {
                std::vector<double> values(31, std::nan(""));
                std::string line = key;
                for (size_t i = 0; i < 31; i++) {
                    if (count[key][i] > 0) {
                        values[i] = s[i] / count[key][i];
                        line += "|" + QString::number(values[i], 'f', 3).toStdString();
                    } else {
                        line += "|";
                    }
                }
                out[key] = values;
                text += line + "\n";
            }
            if (out.empty()) {
                error = "the daily temperature files of 1991-2020 could not be had";
                return out;
            }
            cache.write(monthName, text);
        }
        kept[id] = out;
        return out;
    }
}

bool DroughtDaily::keyFor(const std::string& areaId, const std::string& areaName, const std::string& stateName, Key& out) {
    if (areaId == "US") {
        out = {"cns", "110", "Contiguous 48 States"};
        return true;
    }
    const bool digits = !areaId.empty() && std::all_of(areaId.begin(), areaId.end(), [] (unsigned char c) { return std::isdigit(c); });
    if (digits && areaId.size() == 2) {
        out = {"ste", "", areaName};
        return true;
    }
    if (digits && areaId.size() == 5) {   // a county: its state's
        out = {"ste", "", stateName};
        return !stateName.empty();
    }
    if (areaId.size() == 3 && std::all_of(areaId.begin(), areaId.end(), [] (unsigned char c) { return std::isupper(c); })) {
        out = {"wfo", areaId, areaName};
        return true;
    }
    return false;
}

bool DroughtDaily::departures(const Key& key, bool maximum, const QDate& from, const QDate& to, int smooth, std::vector<Point>& out, std::string& error) {
    out.clear();
    smooth = std::max(1, smooth);
    // every day from the first the mean needs to the last
    const QDate first = from.addDays(-(smooth - 1));
    std::vector<QDate> dates;
    std::vector<double> departure;
    std::map<std::pair<int, int>, std::vector<Row>> months;
    std::map<int, std::vector<double>> normalOfMonth;   // the area's normal days of each month the chart reaches: only those months' files are read
    for (QDate day = first; day <= to; day = day.addDays(1)) {
        if (normalOfMonth.find(day.month()) == normalOfMonth.end()) {
            const auto all = normalsOfMonth(key.kind, maximum, day.month(), error);
            if (all.empty()) {
                return false;
            }
            std::vector<double> mine;
            for (const auto& [id, values] : all) {
                const auto bar = id.find('|');
                const std::string name = id.substr(0, bar), code = id.substr(bar + 1);
                if ((key.kind == "ste" && name == key.name) || (key.kind != "ste" && code == key.code)) {
                    mine = values;
                    break;
                }
            }
            if (mine.size() != 31) {
                error = "there is no daily record for this area";
                return false;
            }
            normalOfMonth[day.month()] = mine;
        }
        const auto monthKey = std::make_pair(day.year(), day.month());
        if (months.find(monthKey) == months.end()) {
            months[monthKey] = parseMonth(monthFile(key.kind, maximum, day.year(), day.month()));
        }
        double value = std::nan("");
        for (const auto& row : months[monthKey]) {
            if (same(key, row) && static_cast<size_t>(day.day() - 1) < row.days.size()) {
                value = row.days[static_cast<size_t>(day.day() - 1)];
                break;
            }
        }
        const double n = normalOfMonth[day.month()][static_cast<size_t>(day.day() - 1)];
        dates.push_back(day);
        departure.push_back(std::isnan(value) || std::isnan(n) ? std::nan("") : value - n);
    }
    for (size_t i = static_cast<size_t>(smooth - 1); i < dates.size(); i++) {
        if (dates[i] < from) {
            continue;
        }
        double total = 0.0;
        int n = 0;
        for (int k = 0; k < smooth; k++) {
            const double v = departure[i - static_cast<size_t>(k)];
            if (!std::isnan(v)) {
                total += v;
                n++;
            }
        }
        if (n >= std::max(1, smooth * 2 / 3)) {   // a mean of too few days is not shown (the current month's last days are not in yet)
            out.push_back({dates[i], total / n});
        }
    }
    if (out.empty()) {
        error = "no daily values for this period yet";
        return false;
    }
    return true;
}
