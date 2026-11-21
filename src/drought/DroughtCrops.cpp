// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "drought/DroughtCrops.h"
#include "drought/DroughtVector.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <mutex>
#include <sstream>
#include <QByteArray>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>
#include <QUrlQuery>
#include "objects/URL.h"
#include "util/PermanentCache.h"
#include "util/UtilityZip.h"

namespace {
    constexpr double west = -130.0, south = 20.0, east = -60.0, north = 55.0, step = 0.05;

    std::string gazetteerUrl() {
        return "https://www2.census.gov/geo/docs/maps-data/data/gazetteer/2022_Gazetteer/2022_Gaz_counties_national.zip";
    }

    // the item's census years come out of Quick Stats one request each: be gentle (the terms ask for no more than needed)
    std::string quickStats(const std::string& key, const std::string& item, int census, const std::string& apiKey) {
        QUrl url{"https://quickstats.nass.usda.gov/api/api_GET"};
        QUrlQuery query;
        query.addQueryItem("key", QString::fromStdString(apiKey));
        query.addQueryItem("source_desc", "CENSUS");
        query.addQueryItem("year", QString::number(census));
        query.addQueryItem("agg_level_desc", "COUNTY");
        query.addQueryItem("short_desc", QString::fromStdString(item));
        query.addQueryItem("domain_desc", "TOTAL");
        query.addQueryItem("format", "JSON");
        url.setQuery(query);
        (void)key;
        return URL::getBytes(url.toString(QUrl::FullyEncoded).toStdString()).toStdString();
    }
}

namespace {
    bool inRing(const UtilityDrought::Ring& ring, double lon, double lat) {
        bool inside = false;
        for (size_t i = 0, j = ring.size() - 1; i < ring.size(); j = i++) {
            const auto& a = ring[i];
            const auto& b = ring[j];
            if ((a.second > lat) != (b.second > lat) && lon < (b.first - a.first) * (lat - a.second) / (b.second - a.second) + a.first) {
                inside = !inside;
            }
        }
        return inside;
    }
}

namespace DroughtCrops {
    const std::vector<Commodity>& commodities() {
        static const std::vector<Commodity> list{
            {"corn", "Corn", "CORN, GRAIN - PRODUCTION, MEASURED IN BU", "bushels"},
            {"soybeans", "Soybeans", "SOYBEANS - PRODUCTION, MEASURED IN BU", "bushels"},
            {"winterwheat", "Winter wheat", "WHEAT, WINTER - PRODUCTION, MEASURED IN BU", "bushels"},
            {"springwheat", "Spring wheat", "WHEAT, SPRING, (EXCL DURUM) - PRODUCTION, MEASURED IN BU", "bushels"},
            {"durum", "Durum wheat", "WHEAT, SPRING, DURUM - PRODUCTION, MEASURED IN BU", "bushels"},
            {"cotton", "Cotton (upland)", "COTTON, UPLAND - PRODUCTION, MEASURED IN BALES", "bales"},
            {"rice", "Rice", "RICE - PRODUCTION, MEASURED IN CWT", "hundredweight"},
            {"sorghum", "Sorghum", "SORGHUM, GRAIN - PRODUCTION, MEASURED IN BU", "bushels"},
            {"barley", "Barley", "BARLEY - PRODUCTION, MEASURED IN BU", "bushels"},
            {"peanuts", "Peanuts", "PEANUTS - PRODUCTION, MEASURED IN LB", "pounds"},
            {"sunflowers", "Sunflowers", "SUNFLOWER - PRODUCTION, MEASURED IN LB", "pounds"},
            {"sugarbeets", "Sugarbeets", "SUGARBEETS - PRODUCTION, MEASURED IN TONS", "tons"},
            {"sugarcane", "Sugarcane", "SUGARCANE, SUGAR & SEED - PRODUCTION, MEASURED IN TONS", "tons", "SUGARCANE, SUGAR - PRODUCTION, MEASURED IN TONS"},
            {"hay", "Hay", "HAY - ACRES HARVESTED", "acres"},
            {"alfalfa", "Alfalfa hay", "HAY, ALFALFA - ACRES HARVESTED", "acres"},
            {"cattle", "Cattle (excl. cows)", "CATTLE, (EXCL COWS) - INVENTORY", "head"},
            {"milkcows", "Milk cows", "CATTLE, COWS, MILK - INVENTORY", "head"},
            {"hogs", "Hogs", "HOGS - INVENTORY", "head"},
            {"sheep", "Sheep", "SHEEP, INCL LAMBS - INVENTORY", "head"},
        };
        return list;
    }

    const Commodity * find(const std::string& key) {
        for (const auto& c : commodities()) {
            if (c.key == key) {
                return &c;
            }
        }
        return nullptr;
    }

    double Table::total() const {
        double sum = 0;
        for (const auto& c : counties) {
            sum += c.value;
        }
        return sum;
    }

    int censusFor(const QDate& week) {
        return week < QDate{2024, 7, 1} ? 2017 : 2022;
    }

    std::string toCsv(const Table& table) {
        std::ostringstream out;
        out << "# " << table.key << " " << table.census << " county production (fips,value)\n";
        for (const auto& c : table.counties) {
            out << c.fips << ',' << static_cast<long long>(std::llround(c.value)) << '\n';
        }
        return out.str();
    }

    std::string parseQuickStats(const std::string& json, std::string& error) {
        QJsonParseError parse;
        const auto doc = QJsonDocument::fromJson(QByteArray::fromStdString(json), &parse);
        if (!doc.isObject()) {
            error = "Quick Stats did not answer (" + parse.errorString().toStdString() + ")";
            return {};
        }
        const auto data = doc.object().value("data").toArray();
        if (data.isEmpty()) {
            const auto message = doc.object().value("error").toArray();
            error = message.isEmpty() ? "Quick Stats has no counties for that item and year" : message.first().toString().toStdString();
            return {};
        }
        std::ostringstream out;
        int kept = 0;
        for (const auto& row : data) {
            const auto o = row.toObject();
            const auto fips = o.value("state_fips_code").toString() + o.value("county_code").toString();
            if (fips.size() != 5 || o.value("county_code").toString() == "998" || o.value("county_code").toString() == "999") {   // (the state's "other counties" and "other" lines)
                continue;
            }
            bool ok = false;
            const double value = o.value("Value").toString().remove(',').toDouble(&ok);
            if (!ok || value <= 0) {
                continue;   // "(D)": the Census withholds a county with a farm or two
            }
            out << fips.toStdString() << ',' << static_cast<long long>(std::llround(value)) << '\n';
            ++kept;
        }
        if (kept == 0) {
            error = "no county has a number";
            return {};
        }
        return out.str();
    }

    bool parseGazetteer(const std::string& text, std::vector<County>& centers) {
        std::string body = text;
        if (body.size() > 2 && body[0] == 'P' && body[1] == 'K') {
            std::map<std::string, std::string> files;
            UtilityZip::read(body, files);
            body.clear();
            for (const auto& [name, contents] : files) {
                if (name.find(".txt") != std::string::npos) {
                    body = contents;
                }
            }
        }
        std::istringstream in{body};
        std::string line;
        int geoid = -1, lat = -1, lon = -1;
        bool header = true;
        while (std::getline(in, line)) {
            std::vector<std::string> cells;
            std::string cell;
            std::istringstream row{line};
            while (std::getline(row, cell, '\t')) {
                while (!cell.empty() && (cell.back() == ' ' || cell.back() == '\r')) {
                    cell.pop_back();
                }
                cells.push_back(cell);
            }
            if (header) {
                for (size_t i = 0; i < cells.size(); ++i) {
                    geoid = cells[i] == "GEOID" ? static_cast<int>(i) : geoid;
                    lat = cells[i] == "INTPTLAT" ? static_cast<int>(i) : lat;
                    lon = cells[i] == "INTPTLONG" ? static_cast<int>(i) : lon;
                }
                header = false;
                if (geoid < 0 || lat < 0 || lon < 0) {
                    return false;
                }
                continue;
            }
            if (static_cast<int>(cells.size()) <= std::max({geoid, lat, lon})) {
                continue;
            }
            County c;
            c.fips = cells[geoid];
            c.lat = std::atof(cells[lat].c_str());
            c.lon = std::atof(cells[lon].c_str());
            centers.push_back(c);
        }
        return !centers.empty();
    }

    Table fromCsv(const std::string& key, int census, const std::string& csv, const std::vector<County>& centers) {
        Table table;
        table.key = key;
        table.census = census;
        std::map<std::string, std::pair<double, double>> where;
        for (const auto& c : centers) {
            where[c.fips] = {c.lat, c.lon};
        }
        std::istringstream in{csv};
        std::string line;
        while (std::getline(in, line)) {
            if (line.empty() || line[0] == '#') {
                continue;
            }
            const auto comma = line.find(',');
            if (comma == std::string::npos) {
                continue;
            }
            County c;
            c.fips = line.substr(0, comma);
            c.value = std::atof(line.c_str() + comma + 1);
            const auto at = where.find(c.fips);
            if (at == where.end() || c.value <= 0) {
                continue;   // (a county that has changed since: no center)
            }
            c.lat = at->second.first;
            c.lon = at->second.second;
            table.counties.push_back(c);
        }
        return table;
    }

    Table table(const Commodity& commodity, int census, std::string& error, const std::string& apiKey) {
        const PermanentCache store{"drought"};
        const auto name = "crops/" + commodity.key + "_" + std::to_string(census) + ".csv";
        std::string csv = store.read(name);
        if (csv.empty()) {
            std::string key = apiKey;
            if (key.empty()) {
                key = qgetenv("WXQT_NASS_KEY").toStdString();
            }
            if (key.empty()) {
                error = "the " + std::to_string(census) + " census table of " + commodity.name + " is not on this computer, and fetching it needs a free Quick Stats key (quickstats.nass.usda.gov/api): set WXQT_NASS_KEY";
                return {};
            }
            csv = parseQuickStats(quickStats(commodity.key, commodity.item, census, key), error);
            if (csv.empty() && !commodity.altItem.empty()) {
                error.clear();
                csv = parseQuickStats(quickStats(commodity.key, commodity.altItem, census, key), error);
            }
            if (csv.empty()) {
                return {};
            }
            store.write(name, csv);
        }
        const auto& list = DroughtCrops::centers(error);
        if (list.empty()) {
            return {};
        }
        return fromCsv(commodity.key, census, csv, list);
    }

    bool monitor(const QDate& tuesday, UtilityDrought::Monitor& out, std::string& error) {
        const PermanentCache store{"drought"};
        const auto date = tuesday.toString("yyyyMMdd").toStdString();
        const auto name = "vector/" + date + "_v1.bin";
        const auto kept = store.read(name);
        if (!kept.empty() && DroughtVector::decode(kept, out)) {
            return true;
        }
        // not kept yet: the week's KMZ (a few hundred KB, not stored: the window's own tabs keep theirs) made into the small form once
        const auto kmz = URL::getBytes("https://droughtmonitor.unl.edu/data/kmz/usdm_" + date + ".kmz").toStdString();
        UtilityDrought::Monitor map;
        if (kmz.size() < 50000 || !UtilityDrought::parseKmz(kmz, map, error)) {
            if (error.empty()) {
                error = "the map of " + date + " is not available";
            }
            return false;
        }
        const auto small = DroughtVector::encode(map);
        if (!DroughtVector::decode(small, out)) {
            error = "the map of " + date + " could not be kept";
            return false;
        }
        store.write(name, small);
        return true;
    }

    int WeekGrid::categoryAt(double lat, double lon) const {
        const int r = static_cast<int>(std::floor((north - lat) / step));
        const int col = static_cast<int>(std::floor((lon - west) / step));
        if (cells.empty() || r < 0 || col < 0 || r >= rows || col >= columns) {
            return -1;
        }
        return cells[static_cast<size_t>(r) * static_cast<size_t>(columns) + static_cast<size_t>(col)];
    }

    bool WeekGrid::atLeastAt(double lat, double lon, double (&share)[5]) const {
        const double y = (north - lat) / step - 0.5, x = (lon - west) / step - 0.5;   // in cells, the cell centers at whole numbers
        const int r0 = static_cast<int>(std::floor(y)), c0 = static_cast<int>(std::floor(x));
        const double fy = y - r0, fx = x - c0;
        if (cells.empty() || r0 < 0 || c0 < 0 || r0 + 1 >= rows || c0 + 1 >= columns) {
            return false;
        }
        for (int k = 0; k < 5; ++k) {
            share[k] = 0;
        }
        for (int dr = 0; dr < 2; ++dr) {
            for (int dc = 0; dc < 2; ++dc) {
                const double weight = (dr ? fy : 1.0 - fy) * (dc ? fx : 1.0 - fx);
                const int category = cells[static_cast<size_t>(r0 + dr) * static_cast<size_t>(columns) + static_cast<size_t>(c0 + dc)];
                for (int k = 0; k < 5; ++k) {
                    if (category >= k + 1) {
                        share[k] += weight;
                    }
                }
            }
        }
        return true;
    }

    // each row: the number of runs (2 bytes), then for each run its category and its length (a varint)
    std::string encode(const WeekGrid& grid) {
        std::string out;
        out.reserve(250000);
        for (int r = 0; r < WeekGrid::rows; ++r) {
            const auto * row = grid.cells.data() + static_cast<size_t>(r) * WeekGrid::columns;
            const size_t countAt = out.size();
            out.append(2, '\0');
            int runs = 0;
            for (int c = 0; c < WeekGrid::columns;) {
                int end = c + 1;
                while (end < WeekGrid::columns && row[end] == row[c]) {
                    ++end;
                }
                out.push_back(static_cast<char>(row[c]));
                for (unsigned length = static_cast<unsigned>(end - c);; length >>= 7) {
                    const auto low = static_cast<char>(length & 0x7f);
                    if (length >> 7) {
                        out.push_back(static_cast<char>(low | 0x80));
                    } else {
                        out.push_back(low);
                        break;
                    }
                }
                ++runs;
                c = end;
            }
            out[countAt] = static_cast<char>(runs & 0xff);
            out[countAt + 1] = static_cast<char>(runs >> 8);
        }
        return out;
    }

    bool decode(const std::string& bytes, WeekGrid& grid) {
        grid.cells.assign(static_cast<size_t>(WeekGrid::columns) * WeekGrid::rows, 0);
        size_t at = 0;
        for (int r = 0; r < WeekGrid::rows; ++r) {
            if (at + 2 > bytes.size()) {
                grid.cells.clear();
                return false;
            }
            const int runs = static_cast<unsigned char>(bytes[at]) | (static_cast<unsigned char>(bytes[at + 1]) << 8);
            at += 2;
            int c = 0;
            for (int i = 0; i < runs; ++i) {
                if (at >= bytes.size()) {
                    grid.cells.clear();
                    return false;
                }
                const auto value = static_cast<uint8_t>(bytes[at++]);
                unsigned length = 0;
                for (int shift = 0; at < bytes.size(); shift += 7) {
                    const auto b = static_cast<unsigned char>(bytes[at++]);
                    length |= static_cast<unsigned>(b & 0x7f) << shift;
                    if (!(b & 0x80)) {
                        break;
                    }
                }
                if (c + static_cast<int>(length) > WeekGrid::columns) {
                    grid.cells.clear();
                    return false;
                }
                std::fill_n(grid.cells.begin() + static_cast<long>(r) * WeekGrid::columns + c, length, value);
                c += static_cast<int>(length);
            }
        }
        return true;
    }

    bool weekGrid(const QDate& tuesday, WeekGrid& out, std::string& error) {
        UtilityDrought::Monitor map;
        if (!monitor(tuesday, map, error)) {
            return false;
        }
        const auto raster = UtilityDrought::rasterize(map, WeekGrid::west, WeekGrid::north - WeekGrid::rows * WeekGrid::step, WeekGrid::west + WeekGrid::columns * WeekGrid::step, WeekGrid::north, WeekGrid::step);
        if (raster.columns != WeekGrid::columns || raster.rows != WeekGrid::rows) {
            error = "the grid of the map has the wrong size";
            return false;
        }
        out.cells = raster.category;
        return true;
    }

    const std::vector<County>& centers(std::string& error) {
        static std::vector<County> list;
        static std::mutex lock;
        const std::lock_guard guard{lock};
        if (list.empty()) {
            const PermanentCache store{"drought"};
            const auto zip = store.fetch("gazetteer_counties_2022.zip", "gazetteer_counties_*.zip", 10000, [] { return URL::getBytes(gazetteerUrl()).toStdString(); });
            if (zip.text.empty() || !parseGazetteer(zip.text, list)) {
                list.clear();
                error = "the Census Bureau's county centers could not be had";
            } else {
                std::sort(list.begin(), list.end(), [] (const County& a, const County& b) { return a.fips < b.fips; });
            }
        }
        return list;
    }

    bool weekCounties(const QDate& tuesday, WeekCounties& out, std::string& error) {
        const auto& list = centers(error);
        if (list.empty()) {
            return false;
        }
        const PermanentCache store{"drought"};
        const auto name = "counties/" + tuesday.toString("yyyyMMdd").toStdString() + "_v1.bin";
        const auto kept = store.read(name);
        out.part.clear();
        if (kept.size() == 7 + list.size() * 5 && kept.compare(0, 3, "DC1") == 0) {   // "DC1", the number of counties (4 bytes), then 5 bytes each in the order of the centers
            for (size_t i = 0; i < list.size(); ++i) {
                std::array<uint8_t, 5> p{};
                for (size_t k = 0; k < 5; ++k) {
                    p[k] = static_cast<uint8_t>(kept[7 + i * 5 + k]);
                }
                out.part[list[i].fips] = p;
            }
            return true;
        }
        WeekGrid grid;
        if (!weekGrid(tuesday, grid, error)) {
            return false;
        }
        std::string bytes{"DC1"};
        const auto n = static_cast<uint32_t>(list.size());
        for (int i = 0; i < 4; ++i) {
            bytes.push_back(static_cast<char>((n >> (8 * i)) & 0xff));
        }
        for (const auto& c : list) {
            std::array<uint8_t, 5> p{};
            double part[5];
            if (grid.atLeastAt(c.lat, c.lon, part)) {
                for (size_t k = 0; k < 5; ++k) {
                    p[k] = static_cast<uint8_t>(std::lround(part[k] * 255.0));
                }
            }
            out.part[c.fips] = p;
            bytes.append(reinterpret_cast<const char *>(p.data()), 5);
        }
        store.write(name, bytes);
        return true;
    }

    std::set<std::string> countiesIn(const std::vector<UtilityDrought::Area>& areas) {
        std::string error;
        std::set<std::string> out;
        for (const auto& c : centers(error)) {
            bool in = false;
            for (const auto& area : areas) {
                if (c.lon < area.west || c.lon > area.east || c.lat < area.south || c.lat > area.north) {
                    continue;
                }
                for (const auto& polygon : area.shapes) {
                    if (polygon.empty() || !inRing(polygon.front(), c.lon, c.lat)) {
                        continue;
                    }
                    bool hole = false;
                    for (size_t h = 1; h < polygon.size() && !hole; ++h) {
                        hole = inRing(polygon[h], c.lon, c.lat);
                    }
                    in = in || !hole;
                }
                if (in) {
                    break;
                }
            }
            if (in) {
                out.insert(c.fips);
            }
        }
        return out;
    }

    Share share(const WeekCounties& week, const Table& table, const std::set<std::string> * only) {
        Share out;
        double sum[5]{};
        for (const auto& c : table.counties) {
            if (c.lat < 20.0 || c.lat >= 55.0 || c.lon <= -130.0 || c.lon >= -60.0) {
                continue;   // (Alaska, Hawaii and the territories: the Monitor's map covers them, the report's figures do not)
            }
            if (only != nullptr && only->count(c.fips) == 0) {
                continue;
            }
            const auto at = week.part.find(c.fips);
            if (at == week.part.end()) {
                continue;
            }
            out.total += c.value;
            ++out.counties;
            for (size_t k = 0; k < 5; ++k) {
                sum[k] += c.value * at->second[k] / 255.0;
            }
        }
        for (int k = 0; k < 5; ++k) {
            out.atLeast[k] = out.total > 0 ? 100.0 * sum[k] / out.total : 0.0;
        }
        return out;
    }
}
