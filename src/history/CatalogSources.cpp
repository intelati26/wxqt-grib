// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "history/CatalogSources.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <mutex>
#include <QFile>
#include <QTimeZone>
#include "climate/UtilityAcisClimate.h"
#include "hurricane/HurricaneData.h"
#include "hurricane/UtilityAtcf.h"
#include "tornado/TornadoData.h"
#include "tornado/UtilityOutbreaks.h"

namespace {
    using Entry = CatalogDialog::Entry;

    QDateTime hurdatTime(const std::string& yyyymmddhh) {
        auto t = QDateTime::fromString(QString::fromStdString(yyyymmddhh), "yyyyMMddHH");
        t.setTimeZone(QTimeZone::utc());
        return t;
    }

    QString storm(int wind) {
        const int c = UtilityAtcf::categoryOf(wind);
        return c <= 0 ? QString{"tropical depression"} : c == 1 ? QString{"tropical storm"} : QString{"hurricane, category %1"}.arg(c - 1);
    }
}

CatalogDialog::Source CatalogSources::tornadoOutbreaks() {
    CatalogDialog::Source source;
    source.name = "Tornado outbreaks since 1950 (SPC database)";
    source.columns = {"Dates (UTC)", "Tornadoes", "Strongest", "Deaths", "Injuries", "States"};
    source.orders = {"Deadliest", "Most tornadoes", "Strongest", "Newest", "Oldest"};
    source.sizeLabel = "tornadoes";
    source.sizeDefault = 15;
    source.note = "Tornadoes are joined when they start within 10 hours and 450 km of each other.";
    source.load = [] (const CatalogDialog::Params&, std::string& error) {
        std::vector<Entry> out;
        const auto database = TornadoData::load();
        if (!database || database->tornadoes.empty()) {
            error = database ? database->error : "the tornado database could not be read";
            return out;
        }
        for (const auto& o : UtilityOutbreaks::find(database->tornadoes, 5)) {
            Entry e;
            QString states;
            for (size_t k = 0; k < o.states.size() && k < 8; k++) {
                states += (k > 0 ? " " : "") + QString::fromStdString(o.states[k]);
            }
            e.cells = {o.start.toString("yyyy-MM-dd HH:mm") + (o.end.date() == o.start.date() ? " - " + o.end.toString("HH:mm") : " to " + o.end.toString("MM-dd HH:mm")), QString::number(o.count),
                       o.strongest >= 0 ? QString::fromStdString(UtilityTornado::ratingOf(o.strongest, o.start.date().year())) : QString{"-"}, QString::number(o.fatalities), QString::number(o.injuries), states};
            e.keys = {o.fatalities * 1000.0 + o.count, static_cast<double>(o.count), o.strongest * 100000.0 + o.fatalities, static_cast<double>(o.start.toSecsSinceEpoch()), -static_cast<double>(o.start.toSecsSinceEpoch())};
            e.size = o.count;
            e.year = o.start.date().year();
            e.event.source = "tornado-outbreak";
            e.event.catalogId = o.id();
            e.event.name = o.title();
            e.event.start = o.start.addSecs(-2 * 3600);
            e.event.end = o.end.addSecs(2 * 3600);
            e.event.lat = o.centerLat;
            e.event.lon = o.centerLon;
            e.event.radiusKm = std::min(1500.0, o.radiusKm + 60.0);
            e.event.note = o.summary();
            e.event.moments.push_back({o.start, "first tornado"});
            if (o.strongest >= 0) {
                e.event.moments.push_back({o.strongestTime, "strongest: " + UtilityTornado::ratingOf(o.strongest, o.start.date().year()) + " (" + o.strongestState + ")"});
            }
            if (o.busiestCount > 1) {
                e.event.moments.push_back({o.busiestHour, "most touchdowns in an hour (" + std::to_string(o.busiestCount) + ")"});
            }
            std::sort(e.event.moments.begin(), e.event.moments.end(), [] (const auto& a, const auto& b) { return a.time < b.time; });
            out.push_back(std::move(e));
        }
        return out;
    };
    return source;
}

// the biggest days at a place: the station nearest the page's point, its daily record from ACIS; the days are ranked by the measure chosen
CatalogDialog::Source CatalogSources::biggestDays() {
    CatalogDialog::Source source;
    source.name = "Biggest days at the page's place (ACIS station record)";
    source.columns = {"Dates (local days)", "Amount", "Station", "Year"};
    source.orders = {"Biggest", "Newest", "Oldest"};
    source.whats = {"Snowfall, 1 day (in)", "Snowfall, 3-day total (in)", "Rain or melted snow, 1 day (in)", "Rain or melted snow, 3-day total (in)", "Hottest days (F)", "Coldest nights (F)", "Coldest days, lowest high (F)"};
    source.place = true;
    source.note = "Taken from the long-record station nearest the point on the page (a city's ThreadEx record where there is one). Days are local days.";
    source.load = [] (const CatalogDialog::Params& params, std::string& error) {
        std::vector<Entry> out;
        const auto record = AcisClimate::readStationRecord(params.lat, params.lon);
        if (!record.error.empty() || record.days.empty()) {
            error = record.error.empty() ? "no record" : record.error;
            return out;
        }
        const int what = params.what;
        const bool snow = what == 0 || what == 1, rain = what == 2 || what == 3;
        const int span = what == 1 || what == 3 ? 3 : 1;
        const bool temperature = what >= 4;
        const auto value = [&] (const AcisClimate::Day& d) { return snow ? d.snow : rain ? d.rain : what == 4 ? d.maxt : what == 5 ? -d.mint : -d.maxt; };   // the colder ones as the negative of the temperature: the biggest first
        // each window of `span` days with every day known and a total worth listing; the biggest first, none overlapping a bigger one
        struct Window {
            size_t first;
            double total;
        };
        std::vector<Window> windows;
        for (size_t i = 0; i + span <= record.days.size(); i++) {
            double total = 0.0;
            bool ok = true;
            for (int k = 0; k < span && ok; k++) {
                const float v = value(record.days[i + static_cast<size_t>(k)]);
                if (std::isnan(v) || (k > 0 && record.days[i + static_cast<size_t>(k)].date != record.days[i].date.addDays(k))) {
                    ok = false;
                } else {
                    total += v;
                }
            }
            if (ok && (temperature || total > 0.05)) {
                windows.push_back({i, total});
            }
        }
        std::sort(windows.begin(), windows.end(), [] (const Window& a, const Window& b) { return a.total > b.total; });
        std::vector<char> taken(record.days.size(), 0);
        int kept = 0;
        const int shift = static_cast<int>(std::lround(-record.lon / 15.0));   // hours from local midnight to UTC
        for (const auto& w : windows) {
            bool free = true;
            for (int k = -(temperature ? 2 : 0); k < span + (temperature ? 2 : 0) && free; k++) {
                const long at = static_cast<long>(w.first) + k;
                free = at < 0 || at >= static_cast<long>(taken.size()) || !taken[static_cast<size_t>(at)];
            }
            if (!free) {
                continue;
            }
            for (int k = 0; k < span; k++) {
                taken[w.first + static_cast<size_t>(k)] = 1;
            }
            const auto& first = record.days[w.first];
            const auto& last = record.days[w.first + static_cast<size_t>(span) - 1];
            const double amount = temperature ? (what == 4 ? w.total : -w.total) : w.total;
            const QString amountText = temperature ? QString::number(std::lround(amount)) + " F" : QString::number(amount, 'f', 2) + " in";
            Entry e;
            e.cells = {first.date.toString("yyyy-MM-dd") + (span > 1 ? " to " + last.date.toString("MM-dd") : QString{}), amountText, QString::fromStdString(record.name), QString::number(first.date.year())};
            e.keys = {w.total, static_cast<double>(first.date.toJulianDay()), -static_cast<double>(first.date.toJulianDay())};
            e.size = std::abs(amount);
            e.year = first.date.year();
            e.event.source = "climate-day";
            e.event.catalogId = "climate:" + record.sid + ":" + first.date.toString("yyyyMMdd").toStdString() + ":" + std::to_string(what);
            QString label = std::vector<QString>{"Snowfall", "Snowfall", "Rain", "Rain", "Heat", "Cold night", "Cold day"}[static_cast<size_t>(what)];
            e.event.name = (label + " " + amountText + ", " + QString::fromStdString(record.name) + ", " + first.date.toString("yyyy-MM-dd")).toStdString();
            e.event.start = QDateTime{first.date, QTime{0, 0}, QTimeZone::utc()}.addSecs(shift * 3600LL);   // local midnight
            e.event.end = QDateTime{last.date, QTime{0, 0}, QTimeZone::utc()}.addSecs((shift + 24) * 3600LL);
            e.event.lat = record.lat;
            e.event.lon = record.lon;
            e.event.radiusKm = 150.0;
            e.event.note = (label + " " + amountText + " at " + QString::fromStdString(record.name) + " (" + first.date.toString("yyyy-MM-dd") + (span > 1 ? " to " + last.date.toString("yyyy-MM-dd") : QString{}) +
                            "), ACIS station record; local days.").toStdString();
            out.push_back(std::move(e));
            if (++kept >= 120) {
                break;
            }
        }
        return out;
    };
    return source;
}

// The days of a Day 1 High risk of the SPC: the list is a resource (res/spc_high_risk_days.txt, made from the IEM archive of the outlooks), the verification the tornado database's
CatalogDialog::Source CatalogSources::highRiskDays() {
    CatalogDialog::Source source;
    source.name = "SPC Day 1 High risk days (2002 on) and the tornadoes that followed";
    source.columns = {"Convective day", "High first issued (UTC)", "Issuances", "Tornadoes", "Strongest", "Deaths", "Area"};
    source.orders = {"Deadliest", "Most tornadoes", "Strongest", "Newest", "Oldest"};
    source.sizeLabel = "tornadoes";
    source.sizeDefault = 0;
    source.note = "The convective day runs 12Z to 12Z. The tornadoes are those of the SPC database that day (the days before the database's last year are final).";
    source.load = [] (const CatalogDialog::Params&, std::string& error) {
        std::vector<Entry> out;
        QFile file{":/res/spc_high_risk_days.txt"};
        if (!file.open(QIODevice::ReadOnly)) {
            error = "the list of High risk days is not in this build";
            return out;
        }
        const auto database = TornadoData::load();
        struct Timed {
            qint64 time;
            const UtilityTornado::Tornado * tornado;
        };
        std::vector<Timed> timed;
        if (database) {
            for (const auto& t : database->tornadoes) {
                if (t.counts() && t.year >= 2002) {
                    timed.push_back({UtilityOutbreaks::utcOf(t).toSecsSinceEpoch(), &t});
                }
            }
            std::sort(timed.begin(), timed.end(), [] (const Timed& a, const Timed& b) { return a.time < b.time; });
        }
        for (const auto& line : QString::fromUtf8(file.readAll()).split('\n', Qt::SkipEmptyParts)) {
            if (line.startsWith('#')) {
                continue;
            }
            const auto parts = line.split(' ', Qt::SkipEmptyParts);
            if (parts.size() < 7) {
                continue;
            }
            auto ends = QDateTime::fromString(parts[0] + "12", "yyyyMMddHH");
            ends.setTimeZone(QTimeZone::utc());
            auto issued = QDateTime::fromString(parts[1], "yyyyMMddHHmm");
            issued.setTimeZone(QTimeZone::utc());
            const auto starts = ends.addDays(-1);
            const double west = parts[3].toDouble(), south = parts[4].toDouble(), east = parts[5].toDouble(), north = parts[6].toDouble();
            const double lat = (south + north) / 2.0, lon = (west + east) / 2.0;
            const double radius = std::clamp(UtilityTornado::kilometers(south, west, north, east) / 2.0 + 150.0, 300.0, 1400.0);
            // what the database says of that day
            int count = 0, deaths = 0, injuries = 0, strongest = -9;
            QDateTime strongestTime;
            std::string strongestState;
            std::map<qint64, int> byHour;
            const auto first = std::lower_bound(timed.begin(), timed.end(), starts.toSecsSinceEpoch(), [] (const Timed& t, qint64 v) { return t.time < v; });
            for (auto it = first; it != timed.end() && it->time < ends.toSecsSinceEpoch(); ++it) {
                count++;
                deaths += it->tornado->fatalities;
                injuries += it->tornado->injuries;
                byHour[it->time / 3600]++;
                if (it->tornado->mag > strongest) {
                    strongest = it->tornado->mag;
                    strongestTime = QDateTime::fromSecsSinceEpoch(it->time, QTimeZone::utc());
                    strongestState = it->tornado->state;
                }
            }
            const QString area = QString{"%1 to %2 N, %3 to %4 W"}.arg(south, 0, 'f', 0).arg(north, 0, 'f', 0).arg(-east, 0, 'f', 0).arg(-west, 0, 'f', 0);
            Entry e;
            e.cells = {starts.toString("yyyy-MM-dd") + " 12Z", issued.toString("MM-dd HH:mm"), parts[2], count > 0 || timed.empty() ? QString::number(count) : QString{"0"},
                       strongest >= 0 ? QString::fromStdString(UtilityTornado::ratingOf(strongest, starts.date().year())) : QString{"-"}, QString::number(deaths), area};
            e.keys = {deaths * 1000.0 + count, static_cast<double>(count), strongest * 100000.0 + deaths, static_cast<double>(starts.toSecsSinceEpoch()), -static_cast<double>(starts.toSecsSinceEpoch())};
            e.size = count;
            e.year = starts.date().year();
            e.event.source = "spc-high-risk";
            e.event.catalogId = "spc-high:" + parts[0].toStdString();
            e.event.name = ("SPC High risk, " + starts.toString("MMM d yyyy") + (count > 0 ? ": " + QString::number(count) + " tornadoes" : QString{})).toStdString();
            e.event.start = starts;
            e.event.end = ends;
            e.event.lat = lat;
            e.event.lon = lon;
            e.event.radiusKm = radius;
            e.event.note = ("SPC Day 1 outlook with a High risk for the convective day of " + starts.toString("yyyy-MM-dd") + ", first issued " + issued.toString("yyyy-MM-dd HH:mm") + "Z (" + parts[2] + " issuances). " +
                            (count > 0 ? QString{"The SPC database has %1 tornadoes that day, %2 deaths, %3 injuries."}.arg(count).arg(deaths).arg(injuries) : QString{"No tornado counts for that day yet."})).toStdString();
            e.event.moments.push_back({issued, "High risk first issued"});
            if (strongest >= 0) {
                e.event.moments.push_back({strongestTime, "strongest tornado: " + UtilityTornado::ratingOf(strongest, starts.date().year()) + " (" + strongestState + ")"});
            }
            int busiest = 0;
            qint64 busiestHour = 0;
            for (const auto& [hour, n] : byHour) {
                if (n > busiest) {
                    busiest = n;
                    busiestHour = hour;
                }
            }
            if (busiest > 1) {
                e.event.moments.push_back({QDateTime::fromSecsSinceEpoch(busiestHour * 3600, QTimeZone::utc()), "most touchdowns in an hour (" + std::to_string(busiest) + ")"});
            }
            std::sort(e.event.moments.begin(), e.event.moments.end(), [] (const auto& a, const auto& b) { return a.time < b.time; });
            out.push_back(std::move(e));
        }
        return out;
    };
    return source;
}

CatalogDialog::Source CatalogSources::tropicalStorms() {
    CatalogDialog::Source source;
    source.name = "Tropical storms and hurricanes (HURDAT2, Atlantic and eastern Pacific)";
    source.columns = {"Dates (UTC)", "Name", "Basin", "Peak wind", "Lowest pressure", "Strength"};
    source.orders = {"Strongest", "Lowest pressure", "Most energy (ACE)", "Longest lived", "Newest", "Oldest"};
    source.sizeLabel = "kt peak wind";
    source.sizeDefault = 64;
    source.note = "NHC's best tracks (named storms; the newest season is added when NHC publishes it).";
    source.load = [] (const CatalogDialog::Params&, std::string& error) {
        std::vector<Entry> out;
        for (const char * basin : {"al", "ep"}) {
            HurricaneData::TrackData data;
            HurricaneData::loadTracks(data, basin);
            if (!data.error.empty() && data.tracks.empty()) {
                error = data.error;
                continue;
            }
            for (const auto& t : data.tracks) {
                if (!t.stormStrength || t.points.size() < 2) {
                    continue;
                }
                const auto start = hurdatTime(t.points.front().time), end = hurdatTime(t.points.back().time);
                if (!start.isValid()) {
                    continue;
                }
                double latSum = 0.0, lonSum = 0.0;
                size_t peak = 0;
                size_t formed = 0;
                bool haveFormed = false;
                for (size_t i = 0; i < t.points.size(); i++) {
                    latSum += t.points[i].lat;
                    lonSum += t.points[i].lon;
                    if (t.points[i].wind > t.points[peak].wind) {
                        peak = i;
                    }
                    if (!haveFormed && t.points[i].wind >= 34) {
                        formed = i;
                        haveFormed = true;
                    }
                }
                const double lat = latSum / t.points.size(), lon = lonSum / t.points.size();
                double radius = 0.0;
                for (const auto& p : t.points) {
                    radius = std::max(radius, UtilityTornado::kilometers(lat, lon, p.lat, p.lon));
                }
                const QString name = QString::fromStdString(t.name == "UNNAMED" ? "(unnamed) " + t.id : t.name);
                Entry e;
                e.cells = {start.toString("yyyy-MM-dd") + " to " + end.toString("MM-dd"), name, QString{basin} == "al" ? "Atlantic" : "East Pacific", QString::number(t.peakWind) + " kt",
                           t.minPressure > 0 ? QString::number(t.minPressure) + " mb" : QString{"-"}, storm(t.peakWind)};
                e.keys = {static_cast<double>(t.peakWind) * 10000.0 + (t.minPressure > 0 ? 2000 - t.minPressure : 0), t.minPressure > 0 ? 2000.0 - t.minPressure : 0.0, UtilityHurdat::ace(t),
                          UtilityHurdat::durationDays(t), static_cast<double>(start.toSecsSinceEpoch()), -static_cast<double>(start.toSecsSinceEpoch())};
                e.size = t.peakWind;
                e.year = t.year;
                e.event.source = "storm";
                e.event.catalogId = "storm:" + t.id;
                e.event.name = (name + " " + QString::number(start.date().year()) + ", " + storm(t.peakWind)).toStdString();
                e.event.start = start.addSecs(-24 * 3600);
                e.event.end = end.addSecs(24 * 3600);
                e.event.lat = lat;
                e.event.lon = lon;
                e.event.radiusKm = std::min(3000.0, radius + 150.0);
                e.event.note = (name + " (" + QString::fromStdString(t.id) + "): peak " + QString::number(t.peakWind) + " kt" + (t.minPressure > 0 ? ", lowest pressure " + QString::number(t.minPressure) + " mb" : QString{}) +
                                ", ACE " + QString::number(UtilityHurdat::ace(t), 'f', 1) + ". NHC HURDAT2 best track.").toStdString();
                if (haveFormed) {
                    e.event.moments.push_back({hurdatTime(t.points[formed].time), "reaches tropical storm strength"});
                }
                e.event.moments.push_back({hurdatTime(t.points[peak].time), "peak intensity, " + std::to_string(t.points[peak].wind) + " kt"});
                std::sort(e.event.moments.begin(), e.event.moments.end(), [] (const auto& a, const auto& b) { return a.time < b.time; });
                out.push_back(std::move(e));
            }
        }
        return out;
    };
    return source;
}

std::vector<CatalogDialog::Source> CatalogSources::all() {
    return {tornadoOutbreaks(), highRiskDays(), biggestDays(), tropicalStorms()};
}
