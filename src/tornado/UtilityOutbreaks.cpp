// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "tornado/UtilityOutbreaks.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <QTimeZone>

namespace {
    const qint64 linkSeconds = 10 * 3600;        // a tornado joins a group whose recent tornado is not older than this ...
    const double linkKm = 450.0;                  // ... and no farther than this
    const qint64 longestSeconds = 7 * 86400;      // a chain longer than a week is cut

    struct Item {
        const UtilityTornado::Tornado * tornado;
        qint64 time;       // seconds since 1970
    };

    struct Group {
        std::vector<size_t> members;   // indices into the sorted items
        qint64 first{0}, last{0};
    };
}

QDateTime UtilityOutbreaks::utcOf(const UtilityTornado::Tornado& t) {
    QDateTime when{QDate{t.year, t.month, t.day}, QTime::fromString(QString::fromStdString(t.time), "HH:mm:ss"), QTimeZone::utc()};
    if (!when.isValid()) {
        when = QDateTime{QDate{t.year, t.month, t.day}, QTime{12, 0}, QTimeZone::utc()};
    }
    return t.timeZone == 9 ? when : when.addSecs(6 * 3600);   // CST is six hours behind UTC
}

std::vector<UtilityOutbreaks::Outbreak> UtilityOutbreaks::find(const std::vector<UtilityTornado::Tornado>& rows, int minimumCount) {
    std::vector<Item> items;
    items.reserve(rows.size());
    for (const auto& t : rows) {
        if (!t.counts() || (t.startLat == 0.0 && t.startLon == 0.0)) {
            continue;
        }
        items.push_back({&t, utcOf(t).toSecsSinceEpoch()});
    }
    std::sort(items.begin(), items.end(), [] (const Item& a, const Item& b) { return a.time < b.time; });
    std::vector<Group> groups;
    std::vector<size_t> active;   // groups that were joined within the link time
    for (size_t i = 0; i < items.size(); i++) {
        const auto& item = items[i];
        active.erase(std::remove_if(active.begin(), active.end(), [&] (size_t g) { return item.time - groups[g].last > linkSeconds; }), active.end());
        std::vector<size_t> near;   // the active groups with a recent member in reach
        for (const size_t g : active) {
            if (item.time - groups[g].first > longestSeconds) {
                continue;
            }
            for (auto it = groups[g].members.rbegin(); it != groups[g].members.rend(); ++it) {
                const auto& other = items[*it];
                if (item.time - other.time > linkSeconds) {
                    break;
                }
                if (UtilityTornado::kilometers(item.tornado->startLat, item.tornado->startLon, other.tornado->startLat, other.tornado->startLon) <= linkKm) {
                    near.push_back(g);
                    break;
                }
            }
        }
        if (near.empty()) {
            groups.push_back({{i}, item.time, item.time});
            active.push_back(groups.size() - 1);
            continue;
        }
        const size_t home = near.front();   // the oldest group of those in reach takes it, and takes the others in (they are one outbreak)
        for (size_t k = 1; k < near.size(); k++) {
            auto& other = groups[near[k]];
            groups[home].members.insert(groups[home].members.end(), other.members.begin(), other.members.end());
            groups[home].first = std::min(groups[home].first, other.first);
            other.members.clear();
            active.erase(std::remove(active.begin(), active.end(), near[k]), active.end());
        }
        groups[home].members.push_back(i);
        groups[home].last = item.time;
        std::sort(groups[home].members.begin(), groups[home].members.end());
    }
    std::vector<Outbreak> out;
    for (const auto& g : groups) {
        if (static_cast<int>(g.members.size()) < minimumCount) {
            continue;
        }
        Outbreak o;
        o.count = static_cast<int>(g.members.size());
        o.start = QDateTime::fromSecsSinceEpoch(items[g.members.front()].time, QTimeZone::utc());
        o.end = QDateTime::fromSecsSinceEpoch(items[g.members.back()].time, QTimeZone::utc());
        std::map<std::string, int> byState;
        std::map<qint64, int> byHour;
        double latSum = 0.0, lonSum = 0.0;
        for (const size_t index : g.members) {
            const auto& t = *items[index].tornado;
            latSum += t.startLat;
            lonSum += t.startLon;
            o.fatalities += t.fatalities;
            o.injuries += t.injuries;
            o.lengthMiles += t.length;
            byState[t.state]++;
            byHour[items[index].time / 3600]++;
            if (t.mag >= 2) {
                o.significant++;
            }
            if (t.mag > o.strongest || (t.mag == o.strongest && t.fatalities > 0 && !o.strongestTime.isValid())) {
                o.strongest = t.mag;
                o.strongestTime = QDateTime::fromSecsSinceEpoch(items[index].time, QTimeZone::utc());
                o.strongestState = t.state;
            }
        }
        o.centerLat = latSum / o.count;
        o.centerLon = lonSum / o.count;
        for (const size_t index : g.members) {
            const auto& t = *items[index].tornado;
            o.radiusKm = std::max(o.radiusKm, UtilityTornado::kilometers(o.centerLat, o.centerLon, t.startLat, t.startLon));
        }
        std::vector<std::pair<int, std::string>> ranked;
        for (const auto& [state, n] : byState) {
            ranked.push_back({n, state});
        }
        std::sort(ranked.begin(), ranked.end(), [] (const auto& a, const auto& b) { return a.first != b.first ? a.first > b.first : a.second < b.second; });
        for (const auto& r : ranked) {
            o.states.push_back(r.second);
        }
        for (const auto& [hour, n] : byHour) {
            if (n > o.busiestCount) {
                o.busiestCount = n;
                o.busiestHour = QDateTime::fromSecsSinceEpoch(hour * 3600, QTimeZone::utc());
            }
        }
        out.push_back(std::move(o));
    }
    return out;
}

std::string UtilityOutbreaks::Outbreak::title() const {
    const bool sameDay = start.date() == end.date();
    QString dates = start.toString("MMM d");
    if (!sameDay) {
        dates += start.date().month() == end.date().month() ? "-" + end.toString("d") : " - " + end.toString("MMM d");
    }
    dates += " " + start.toString("yyyy");
    QString text = dates + ": " + QString::number(count) + " tornadoes";
    if (strongest >= 0) {
        text += ", " + QString::fromStdString(UtilityTornado::ratingOf(strongest, start.date().year()));
    }
    return text.toStdString();
}

std::string UtilityOutbreaks::Outbreak::summary() const {
    QString states_;
    for (size_t i = 0; i < states.size() && i < 8; i++) {
        states_ += (i > 0 ? " " : "") + QString::fromStdString(states[i]);
    }
    QString text = QString{"%1 tornadoes in %2 (%3 rated EF2 or stronger), %4 deaths, %5 injuries, tracks %6 miles in all. SPC tornado database; the group joins tornadoes that start within 10 hours and 450 km of each other."}
        .arg(count).arg(states_).arg(significant).arg(fatalities).arg(injuries).arg(lengthMiles, 0, 'f', 0);
    return text.toStdString();
}
