// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "drought/UtilityDrought.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>
#include <QRegularExpression>
#include "hurricane/UtilityShapefile.h"
#include "util/UtilityZip.h"

namespace UtilityDrought {
    namespace {
        // "lon,lat,alt lon,lat,alt ..." of a coordinates element
        Ring parseCoordinates(const std::string& text) {
            Ring ring;
            const char * p = text.c_str();
            char * end = nullptr;
            while (*p) {
                const double lon = std::strtod(p, &end);
                if (end == p) {
                    p++;
                    continue;
                }
                p = end;
                if (*p != ',') {
                    continue;
                }
                const double lat = std::strtod(p + 1, &end);
                if (end == p + 1) {
                    break;
                }
                p = end;
                if (*p == ',') {   // the altitude
                    std::strtod(p + 1, &end);
                    p = end;
                }
                ring.emplace_back(lon, lat);
            }
            return ring;
        }

        // the text between <tag> and </tag> from `from`, advancing `from` past the end tag; false when there is none
        bool element(const std::string& text, const std::string& tag, size_t& from, std::string& inside, size_t limit = std::string::npos) {
            const std::string open = "<" + tag;
            size_t at = from;
            while (true) {
                at = text.find(open, at);
                if (at == std::string::npos || at >= limit) {
                    return false;
                }
                const char next = at + open.size() < text.size() ? text[at + open.size()] : '\0';
                if (next == '>' || next == ' ' || next == '\n') {   // the tag itself, with or without attributes, not a longer name that starts the same
                    break;
                }
                at += open.size();
            }
            const auto start = text.find('>', at);
            if (start == std::string::npos) {
                return false;
            }
            const auto close = text.find("</" + tag + ">", start);
            if (close == std::string::npos) {
                return false;
            }
            inside = text.substr(start + 1, close - start - 1);
            from = close + tag.size() + 3;
            return true;
        }

        void bounds(Area& a) {
            a.west = a.south = 1e9;
            a.east = a.north = -1e9;
            for (const auto& polygon : a.shapes) {
                for (const auto& ring : polygon) {
                    for (const auto& [lon, lat] : ring) {
                        a.west = std::min(a.west, lon);
                        a.east = std::max(a.east, lon);
                        a.south = std::min(a.south, lat);
                        a.north = std::max(a.north, lat);
                    }
                }
            }
        }

        QPainterPath pathOf(const std::vector<Polygon>& shapes, const Raster& r) {
            QPainterPath path;
            path.setFillRule(Qt::OddEvenFill);   // a hole is a ring inside a ring
            for (const auto& polygon : shapes) {
                for (const auto& ring : polygon) {
                    QPolygonF poly;
                    for (const auto& [lon, lat] : ring) {
                        poly << QPointF{(lon - r.west) / r.step, (r.north - lat) / r.step};
                    }
                    path.addPolygon(poly);
                    path.closeSubpath();
                }
            }
            return path;
        }
    }

    bool parseKmz(const std::string& kmz, Monitor& out, std::string& error) {
        out = Monitor{};
        std::map<std::string, std::string> files;
        UtilityZip::read(kmz, files);
        std::string kml;
        for (const auto& [name, bytes] : files) {
            if (name.size() >= 4 && name.compare(name.size() - 4, 4, ".kml") == 0) {
                kml = bytes;
            }
        }
        if (kml.empty()) {
            error = "no map in the KMZ file";
            return false;
        }
        const auto valid = QRegularExpression{"Valid:.*?([0-9]{4})/([0-9]{2})/([0-9]{2})"}.match(QString::fromStdString(kml.substr(0, 2000)));
        if (valid.hasMatch()) {
            out.valid = QDate{valid.captured(1).toInt(), valid.captured(2).toInt(), valid.captured(3).toInt()};
        }
        size_t from = 0;
        std::string placemark;
        int found = 0;
        while (element(kml, "Placemark", from, placemark)) {
            size_t at = 0;
            std::string name;
            if (!element(placemark, "name", at, name)) {
                continue;
            }
            const int category = std::atoi(name.c_str());
            if (category < 0 || category > 4) {
                continue;
            }
            size_t pAt = 0;
            std::string polygon;
            while (element(placemark, "Polygon", pAt, polygon)) {
                Polygon shape;
                size_t oAt = 0;
                std::string outer;
                if (element(polygon, "outerBoundaryIs", oAt, outer)) {
                    size_t cAt = 0;
                    std::string coordinates;
                    if (element(outer, "coordinates", cAt, coordinates)) {
                        shape.push_back(parseCoordinates(coordinates));
                    }
                }
                size_t iAt = 0;
                std::string inner;
                while (element(polygon, "innerBoundaryIs", iAt, inner)) {
                    size_t cAt = 0;
                    std::string coordinates;
                    if (element(inner, "coordinates", cAt, coordinates)) {
                        shape.push_back(parseCoordinates(coordinates));
                    }
                }
                if (!shape.empty() && shape.front().size() >= 3) {
                    out.shapes[category].push_back(std::move(shape));
                    found++;
                }
            }
        }
        if (found == 0) {
            error = "the KMZ file has no drought shapes";
            return false;
        }
        return true;
    }

    bool parseAreas(const std::string& zip, bool counties, bool all, std::vector<Area>& out, std::string& error) {
        std::map<std::string, std::string> files;
        UtilityZip::read(zip, files);
        std::string shp, dbf;
        for (const auto& [name, bytes] : files) {
            if (name.size() > 4 && name.compare(name.size() - 4, 4, ".shp") == 0) {
                shp = bytes;
            } else if (name.size() > 4 && name.compare(name.size() - 4, 4, ".dbf") == 0) {
                dbf = bytes;
            }
        }
        std::vector<UtilityShapefile::Feature> features;
        if (shp.empty() || dbf.empty() || !UtilityShapefile::parse(shp, dbf, features)) {
            error = "could not read the boundary file";
            return false;
        }
        static const std::map<std::string, std::string> states{
            {"01", "AL"}, {"02", "AK"}, {"04", "AZ"}, {"05", "AR"}, {"06", "CA"}, {"08", "CO"}, {"09", "CT"}, {"10", "DE"}, {"11", "DC"}, {"12", "FL"}, {"13", "GA"}, {"15", "HI"},
            {"16", "ID"}, {"17", "IL"}, {"18", "IN"}, {"19", "IA"}, {"20", "KS"}, {"21", "KY"}, {"22", "LA"}, {"23", "ME"}, {"24", "MD"}, {"25", "MA"}, {"26", "MI"}, {"27", "MN"},
            {"28", "MS"}, {"29", "MO"}, {"30", "MT"}, {"31", "NE"}, {"32", "NV"}, {"33", "NH"}, {"34", "NJ"}, {"35", "NM"}, {"36", "NY"}, {"37", "NC"}, {"38", "ND"}, {"39", "OH"},
            {"40", "OK"}, {"41", "OR"}, {"42", "PA"}, {"44", "RI"}, {"45", "SC"}, {"46", "SD"}, {"47", "TN"}, {"48", "TX"}, {"49", "UT"}, {"50", "VT"}, {"51", "VA"}, {"53", "WA"},
            {"54", "WV"}, {"55", "WI"}, {"56", "WY"}, {"72", "PR"}};
        for (auto& f : features) {
            if (f.shapeType != 5) {
                continue;
            }
            const auto state = f.attributes.count("STATEFP") ? f.attributes["STATEFP"] : std::string{};
            if (!all && (state == "02" || state == "15" || state == "72" || state > "56")) {   // Alaska, Hawaii, Puerto Rico and the territories: not on the contiguous map
                continue;
            }
            Area a;
            a.id = f.attributes.count("GEOID") ? f.attributes["GEOID"] : state;
            const auto attribute = [&f] (const char * key) { return f.attributes.count(key) ? f.attributes[key] : std::string{}; };
            const auto name = !attribute("NAME").empty() ? attribute("NAME") : a.id;
            const auto abbreviation = !attribute("STUSPS").empty() ? attribute("STUSPS") : states.count(state) ? states.at(state) : std::string{};
            a.name = counties ? (!attribute("NAMELSAD").empty() ? attribute("NAMELSAD") : name + " County") + ", " + abbreviation : name;   // "Denver County, CO", "Orleans Parish, LA"
            a.group = counties ? (!attribute("STATE_NAME").empty() ? attribute("STATE_NAME") : abbreviation) : "States";
            // the rings: an outer ring turns clockwise in a shapefile, a hole the other way; they go together into one shape and the even-odd rule does the rest
            Polygon polygon;
            for (auto& part : f.parts) {
                polygon.push_back(std::move(part));
            }
            a.shapes.push_back(std::move(polygon));
            bounds(a);
            out.push_back(std::move(a));
        }
        std::sort(out.begin(), out.end(), [] (const Area& a, const Area& b) { return a.name < b.name; });
        return !out.empty();
    }

    Ring simplify(const Ring& ring, double tolerance) {
        if (ring.size() < 8) {
            return ring;
        }
        std::vector<bool> keep(ring.size(), false);
        keep.front() = keep.back() = true;
        std::vector<std::pair<size_t, size_t>> stack{{0, ring.size() - 1}};
        while (!stack.empty()) {
            const auto [a, b] = stack.back();
            stack.pop_back();
            double worst = 0.0;
            size_t at = 0;
            const auto& p = ring[a];
            const auto& q = ring[b];
            const double dx = q.first - p.first, dy = q.second - p.second, length = std::hypot(dx, dy);
            for (size_t i = a + 1; i < b; i++) {
                const double d = length < 1e-12 ? std::hypot(ring[i].first - p.first, ring[i].second - p.second)
                                                : std::abs(dy * (ring[i].first - p.first) - dx * (ring[i].second - p.second)) / length;
                if (d > worst) {
                    worst = d;
                    at = i;
                }
            }
            if (worst > tolerance) {
                keep[at] = true;
                stack.emplace_back(a, at);
                stack.emplace_back(at, b);
            }
        }
        Ring out;
        for (size_t i = 0; i < ring.size(); i++) {
            if (keep[i]) {
                out.push_back(ring[i]);
            }
        }
        return out.size() >= 4 ? out : ring;
    }

    bool parseWarningAreas(const std::string& zip, const std::function<std::string(const std::string&)>& name, std::vector<Area>& out, std::string& error) {
        std::map<std::string, std::string> files;
        UtilityZip::read(zip, files);
        std::string shp, dbf;
        for (const auto& [file, bytes] : files) {
            if (file.size() > 4 && file.compare(file.size() - 4, 4, ".shp") == 0) {
                shp = bytes;
            } else if (file.size() > 4 && file.compare(file.size() - 4, 4, ".dbf") == 0) {
                dbf = bytes;
            }
        }
        std::vector<UtilityShapefile::Feature> features;
        if (shp.empty() || dbf.empty() || !UtilityShapefile::parse(shp, dbf, features)) {
            error = "could not read the warning area file";
            return false;
        }
        std::map<std::string, Area> byOffice;   // an office's area may be several shapes (islands)
        for (auto& f : features) {
            if (f.shapeType != 5) {
                continue;
            }
            const auto code = f.attributes.count("CWA") ? f.attributes["CWA"] : f.attributes.count("WFO") ? f.attributes["WFO"] : std::string{};
            if (code.empty()) {
                continue;
            }
            auto& a = byOffice[code];
            a.id = code;
            auto label = name ? name(code) : std::string{};
            std::string alias;   // "Kansas City MO|KC": what the office is called, then what people call it for short
            if (const auto bar = label.find('|'); bar != std::string::npos) {
                alias = ", " + label.substr(bar + 1);
                label = label.substr(0, bar);
            }
            a.name = "NWS " + (label.empty() ? (f.attributes.count("CITYSTATE") ? f.attributes["CITYSTATE"] : code) : label) + " (" + code + alias + ")";
            a.group = "NWS forecast offices";
            Polygon polygon;
            for (auto& part : f.parts) {
                polygon.push_back(simplify(part, 0.006));
            }
            a.shapes.push_back(std::move(polygon));
        }
        for (auto& [code, a] : byOffice) {
            bounds(a);
            out.push_back(std::move(a));
        }
        std::sort(out.begin(), out.end(), [] (const Area& a, const Area& b) { return a.name < b.name; });
        return !out.empty();
    }

    std::string newestWarningAreaFile(const std::string& html) {
        static const char * months[] = {"ja", "fe", "mr", "ap", "my", "jn", "jl", "au", "sp", "oc", "no", "de"};
        std::string best;
        int bestKey = -1;
        const QRegularExpression re{"/(w_([0-9]{2})([a-z]{2})([0-9]{2})\\.zip)"};
        auto it = re.globalMatch(QString::fromStdString(html));
        while (it.hasNext()) {
            const auto m = it.next();
            int month = -1;
            for (int i = 0; i < 12; i++) {
                if (m.captured(3) == months[i]) {
                    month = i;
                }
            }
            const int key = m.captured(4).toInt() * 10000 + (month + 1) * 100 + m.captured(2).toInt();
            if (month >= 0 && key > bestKey) {
                bestKey = key;
                best = m.captured(1).toStdString();
            }
        }
        return best;
    }

    std::string serialize(const std::vector<Area>& areas) {
        std::string out;
        const auto put = [&out] (const auto& value) { out.append(reinterpret_cast<const char *>(&value), sizeof value); };
        const auto putString = [&] (const std::string& s) {
            put(static_cast<uint32_t>(s.size()));
            out += s;
        };
        put(static_cast<uint32_t>(areas.size()));
        for (const auto& a : areas) {
            putString(a.id);
            putString(a.name);
            putString(a.group);
            put(static_cast<uint32_t>(a.shapes.size()));
            for (const auto& polygon : a.shapes) {
                put(static_cast<uint32_t>(polygon.size()));
                for (const auto& ring : polygon) {
                    put(static_cast<uint32_t>(ring.size()));
                    for (const auto& [lon, lat] : ring) {
                        put(static_cast<float>(lon));
                        put(static_cast<float>(lat));
                    }
                }
            }
        }
        return out;
    }

    bool deserialize(const std::string& bytes, std::vector<Area>& out) {
        size_t at = 0;
        bool ok = true;
        const auto get = [&] (auto& value) {
            if (at + sizeof value > bytes.size()) {
                ok = false;
                return;
            }
            std::memcpy(&value, bytes.data() + at, sizeof value);
            at += sizeof value;
        };
        const auto getString = [&] () {
            uint32_t n = 0;
            get(n);
            if (!ok || at + n > bytes.size()) {
                ok = false;
                return std::string{};
            }
            std::string s = bytes.substr(at, n);
            at += n;
            return s;
        };
        uint32_t count = 0;
        get(count);
        for (uint32_t i = 0; ok && i < count; i++) {
            Area a;
            a.id = getString();
            a.name = getString();
            a.group = getString();
            uint32_t polygons = 0;
            get(polygons);
            for (uint32_t p = 0; ok && p < polygons; p++) {
                Polygon polygon;
                uint32_t rings = 0;
                get(rings);
                for (uint32_t r = 0; ok && r < rings; r++) {
                    Ring ring;
                    uint32_t points = 0;
                    get(points);
                    for (uint32_t k = 0; ok && k < points; k++) {
                        float lon = 0, lat = 0;
                        get(lon);
                        get(lat);
                        ring.emplace_back(lon, lat);
                    }
                    polygon.push_back(std::move(ring));
                }
                a.shapes.push_back(std::move(polygon));
            }
            bounds(a);
            out.push_back(std::move(a));
        }
        return ok && !out.empty();
    }

    std::vector<Area> parseSpcPolygons(const std::string& latLonList, const std::string& numbers, const std::string& idPrefix, const std::string& namePrefix, const std::string& group) {
        std::vector<Area> out;
        const auto polygons = QString::fromStdString(latLonList).split(':', Qt::SkipEmptyParts);
        const auto labels = QString::fromStdString(numbers).split(':', Qt::SkipEmptyParts);
        for (int i = 0; i < polygons.size(); i++) {
            Ring ring;
            const auto values = polygons[i].split(' ', Qt::SkipEmptyParts);
            for (int k = 0; k + 1 < values.size(); k += 2) {
                ring.emplace_back(values[k + 1].toDouble(), values[k].toDouble());
            }
            if (ring.size() < 3) {
                continue;
            }
            Area a;
            const auto number = i < labels.size() ? labels[i].toStdString() : std::to_string(i + 1);
            a.id = idPrefix + number;
            a.name = namePrefix + " " + number;
            a.group = group;
            a.shapes.push_back({ring});
            bounds(a);
            out.push_back(std::move(a));
        }
        return out;
    }

    Raster rasterize(const Monitor& monitor, double west, double south, double east, double north, double step) {
        Raster r;
        r.step = step;
        r.west = west;
        r.north = north;
        r.columns = std::max(1, static_cast<int>(std::ceil((east - west) / step)));
        r.rows = std::max(1, static_cast<int>(std::ceil((north - south) / step)));
        QImage image{r.columns, r.rows, QImage::Format_Grayscale8};
        image.fill(0);
        QPainter p{&image};   // no smoothing: a cell holds a category, not a blend of two
        p.setPen(Qt::NoPen);
        for (int c = 0; c < 5; c++) {   // the milder first: a worse category lies over them
            p.setBrush(QColor{(c + 1) * 40, (c + 1) * 40, (c + 1) * 40});
            p.drawPath(pathOf([&] {
                std::vector<Polygon> shapes;
                for (const auto& polygon : monitor.shapes[c]) {
                    shapes.push_back(polygon);
                }
                return shapes;
            }(), r));
        }
        p.end();
        r.category.resize(static_cast<size_t>(r.columns) * static_cast<size_t>(r.rows));
        for (int y = 0; y < r.rows; y++) {
            const auto * line = image.constScanLine(y);
            for (int x = 0; x < r.columns; x++) {
                r.category[static_cast<size_t>(y) * static_cast<size_t>(r.columns) + static_cast<size_t>(x)] = static_cast<uint8_t>((line[x] + 20) / 40);
            }
        }
        return r;
    }

    std::vector<uint8_t> mask(const Raster& like, const std::vector<Area>& areas) {
        QImage image{like.columns, like.rows, QImage::Format_Grayscale8};
        image.fill(0);
        QPainter p{&image};
        p.setPen(Qt::NoPen);
        p.setBrush(QColor{255, 255, 255});
        for (const auto& area : areas) {
            p.drawPath(pathOf(area.shapes, like));
        }
        p.end();
        std::vector<uint8_t> out(static_cast<size_t>(like.columns) * static_cast<size_t>(like.rows));
        for (int y = 0; y < like.rows; y++) {
            const auto * line = image.constScanLine(y);
            for (int x = 0; x < like.columns; x++) {
                out[static_cast<size_t>(y) * static_cast<size_t>(like.columns) + static_cast<size_t>(x)] = line[x] > 127 ? 1 : 0;
            }
        }
        return out;
    }

    Share share(const Raster& r, const std::vector<uint8_t>& mask) {
        Share s;
        double total = 0.0, none = 0.0, atLeast[5]{};
        for (int y = 0; y < r.rows; y++) {
            const double weight = std::cos((r.north - (y + 0.5) * r.step) * M_PI / 180.0);
            for (int x = 0; x < r.columns; x++) {
                const size_t i = static_cast<size_t>(y) * static_cast<size_t>(r.columns) + static_cast<size_t>(x);
                if (!mask[i]) {
                    continue;
                }
                total += weight;
                const int c = r.category[i];
                if (c == 0) {
                    none += weight;
                }
                for (int k = 0; k < c; k++) {
                    atLeast[k] += weight;
                }
            }
        }
        if (total > 0.0) {
            s.none = none / total * 100.0;
            for (int k = 0; k < 5; k++) {
                s.atLeast[k] = atLeast[k] / total * 100.0;
            }
        }
        return s;
    }

    std::vector<int8_t> change(const Raster& a, const Raster& b) {
        std::vector<int8_t> out;
        if (a.columns != b.columns || a.rows != b.rows) {
            return out;
        }
        out.resize(a.category.size());
        for (size_t i = 0; i < out.size(); i++) {
            out[i] = static_cast<int8_t>(static_cast<int>(b.category[i]) - static_cast<int>(a.category[i]));
        }
        return out;
    }
}
