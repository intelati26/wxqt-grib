// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "climate/UtilityAcisClimate.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <numbers>
#include <limits>
#include <mutex>
#include <set>
#include <thread>
#include <tuple>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QTextStream>
#include <QUrl>
#include "objects/NetManager.h"
#include "objects/URL.h"

namespace {
    std::mutex progressMutex;
    std::string stage;
    std::atomic<int> tilesDone{0};
    std::atomic<int> tilesAll{0};

    QString folder() {
        const QString path = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/acis";
        QDir{}.mkpath(path);
        return path;
    }

    using Band = AcisClimate::Band;

    // the departures of temperature, as the NWS map of the departure from normal draws them (degrees F): cold in blues, near normal grey, warm from pale yellow to dark red
    std::vector<Band> temperatureBands() {
        return {{-1e9, QColor{255, 190, 235}, ""}, {-9.0, QColor{0, 40, 140}, "-9"}, {-7.0, QColor{0, 100, 255}, "-7"}, {-5.0, QColor{0, 175, 235}, "-5"}, {-3.0, QColor{120, 225, 255}, "-3"},
                {-1.0, QColor{200, 200, 200}, "-1"}, {1.0, QColor{255, 255, 175}, "1"}, {3.0, QColor{255, 255, 0}, "3"}, {5.0, QColor{255, 170, 0}, "5"}, {7.0, QColor{225, 80, 0}, "7"},
                {9.0, QColor{160, 0, 0}, "9"}};
    }

    // precipitation: dry in browns, wet in greens to blues and purple (inches)
    std::vector<Band> precipDepartureBands() {
        return {{-1e9, QColor{130, 70, 0}, ""}, {-4.0, QColor{185, 105, 30}, "-4"}, {-2.0, QColor{225, 155, 75}, "-2"}, {-1.0, QColor{245, 205, 135}, "-1"}, {-0.5, QColor{250, 236, 205}, "-0.5"},
                {-0.1, QColor{232, 232, 232}, "-0.1"}, {0.1, QColor{200, 236, 200}, "0.1"}, {0.5, QColor{140, 210, 150}, "0.5"}, {1.0, QColor{60, 170, 110}, "1"}, {2.0, QColor{30, 140, 170}, "2"},
                {4.0, QColor{30, 80, 180}, "4"}, {8.0, QColor{100, 30, 150}, "8"}};
    }

    std::vector<Band> percentBands() {
        return {{-1e9, QColor{130, 70, 0}, ""}, {25.0, QColor{190, 110, 30}, "25"}, {50.0, QColor{230, 170, 80}, "50"}, {75.0, QColor{250, 220, 150}, "75"}, {90.0, QColor{235, 235, 235}, "90"},
                {110.0, QColor{190, 230, 190}, "110"}, {125.0, QColor{110, 200, 140}, "125"}, {150.0, QColor{40, 150, 150}, "150"}, {200.0, QColor{30, 90, 180}, "200"}, {300.0, QColor{110, 40, 160}, "300"}};
    }
}

const std::vector<AcisClimate::Region>& AcisClimate::regions() {
    static const std::vector<Region> all{
        {"conus", "Contiguous United States", -125.0, 24.0, -66.0, 50.0, 0.05, 190.0, 85.0},
        {"alaska", "Alaska", -170.0, 54.0, -130.0, 72.0, 0.1, 300.0, 170.0},
        {"hawaii", "Hawaii", -161.0, 18.5, -154.0, 22.5, 0.02, 60.0, 40.0},
    };
    return all;
}

// The measures. A new one is a new line here.
const std::vector<AcisClimate::Product>& AcisClimate::products() {
    static const std::vector<Product> all{
        {"avgt-dep", "Average temperature, departure from the 1991-2020 normal", "F", "avgt", Product::Kind::MeanDeparture, temperatureBands(), 1},
        {"maxt-dep", "Maximum temperature, departure from normal", "F", "maxt", Product::Kind::MeanDeparture, temperatureBands(), 1},
        {"mint-dep", "Minimum temperature, departure from normal", "F", "mint", Product::Kind::MeanDeparture, temperatureBands(), 1},
        {"pcpn-dep", "Precipitation, departure from normal (inches)", "in", "pcpn", Product::Kind::SumDeparture, precipDepartureBands(), 2},
        {"pcpn-pct", "Precipitation, percent of normal", "%", "pcpn", Product::Kind::PercentOfNormal, percentBands(), 0},
    };
    return all;
}

const AcisClimate::Product * AcisClimate::product(const std::string& id) {
    for (const auto& p : products()) {
        if (p.id == id) {
            return &p;
        }
    }
    return nullptr;
}

int AcisClimate::bandOf(const Product& product, double value) {
    int found = 0;
    for (size_t i = 0; i < product.bands.size(); i++) {
        if (value >= product.bands[i].from) {
            found = static_cast<int>(i);
        }
    }
    return found;
}

std::string AcisClimate::progressText() {
    std::lock_guard<std::mutex> lock{progressMutex};
    return stage + (tilesAll.load() > 0 ? " (" + std::to_string(tilesDone.load()) + " of " + std::to_string(tilesAll.load()) + " parts)" : std::string{});
}

namespace {
    struct Tile {
        double west, south, east, north;
    };

    // a number of the days, "M" and the like are missing
    bool number(const QJsonValue& v, double& out) {
        const QString text = v.toString();
        if (text.isEmpty() || text == "M" || text == "S" || text == "A") {
            return false;
        }
        bool ok = false;
        out = (text == "T" ? QString{"0"} : text).toDouble(&ok);
        return ok;
    }

    std::vector<AcisClimate::Station> readTile(const AcisClimate::Product& product, const Tile& tile, const QDate& from, const QDate& to, std::string& error) {
        using Kind = AcisClimate::Product::Kind;
        QJsonArray elems;
        QJsonObject departure{{"name", QString::fromStdString(product.element)}, {"interval", "dly"}, {"duration", "dly"}, {"normal", "departure"}};
        elems.append(departure);
        if (product.kind == Kind::PercentOfNormal) {
            elems.append(QJsonObject{{"name", QString::fromStdString(product.element)}, {"interval", "dly"}, {"duration", "dly"}, {"normal", "1"}});
        }
        const QJsonObject params{{"bbox", QString{"%1,%2,%3,%4"}.arg(tile.west).arg(tile.south).arg(tile.east).arg(tile.north)}, {"sdate", from.toString("yyyy-MM-dd")}, {"edate", to.toString("yyyy-MM-dd")},
                                 {"elems", elems}, {"meta", "ll,name,state"}};
        const std::string url = "https://data.rcc-acis.org/MultiStnData?params=" + QUrl::toPercentEncoding(QString::fromUtf8(QJsonDocument{params}.toJson(QJsonDocument::Compact))).toStdString();
        std::string text;
        for (int attempt = 0; attempt < 2 && text.empty(); attempt++) {
            text = URL::getText(url);
        }
        const auto doc = QJsonDocument::fromJson(QByteArray::fromStdString(text));
        if (!doc.isObject() || !doc.object().contains("data")) {
            error = doc.isObject() && doc.object().contains("error") ? doc.object().value("error").toString().toStdString() : "no answer from ACIS";
            return {};
        }
        const int days = static_cast<int>(from.daysTo(to)) + 1;
        const int allowed = std::max(1, days / 15);   // the NWS maps use stations with at most one day missing; a long period may miss a few more
        std::vector<AcisClimate::Station> out;
        for (const auto& item : doc.object().value("data").toArray()) {
            const auto station = item.toObject();
            const auto meta = station.value("meta").toObject();
            const auto ll = meta.value("ll").toArray();
            const QString state = meta.value("state").toString();
            if (ll.size() < 2 || state.isEmpty()) {
                continue;   // not a station of the United States
            }
            double sumDeparture = 0.0, sumNormal = 0.0;
            int have = 0;
            for (const auto& day : station.value("data").toArray()) {
                const auto cells = day.toArray();
                double d = 0.0, n = 0.0;
                if (cells.isEmpty() || !number(cells[0], d)) {
                    continue;
                }
                if (product.kind == Kind::PercentOfNormal && (cells.size() < 2 || !number(cells[1], n))) {
                    continue;
                }
                sumDeparture += d;
                sumNormal += n;
                have++;
            }
            if (have < days - allowed || have == 0) {
                continue;
            }
            float value = 0.0f;
            switch (product.kind) {
                case Kind::MeanDeparture: value = static_cast<float>(sumDeparture / have); break;
                case Kind::SumDeparture: value = static_cast<float>(sumDeparture); break;
                case Kind::PercentOfNormal:
                    if (sumNormal < 0.1) {
                        continue;   // a normal of next to nothing makes a percentage meaningless
                    }
                    value = static_cast<float>((sumNormal + sumDeparture) / sumNormal * 100.0);
                    break;
            }
            out.push_back({ll[1].toDouble(), ll[0].toDouble(), value, have, meta.value("name").toString().toStdString(), state.toStdString()});
        }
        return out;
    }
}

AcisClimate::Result AcisClimate::read(const Product& product, const Region& region, const QDate& from, const QDate& to) {
    Result result;
    result.from = from;
    result.to = to;
    const QString path = folder() + "/" + QString::fromStdString(product.id + "_" + region.id) + "_" + from.toString("yyyyMMdd") + "_" + to.toString("yyyyMMdd") + ".tsv";
    const QFileInfo info{path};
    const bool settled = to <= QDate::currentDate().addDays(-4);   // a period that ended days ago does not change (but for late reports)
    if (info.exists() && (settled || info.lastModified().secsTo(QDateTime::currentDateTime()) < 3 * 3600)) {
        QFile file{path};
        if (file.open(QIODevice::ReadOnly)) {
            for (const auto& line : QString::fromUtf8(file.readAll()).split('\n', Qt::SkipEmptyParts)) {
                const auto parts = line.split('\t');
                if (parts.size() >= 6) {
                    result.stations.push_back({parts[0].toDouble(), parts[1].toDouble(), parts[2].toFloat(), parts[3].toInt(), parts[5].toStdString(), parts[4].toStdString()});
                }
            }
            if (!result.stations.empty()) {
                return result;
            }
        }
    }
    // the region in tiles of 10 x 8 degrees, asked for on a few threads (one request for the whole country takes minutes)
    std::vector<Tile> tiles;
    for (double south = region.south; south < region.north; south += 8.0) {
        for (double west = region.west; west < region.east; west += 10.0) {
            tiles.push_back({west, south, std::min(west + 10.0, region.east), std::min(south + 8.0, region.north)});
        }
    }
    {
        std::lock_guard<std::mutex> lock{progressMutex};
        stage = "reading " + region.label.substr(0, 40) + " from ACIS";
    }
    tilesDone = 0;
    tilesAll = static_cast<int>(tiles.size());
    std::vector<std::vector<Station>> parts(tiles.size());
    std::vector<std::string> errors(tiles.size());
    std::atomic<size_t> next{0};
    const auto work = [&] {
        const NetManager::Scope background{NetManager::Priority::Ahead};
        for (;;) {
            const size_t i = next++;
            if (i >= tiles.size()) {
                return;
            }
            parts[i] = readTile(product, tiles[i], from, to, errors[i]);
            tilesDone++;
        }
    };
    std::vector<std::thread> pool;
    for (size_t k = 0; k < std::min<size_t>(6, tiles.size()); k++) {
        pool.emplace_back(work);
    }
    for (auto& thread : pool) {
        thread.join();
    }
    std::set<std::tuple<long, long, std::string>> seen;   // a station on the edge of two tiles
    int answered = 0;
    for (size_t i = 0; i < tiles.size(); i++) {
        if (errors[i].empty()) {
            answered++;
        } else if (result.error.empty()) {
            result.error = errors[i];
        }
        for (auto& s : parts[i]) {
            if (seen.insert({std::lround(s.lat * 1000.0), std::lround(s.lon * 1000.0), s.name}).second) {
                result.stations.push_back(std::move(s));
            }
        }
    }
    tilesAll = 0;
    if (answered == 0 || result.stations.empty()) {
        if (result.error.empty()) {
            result.error = "no stations had data for those days";
        }
        return result;
    }
    if (answered < static_cast<int>(tiles.size())) {
        result.error = "part of the region could not be read (" + result.error + ")";   // what was read is still returned, but not kept
    } else {
        QFile out{path};
        if (out.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream stream{&out};
            for (const auto& s : result.stations) {
                stream << s.lat << '\t' << s.lon << '\t' << s.value << '\t' << s.days << '\t' << QString::fromStdString(s.state) << '\t' << QString::fromStdString(s.name) << '\n';
            }
        }
    }
    return result;
}

// ---- the daily record of one place ----

namespace {
    QJsonDocument acisGet(const char * service, const QJsonObject& params, std::string& error) {
        const std::string url = std::string{"https://data.rcc-acis.org/"} + service + "?params=" + QUrl::toPercentEncoding(QString::fromUtf8(QJsonDocument{params}.toJson(QJsonDocument::Compact))).toStdString();
        std::string text;
        for (int attempt = 0; attempt < 2 && text.empty(); attempt++) {
            text = URL::getText(url);
        }
        const auto doc = QJsonDocument::fromJson(QByteArray::fromStdString(text));
        if (!doc.isObject() || doc.object().contains("error")) {
            error = doc.isObject() ? doc.object().value("error").toString().toStdString() : "no answer from ACIS";
            return {};
        }
        return doc;
    }

    float dayValue(const QJsonValue& v) {
        const QString text = v.toString();
        if (text.isEmpty() || text == "M") {
            return std::numeric_limits<float>::quiet_NaN();
        }
        if (text == "T") {
            return 0.0f;   // a trace
        }
        bool ok = false;
        const float value = text.toFloat(&ok);
        return ok ? value : std::numeric_limits<float>::quiet_NaN();
    }
}

AcisClimate::StationRecord AcisClimate::readStationRecord(double lat, double lon) {
    StationRecord record;
    {
        std::lock_guard<std::mutex> lock{progressMutex};
        stage = "finding the station nearest the place";
    }
    tilesAll = 0;
    // the stations near the point, with their records: a ThreadEx record ("SGFthr") of a city is the long one, with a station's own name
    const QJsonObject meta{{"bbox", QString{"%1,%2,%3,%4"}.arg(lon - 1.2).arg(lat - 0.9).arg(lon + 1.2).arg(lat + 0.9)}, {"elems", QJsonArray{"snow", "pcpn", "maxt"}}, {"meta", "name,ll,sids,valid_daterange"}};
    std::string error;
    const auto found = acisGet("StnMeta", meta, error);
    if (found.isNull()) {
        record.error = error;
        return record;
    }
    double bestScore = -1e9;
    QString bestSid, bestName;
    double bestLat = lat, bestLon = lon;
    struct Airport {
        double km;
        QString faa;
        double lat, lon;
    };
    std::vector<Airport> airports;   // the stations with an airport code: a city's ThreadEx record is named from it ("SGF" -> "SGFthr")
    for (const auto& item : found.object().value("meta").toArray()) {
        const auto station = item.toObject();
        const auto ll = station.value("ll").toArray();
        if (ll.size() < 2) {
            continue;
        }
        QString threaded, other;
        for (const auto& sid : station.value("sids").toArray()) {
            const QString text = sid.toString();
            if (text.contains("thr")) {
                threaded = text.section(' ', 0, 0);
            } else if (other.isEmpty() && text.endsWith(" 2")) {
                other = text.section(' ', 0, 0);   // a COOP id
            } else if (text.endsWith(" 3") && text.section(' ', 0, 0).size() == 3) {
                airports.push_back({std::hypot((ll[1].toDouble() - lat) * 111.2, (ll[0].toDouble() - lon) * 111.2 * std::cos(lat * std::numbers::pi / 180.0)), text.section(' ', 0, 0), ll[1].toDouble(), ll[0].toDouble()});
            }
        }
        const QString use = !threaded.isEmpty() ? threaded : other;
        if (use.isEmpty()) {
            continue;
        }
        // the years of record (the first range, the snow or the rain), and a station still reporting
        int years = 0;
        bool current = false;
        for (const auto& range : station.value("valid_daterange").toArray()) {
            const auto r = range.toArray();
            if (r.size() == 2) {
                const auto first = QDate::fromString(r[0].toString(), "yyyy-MM-dd"), last = QDate::fromString(r[1].toString(), "yyyy-MM-dd");
                years = std::max(years, static_cast<int>(first.daysTo(last) / 365));
                current = current || last >= QDate::currentDate().addDays(-60);
            }
        }
        const double dy = (ll[1].toDouble() - lat) * 111.2, dx = (ll[0].toDouble() - lon) * 111.2 * std::cos(lat * std::numbers::pi / 180.0);
        const double km = std::hypot(dx, dy);
        const double score = (threaded.isEmpty() ? 0.0 : 60.0) + std::min(years, 130) * 0.6 - km * 0.5 - (current ? 0.0 : 80.0);
        if (score > bestScore) {
            bestScore = score;
            bestSid = use;
            bestName = station.value("name").toString();
            bestLat = ll[1].toDouble();
            bestLon = ll[0].toDouble();
        }
    }
    // a city's ThreadEx record, from the nearest airports' codes (it has no position of its own, so the bounding box cannot find it)
    std::sort(airports.begin(), airports.end(), [] (const Airport& a, const Airport& b) { return a.km < b.km; });
    for (size_t i = 0; i < airports.size() && i < 4; i++) {
        std::string ignored;
        const auto check = acisGet("StnMeta", QJsonObject{{"sids", airports[i].faa + "thr"}, {"meta", "name"}}, ignored);
        const auto list = check.isNull() ? QJsonArray{} : check.object().value("meta").toArray();
        if (!list.isEmpty()) {
            bestSid = airports[i].faa + "thr";
            bestName = list[0].toObject().value("name").toString();
            bestLat = airports[i].lat;
            bestLon = airports[i].lon;
            break;
        }
    }
    if (bestSid.isEmpty()) {
        record.error = "no long-record station within about 100 km of that point";
        return record;
    }
    record.sid = bestSid.toStdString();
    record.name = bestName.toStdString();
    record.lat = bestLat;
    record.lon = bestLon;
    // kept on disk twelve hours (a record of 100 years is a few MB)
    const QString path = folder() + "/record_" + bestSid + ".json";
    QByteArray bytes;
    {
        const QFileInfo info{path};
        QFile file{path};
        if (info.exists() && info.lastModified().secsTo(QDateTime::currentDateTime()) < 12 * 3600 && file.open(QIODevice::ReadOnly)) {
            bytes = file.readAll();
        }
    }
    QJsonDocument doc = bytes.isEmpty() ? QJsonDocument{} : QJsonDocument::fromJson(bytes);
    if (doc.isNull()) {
        {
            std::lock_guard<std::mutex> lock{progressMutex};
            stage = "reading the record of " + bestName.toStdString();
        }
        const QJsonObject params{{"sid", bestSid}, {"sdate", "por"}, {"edate", QDate::currentDate().toString("yyyy-MM-dd")},
                                 {"elems", QJsonArray{QJsonObject{{"name", "snow"}}, QJsonObject{{"name", "pcpn"}}, QJsonObject{{"name", "maxt"}}, QJsonObject{{"name", "mint"}}}}};
        doc = acisGet("StnData", params, error);
        if (doc.isNull()) {
            record.error = error;
            return record;
        }
        QFile out{path};
        if (out.open(QIODevice::WriteOnly)) {
            out.write(doc.toJson(QJsonDocument::Compact));
        }
    }
    for (const auto& row : doc.object().value("data").toArray()) {
        const auto cells = row.toArray();
        if (cells.size() < 5) {
            continue;
        }
        record.days.push_back({QDate::fromString(cells[0].toString(), "yyyy-MM-dd"), dayValue(cells[1]), dayValue(cells[2]), dayValue(cells[3]), dayValue(cells[4])});
    }
    if (record.days.empty()) {
        record.error = "the station has no daily record";
    }
    return record;
}
