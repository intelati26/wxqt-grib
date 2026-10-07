// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "hurricane/UtilityNhcGis.h"
#include <cstdlib>
#include <map>
#include <regex>
#include <sstream>
#include "hurricane/UtilityShapefile.h"
#include "util/UtilityZip.h"

namespace {
    // the file of a layer in the zip: any name that ends with the suffix (the names carry the storm, advisory and time)
    bool layer(const std::map<string, string>& files, const string& suffix, string& shp, string& dbf) {
        for (const auto& [name, data] : files) {
            if (name.size() > suffix.size() + 4 && name.compare(name.size() - suffix.size() - 4, suffix.size() + 4, suffix + ".shp") == 0) {
                shp = data;
                const auto d = files.find(name.substr(0, name.size() - 3) + "dbf");
                dbf = d == files.end() ? string{} : d->second;
                return true;
            }
        }
        return false;
    }

    double number(const std::map<string, string>& a, const char * key, double fallback = -1.0) {
        const auto found = a.find(key);
        return found == a.end() || found->second.empty() ? fallback : std::strtod(found->second.c_str(), nullptr);
    }

    string text(const std::map<string, string>& a, const char * key) {
        const auto found = a.find(key);
        return found == a.end() ? string{} : found->second;
    }
}

UtilityNhcGis::Cone UtilityNhcGis::parseCone(const string& zip) {
    Cone cone;
    std::map<string, string> files;
    if (!UtilityZip::read(zip, files)) {
        return cone;
    }
    string shp;
    string dbf;
    vector<UtilityShapefile::Feature> features;
    if (layer(files, "_5day_pgn", shp, dbf) && UtilityShapefile::parse(shp, dbf, features)) {
        for (const auto& f : features) {
            for (const auto& part : f.parts) {
                cone.polygons.push_back(part);
            }
            if (cone.stormName.empty()) {
                cone.stormName = text(f.attributes, "STORMNAME");
                cone.advisory = text(f.attributes, "ADVISNUM");
                cone.advisoryDate = text(f.attributes, "ADVDATE");
            }
        }
        cone.ok = !cone.polygons.empty();
    }
    if (layer(files, "_5day_lin", shp, dbf) && UtilityShapefile::parse(shp, dbf, features)) {
        for (const auto& f : features) {
            for (const auto& part : f.parts) {
                cone.lines.push_back(part);
            }
        }
    }
    if (layer(files, "_5day_pts", shp, dbf) && UtilityShapefile::parse(shp, dbf, features)) {
        for (const auto& f : features) {
            if (f.parts.empty() || f.parts[0].empty()) {
                continue;
            }
            Point p;
            p.lon = f.parts[0][0].first;
            p.lat = f.parts[0][0].second;
            p.tau = static_cast<int>(number(f.attributes, "TAU", 0));
            p.maxWind = number(f.attributes, "MAXWIND");
            p.gust = number(f.attributes, "GUST");
            p.pressure = number(f.attributes, "MSLP");
            p.label = text(f.attributes, "DATELBL");
            p.development = text(f.attributes, "TCDVLP");
            p.validTime = text(f.attributes, "VALIDTIME");
            cone.points.push_back(p);
        }
    }
    return cone;
}

vector<UtilityNhcGis::WindRadius> UtilityNhcGis::parseRadii(const string& zip) {
    vector<WindRadius> radii;
    std::map<string, string> files;
    if (!UtilityZip::read(zip, files)) {
        return radii;
    }
    for (const char * suffix : {"initialradii", "forecastradii"}) {
        string shp;
        string dbf;
        vector<UtilityShapefile::Feature> features;
        if (layer(files, suffix, shp, dbf) && UtilityShapefile::parse(shp, dbf, features)) {
            for (const auto& f : features) {
                WindRadius r;
                r.knots = static_cast<int>(number(f.attributes, "RADII", 0));
                r.tau = static_cast<int>(number(f.attributes, "TAU", 0));
                r.ne = number(f.attributes, "NE", 0);
                r.se = number(f.attributes, "SE", 0);
                r.sw = number(f.attributes, "SW", 0);
                r.nw = number(f.attributes, "NW", 0);
                radii.push_back(r);
            }
        }
    }
    return radii;
}

string UtilityNhcGis::nameFor(const string& code) {
    static const std::map<string, string> names{{"HWR", "Hurricane Warning"}, {"HWA", "Hurricane Watch"}, {"TWR", "Tropical Storm Warning"}, {"TWA", "Tropical Storm Watch"},
                                                {"SSW", "Storm Surge Warning"}, {"SSA", "Storm Surge Watch"}};
    const auto found = names.find(code);
    return found == names.end() ? code : found->second;
}

string UtilityNhcGis::colorFor(const string& code) {
    // NHC's own convention: hurricane warning red, hurricane watch pink, tropical storm warning blue, tropical storm watch yellow
    if (code == "HWR") return "#ff0000";
    if (code == "HWA") return "#ff80c0";
    if (code == "TWR") return "#0055ff";
    if (code == "TWA") return "#ffd700";
    if (code == "SSW") return "#b000ff";
    if (code == "SSA") return "#ff9000";
    return "#ffffff";
}

vector<UtilityNhcGis::WatchWarning> UtilityNhcGis::parseKml(const string& kml) {
    vector<WatchWarning> out;
    static const std::regex placemark{R"(<Placemark>([\s\S]*?)</Placemark>)"};
    static const std::regex nameRe{R"(<name>([^<]*)</name>)"};
    static const std::regex style{R"(<styleUrl>#?([A-Za-z]+)</styleUrl>)"};
    static const std::regex coordinates{R"(<coordinates>([\s\S]*?)</coordinates>)"};
    for (std::sregex_iterator it{kml.begin(), kml.end(), placemark}, end; it != end; ++it) {
        const string body = (*it)[1];
        std::smatch m;
        WatchWarning w;
        if (std::regex_search(body, m, nameRe)) {
            w.kind = m[1];
        }
        if (std::regex_search(body, m, style)) {
            w.code = m[1];
        }
        for (std::sregex_iterator c{body.begin(), body.end(), coordinates}, cend; c != cend; ++c) {
            Ring ring;
            std::istringstream stream{(*c)[1].str()};
            string token;
            while (stream >> token) {
                // lon,lat,altitude
                const auto first = token.find(',');
                if (first == string::npos) {
                    continue;
                }
                const auto second = token.find(',', first + 1);
                ring.emplace_back(std::strtod(token.substr(0, first).c_str(), nullptr),
                                  std::strtod(token.substr(first + 1, second == string::npos ? string::npos : second - first - 1).c_str(), nullptr));
            }
            if (ring.size() >= 2) {
                w.lines.push_back(std::move(ring));
            }
        }
        if (!w.lines.empty()) {
            out.push_back(std::move(w));
        }
    }
    return out;
}

vector<UtilityNhcGis::WatchWarning> UtilityNhcGis::parseWatchWarnings(const string& data) {
    if (data.size() > 4 && data.compare(0, 2, "PK") == 0) {
        std::map<string, string> files;
        if (!UtilityZip::read(data, files)) {
            return {};
        }
        for (const auto& [name, contents] : files) {
            if (name.size() > 4 && name.compare(name.size() - 4, 4, ".kml") == 0) {
                return parseKml(contents);
            }
        }
        return {};
    }
    return parseKml(data);
}
