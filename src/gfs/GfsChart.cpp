// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "gfs/GfsChart.h"
#include "gfs/GfsChartInternal.h"
#include <limits>
#include <QDate>
#include <algorithm>
#include <regex>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <QDateTime>
#include <QFontMetricsF>
#include <QPainter>
#include <QRegularExpression>
#include <QPainterPath>
#include <QPolygonF>
#include <QStringList>
#include <QTimeZone>
#include <set>
#include <tuple>
#include "cfs/UtilityCfs.h"
#include "gfs/AirNowLoader.h"
#include "gfs/GfsClimate.h"
#include "gfs/GfsNormals.h"
#include "gfs/GfsModels.h"
#include "spcrefs/UtilitySpcRefs.h"
#include "gfs/BarbGrid.h"
#include "ui/WindBarb.h"

namespace GfsChart {
using namespace detail;
namespace {



    double mercatorY(double lat) {
        const double clamped = std::clamp(lat, -82.0, 82.0) * pi / 180.0;
        return std::log(std::tan(pi / 4.0 + clamped / 2.0));
    }
    double mercatorLat(double y) {
        return (2.0 * std::atan(std::exp(y)) - pi / 2.0) * 180.0 / pi;
    }

    // The view: longitude is linear across the width, latitude is Mercator down the height.
    struct View {
        const Sector& sector;
        QRectF area;
        double yTop, yBottom;
        View(const Sector& s, const QRectF& a) : sector{s}, area{a}, yTop{mercatorY(s.north)}, yBottom{mercatorY(s.south)} {}
        QPointF toPixel(double lon, double lat) const {
            double dx = lon - sector.west;
            while (dx < -180.0) {   // only a point a half turn out is moved: a hair beyond the edge stays beyond it
                dx += 360.0;
            }
            while (dx > 540.0) {
                dx -= 360.0;
            }
            return {area.left() + dx / (sector.east - sector.west) * area.width(), area.top() + (yTop - mercatorY(lat)) / (yTop - yBottom) * area.height()};
        }
        double lonAt(double x) const { return sector.west + (x - area.left()) / area.width() * (sector.east - sector.west); }
        double latAt(double y) const { return mercatorLat(yTop - (y - area.top()) / area.height() * (yTop - yBottom)); }
        // the height the area needs so that a degree is as long both ways at the center of the view
        static double heightFor(const Sector& s, double width) {
            return width * (mercatorY(s.north) - mercatorY(s.south)) / ((s.east - s.west) * pi / 180.0);
        }
    };


}

QRgb Ramp::at(double value) const {
    if (stops.empty() || std::isnan(value)) {
        return qRgba(0, 0, 0, 0);
    }
    if (banded) {
        if (value < stops.front().first) {
            return qRgba(0, 0, 0, 0);
        }
        size_t at = 0;
        for (size_t i = 1; i < stops.size() && stops[i].first <= value; i++) {
            at = i;
        }
        return stops[at].second.rgba();
    }
    if (value <= stops.front().first) {
        return stops.front().second.rgba();
    }
    for (size_t i = 1; i < stops.size(); i++) {
        if (value <= stops[i].first) {
            const double t = (value - stops[i - 1].first) / (stops[i].first - stops[i - 1].first);
            const auto a = stops[i - 1].second, b = stops[i].second;
            return qRgba(static_cast<int>(a.red() + (b.red() - a.red()) * t), static_cast<int>(a.green() + (b.green() - a.green()) * t),
                         static_cast<int>(a.blue() + (b.blue() - a.blue()) * t), static_cast<int>(a.alpha() + (b.alpha() - a.alpha()) * t));
        }
    }
    return stops.back().second.rgba();
}

const std::vector<Sector>& sectors() {
    static const std::vector<Sector> list{
        {"CONUS", -127, 22, -65, 51},
        {"NAMER", -170, 5, -50, 72},
        {"SAMER", -95, -58, -30, 17},
        {"AFRICA", -25, -38, 62, 40},
        {"NORTH-PAC", 110, 0, 250, 62},
        {"EAST-PAC", -160, 0, -75, 50},
        {"WEST-ATL", -100, 5, -40, 50},
        {"ATLANTIC", -100, -5, 20, 65},
        {"POLAR", -180, 55, 180, 82},
        {"ALASKA", -175, 50, -125, 72},
        {"EUROPE", -25, 33, 50, 72},
        {"ASIA", 55, 0, 150, 60},
        {"SOUTH-PAC", 140, -60, 290, 5},
        {"ARTIC", -180, 60, 180, 82},
        {"INDIA", 50, -10, 110, 40},
        {"US-SAMOA", -190, -30, -140, 5},
        // the world
        {"GLOBAL", -180, -75, 180, 75},
        {"TROPICS", -180, -38, 180, 38},
        {"NORTHERN-HEMI", -180, 15, 180, 82},
        {"SOUTHERN-HEMI", -180, -82, 180, -15},
        {"AUSTRALIA", 105, -50, 180, -5},
        {"MIDDLE-EAST", 25, 8, 72, 45},
        {"INDIAN-OCEAN", 30, -45, 125, 28},
        {"E-ASIA", 95, 15, 150, 56},
        {"SE-ASIA", 88, -12, 150, 30},
        {"NORTH-ATL", -85, 10, 20, 68},
        {"CARIBBEAN", -92, 6, -56, 29},
        {"GULF-MEXICO", -100, 17, -78, 32},
        {"CENT-AMER", -95, 5, -70, 22},
        {"HAWAII", -165, 15, -150, 26},
        {"HAWAII-ISLANDS", -161.8, 18.0, -153.8, 23.2},   // the islands, as the Hawaii analyses cover them
        {"PUERTO-RICO", -68.4, 16.7, -64.0, 19.9},
        {"GUAM", 143.6, 12.3, 148.4, 16.9},   // Guam and the southern Marianas, as the analysis covers them       // Puerto Rico and the Virgin Islands, as the analyses cover them
        // the United States by region
        {"NORTHEAST", -83, 36, -66, 48},
        {"MID-ATLANTIC", -86, 34, -72, 42},
        {"SOUTHEAST", -93, 24, -74, 38},
        {"GREAT-LAKES", -96, 40, -74, 50},
        {"OHIO-VALLEY", -92, 34, -78, 43},
        {"S-PLAINS", -107, 25, -90, 40},
        {"C-PLAINS", -106, 34, -90, 44},
        {"N-PLAINS", -113, 39, -94, 50},
        {"ROCKIES", -118, 35, -101, 50},
        {"SOUTHWEST", -122, 30, -102, 42},
        {"PACIFIC-NW", -128, 41, -113, 52},
        {"CALIFORNIA", -126, 31, -112, 43},
        {"GULF-COAST", -100, 24, -80, 35}};
    return list;
}

Sector gridSector(const std::string& id, const GfsGrid::Grid& g) {
    int left = g.columns, right = -1, top = g.rows, bottom = -1;
    for (int y = 0; y < g.rows; y++) {
        for (int x = 0; x < g.columns; x++) {
            if (!std::isnan(g.values[static_cast<size_t>(y) * static_cast<size_t>(g.columns) + static_cast<size_t>(x)])) {
                left = std::min(left, x);
                right = std::max(right, x);
                top = std::min(top, y);
                bottom = std::max(bottom, y);
            }
        }
    }
    if (right < 0) {   // nothing valid: the whole grid
        left = 0;
        right = g.columns - 1;
        top = 0;
        bottom = g.rows - 1;
    }
    const double margin = g.step / 2.0;   // half a cell: the data and nothing more (the tilted footprint leaves wedges in the corners)
    return {id, g.lon0 + left * g.step - margin, g.lat0 - bottom * g.step - margin, g.lon0 + right * g.step + margin, g.lat0 - top * g.step + margin};
}

Sector cropToTrack(const Sector& within, const std::vector<TrackPoint>& track, int hour, double margin) {
    double west = 1e9, east = -1e9, south = 1e9, north = -1e9;
    for (const auto& t : track) {
        if (t.hour <= hour) {
            west = std::min(west, t.lon);
            east = std::max(east, t.lon);
            south = std::min(south, t.lat);
            north = std::max(north, t.lat);
        }
    }
    if (east < west) {
        return within;
    }
    Sector s{within.id, std::max(within.west, west - margin), std::max(within.south, south - margin), std::min(within.east, east + margin), std::min(within.north, north + margin)};
    const double minimum = 8.0;   // never narrower than this, in either direction
    if (s.east - s.west < minimum) {
        const double mid = (s.east + s.west) / 2.0;
        s.west = std::max(within.west, mid - minimum / 2.0);
        s.east = std::min(within.east, mid + minimum / 2.0);
    }
    if (s.north - s.south < minimum) {
        const double mid = (s.north + s.south) / 2.0;
        s.south = std::max(within.south, mid - minimum / 2.0);
        s.north = std::min(within.north, mid + minimum / 2.0);
    }
    return s;
}

std::vector<TrackPoint> parseTrack(const std::string& text) {
    std::vector<TrackPoint> points;
    const auto position = [] (const std::string& s, char negative) {   // "235N" -> 23.5, "906W" -> -90.6
        if (s.size() < 2) {
            return 0.0;
        }
        const double value = std::atof(s.c_str()) / 10.0;
        return s.back() == negative ? -value : value;
    };
    size_t from = 0;
    while (from < text.size()) {
        auto end = text.find('\n', from);
        if (end == std::string::npos) {
            end = text.size();
        }
        const auto line = text.substr(from, end - from);
        from = end + 1;
        std::vector<std::string> f;
        size_t at = 0;
        while (true) {
            const auto comma = line.find(',', at);
            std::string field = line.substr(at, comma == std::string::npos ? std::string::npos : comma - at);
            while (!field.empty() && field.front() == ' ') {
                field.erase(field.begin());
            }
            while (!field.empty() && field.back() == ' ') {
                field.pop_back();
            }
            f.push_back(std::move(field));
            if (comma == std::string::npos) {
                break;
            }
            at = comma + 1;
        }
        if (f.size() < 17) {
            continue;
        }
        const int hour = std::atoi(f[5].c_str());
        if (points.empty() || points.back().hour != hour) {
            TrackPoint p;
            p.hour = hour;
            p.lat = position(f[6], 'S');
            p.lon = position(f[7], 'W');
            p.wind = std::atoi(f[8].c_str());
            p.pressure = std::atoi(f[9].c_str());
            points.push_back(p);
        }
        const int threshold = std::atoi(f[11].c_str());
        const int row = threshold == 34 ? 0 : threshold == 50 ? 1 : threshold == 64 ? 2 : -1;
        if (row >= 0) {
            for (int q = 0; q < 4; q++) {
                points.back().radii[row][q] = std::atof(f[static_cast<size_t>(13 + q)].c_str());
            }
        }
    }
    return points;
}

std::string localId(double lat, double lon, double km, const std::string& name) {
    std::string clean = name;
    for (auto& c : clean) {
        if (c == ',' || c == '|' || c == ':') {
            c = ' ';
        }
    }
    char text[64];
    std::snprintf(text, sizeof text, "%.3f,%.3f,%d,", lat, lon, static_cast<int>(std::lround(km)));
    return std::string{"LOCAL:"} + text + clean;
}

bool parseLocalId(const std::string& id, double& lat, double& lon, double& km, std::string& name) {
    if (id.compare(0, 6, "LOCAL:") != 0) {
        return false;
    }
    const auto parts = QString::fromStdString(id.substr(6)).split(',');
    if (parts.size() < 3) {
        return false;
    }
    bool a = false, b = false, c = false;
    lat = parts[0].toDouble(&a);
    lon = parts[1].toDouble(&b);
    km = parts[2].toDouble(&c);
    name = parts.size() > 3 ? parts.mid(3).join(',').toStdString() : std::string{};
    return a && b && c && km > 10.0 && std::fabs(lat) <= 90.0 && std::fabs(lon) <= 360.0;
}

std::string boxId(double west, double south, double east, double north) {
    char text[96];
    std::snprintf(text, sizeof text, "BOX:%.3f,%.3f,%.3f,%.3f", west, south, east, north);
    return text;
}

bool parseBoxId(const std::string& id, Sector& out) {
    if (id.compare(0, 4, "BOX:") != 0) {
        return false;
    }
    const auto parts = QString::fromStdString(id.substr(4)).split(',');
    if (parts.size() != 4) {
        return false;
    }
    bool ok[4];
    const double west = parts[0].toDouble(&ok[0]), south = parts[1].toDouble(&ok[1]), east = parts[2].toDouble(&ok[2]), north = parts[3].toDouble(&ok[3]);
    if (!ok[0] || !ok[1] || !ok[2] || !ok[3] || !(east - west > 0.2) || !(north - south > 0.2) || west < -180.0 || east > 180.0 || south < -85.0 || north > 85.0) {
        return false;
    }
    out = Sector{id, west, south, east, north};
    return true;
}

Sector localSector(const std::string& id, double west, double south, double east, double north) {
    double lat = 0, lon = 0, km = 0;
    std::string name;
    Sector out{id, west, south, east, north};
    if (!parseLocalId(id, lat, lon, km, name)) {
        return out;
    }
    const double halfLat = km / 111.32 / 2.0;
    const double halfLon = km / (111.32 * std::max(std::cos(lat * pi / 180.0), 0.2)) / 2.0;
    // a window of that size round the place, moved inside the coverage (not narrowed: the same kilometers wherever it is)
    const auto place = [] (double center, double half, double low, double high, double& from, double& to) {
        if (2.0 * half >= high - low) {
            from = low;
            to = high;
            return;
        }
        from = center - half;
        to = center + half;
        if (from < low) {
            to += low - from;
            from = low;
        }
        if (to > high) {
            from -= to - high;
            to = high;
        }
    };
    place(lat, halfLat, std::max(south, -85.0), std::min(north, 85.0), out.south, out.north);
    place(lon, halfLon, west, east, out.west, out.east);
    return out;
}

const Sector * sector(const std::string& id) {
    for (const auto& s : sectors()) {
        if (s.id == id) {
            return &s;
        }
    }
    return nullptr;
}

const std::vector<Product>& products() {
    static const std::vector<Product> list = [] {
        std::vector<Product> p;

        std::map<std::string, Product> plainPrecip;   // the precipitation charts before the lines are put on them: for the models whose maps have none
        gfsProducts(p, plainPrecip);
        nbmProducts(p);
        const auto cloneFromGfs = [&] (const GfsModels::Def& def) { cloneFrom(p, def, plainPrecip); };

        // ---- GEFS: the ensemble mean drawn like the GFS (every chart whose fields the mean files hold: they have relative humidity, and precipitation in 6 hour pieces like the AI model),
        // and the spread (the standard deviation of the 30 members) of the fields that matter, filled under the mean's lines
        const auto gefsRecipes = [&] { gefsProducts(p); };

        // ---- RRFS: the 3 km Rapid Refresh Forecast System (NAM, HRRR, RAP and the high resolution windows all end up here). Every GFS chart whose fields it holds is made again from it (sea level
        // pressure is its MSLET; precipitation has running totals like the GFS), then the charts only a convection-allowing model has: radar, helicity, updraft helicity, echo tops, ceiling,
        // visibility, lightning, gusts and CAPE with CIN.
        const auto rrfsRecipes = [&] (const std::string& model) { rrfsProducts(p, model); };

        // ---- REFS: the 5 member 3 km ensemble. The ready-made products (the ensemble mean of precipitation, the probabilities of precipitation and snow, flash flood risk) are charts of their own;
        // the members make the rest: probabilities of strong echoes and rotating updrafts (the fraction of members, neighborhood averaged), the strongest rotation, a paintball of each
        // member's strong echoes, and the mean and spread of the 2 m temperature.
        const auto refsRecipes = [&] { refsProducts(p); };

        // ---- the wave models (GFS-Wave and the GEFS-Wave ensemble mean): each wave component (all of the sea, the wind sea, the three swells) as a height map with the wind, and as the period
        // with the direction the waves come from drawn as streamlines
        const auto sstRecipes = [&] (const char * model) { sstProducts(p, model); };
        const auto camsRecipes = [&] (const char * model) { camsProducts(p, model); };
        const auto mfWaveRecipes = [&] (const char * model) { mfWaveProducts(p, model); };
        const auto currentRecipes = [&] (const char * model) { currentProducts(p, model); };
        const auto waveRecipes = [&] (const char * model) { waveProducts(p, model); };

        // ---- HREF: the 10 member ensemble's own ready-made files (the mean, the probability-matched mean, the localized one, the blend, the probabilities and the agreement-scale probabilities).
        // Every one of the model guidance site's 71 HREF maps is a row below: which file, which field, over what time, which threshold, and how it is colored.
        const auto hrefRecipes = [&] { hrefProducts(p); };

        // ---- HAFS: the hurricane model for one storm (the screen picks the storm): wind, simulated radar and satellite, rain, sea surface temperature, shear, waves
        const auto hafsRecipes = [&] (const char * model) { hafsProducts(p, model); };

        // ---- GEPS (Canada): the clone of the GFS charts is the 21 members' mean; its own are the spread of the members and the chance of a temperature either side of a limit
        const auto gepsRecipes = [&] { gepsProducts(p); };

        // ---- SPC's post-processing (SPC_POST v3, effective Nov 3 2026; in the test area before): probabilities in per cent, one product per file, a variable per message
        const auto spcRecipes = [&] (const char * source, bool gefs) { spcProducts(p, source, gefs); };

        // ---- the mesoanalysis fields (SFCOA-HRRR, and the RRFS's RRFSD): the NetCDF variable names and units are the file's
        const auto mesoRecipes = [&] (const char * source) { mesoProducts(p, source); };

        // ---- SPC's REFS products (the open Zarr store): the curated list of SPC's own viewer, each as the colors and units it has there
        // ---- the ECMWF's extended range forecast (EC46): the weekly mean of the ensemble, from the loader of Open-Meteo's seasonal data; the "hour" of a chart is a week (168 hours each)
        const auto openMeteoRecipes = [&] (const std::string& source) { openMeteoProducts(p, source); };

        // ---- the CFSv2's next 30 days against its normal (the loader of this source reads the CFS): temperature in colors with the precipitation as lines, and the precipitation itself
        // The Weather Prediction Center's probabilistic precipitation portal: for each period (24, 36, 48, 72 hours) and each of rain, snow and freezing rain, the expected amount, the amount at a percentile of the blend's
        // members and the chance of an amount (the loader of WpcPppLoader.cpp makes the grid; the want's variable is "<hours>|<element>|<kind>")
        const auto pppRecipes = [&] { pppProducts(p); };

        const auto cfs30Recipes = [&] { cfs30Products(p); };

        const auto spcRefsRecipes = [&] { spcRefsProducts(p); };

        // ---- IFS-ENS (ECMWF's ensemble, 50 members): the spread of a field with its mean as lines, a spaghetti of one contour from every member, and a single member by the member picker
        const auto ensRecipes = [&] { ensProducts(p); };

        // ---- ENS-ALL: every member of the ECMWF ensemble (50), the GEFS (31) and the GEPS (21) pooled as 102 members of one ensemble on a 0.5 degree grid, for the same valid time
        const auto ensAllRecipes = [&] { ensAllProducts(p); };

        // ---- GEOS-FP (NASA GMAO): the aerosol fields the model is used for, from its 3 hourly aerosol collection (kg/m3, drawn in micrograms; dust has a chart of its own in several forms)
        const auto geosRecipes = [&] { geosProducts(p); };

        // The models in the registry's order: each one's clone of the GFS charts, then the charts of its own
        const std::map<std::string, std::function<void()>> own{{"GEFS", gefsRecipes}, {"GEPS", gepsRecipes}, {"SPCPOST", [&] { spcRecipes("SPCPOST", false); }}, {"SPCGEFS", [&] { spcRecipes("SPCGEFS", true); }}, {"SFCOA", [&] { mesoRecipes("SFCOA"); }}, {"SPCREFS", spcRefsRecipes}, {"CFS30", cfs30Recipes}, {"EC46", [&] { openMeteoRecipes("EC46"); }}, {"SEAS5", [&] { openMeteoRecipes("SEAS5"); }}, {"WPC-PPP", pppRecipes}, {"RRFSD", [&] { mesoRecipes("RRFSD"); }}, {"RRFS", [&] { rrfsRecipes("RRFS"); }}, {"HRRR", [&] { rrfsRecipes("HRRR"); }}, {"HRRR-AK", [&] { rrfsRecipes("HRRR-AK"); }}, {"MPAS-HT", [&] { rrfsRecipes("MPAS-HT"); }}, {"MPAS-RN", [&] { rrfsRecipes("MPAS-RN"); }}, {"RRFS-AK", [&] { rrfsRecipes("RRFS-AK"); }}, {"RRFS-FIREWX", [&] { rrfsRecipes("RRFS-FIREWX"); }}, {"RRFS-HI", [&] { rrfsRecipes("RRFS-HI"); }}, {"RRFS-PR", [&] { rrfsRecipes("RRFS-PR"); }}, {"RRFS-NA", [&] { rrfsRecipes("RRFS-NA"); }}, {"GEOS-FP", geosRecipes}, {"IFS-ENS", ensRecipes}, {"ENS-ALL", ensAllRecipes}, {"REFS", refsRecipes}, {"HREF", hrefRecipes}, {"MF-SST", [&] { sstRecipes("MF-SST"); }}, {"MF-WAVE", [&] { mfWaveRecipes("MF-WAVE"); }}, {"MF-CURRENT", [&] { currentRecipes("MF-CURRENT"); }}, {"COPERNICUS", [&] { camsRecipes("COPERNICUS"); }}, {"GFS-WAVE", [&] { waveRecipes("GFS-WAVE"); }}, {"GEFS-WAVE", [&] { waveRecipes("GEFS-WAVE"); }}, {"HAFSA", [&] { hafsRecipes("HAFSA"); }}, {"HAFSB", [&] { hafsRecipes("HAFSB"); }}};
        for (const auto& def : GfsModels::all()) {
            if (def.clone.enabled) {
                cloneFromGfs(def);
            }
            const auto found = own.find(def.id);
            if (found != own.end()) {
                found->second();
            }
        }
        for (auto& chart : p) {   // the NBM's hourly runs (01Z, 13Z ...) carry the 1 hour fields only; the 12 hour extremes are in the 00 and 12Z runs, the 6 and 12 hour chances in 00, 06, 12 and 18Z
            if (chart.source == "NBM") {
                if (chart.id == "2m_min_temp" || chart.id == "2m_max_temp") {
                    chart.mainCycleStep = 12;
                    chart.periodHours = 12;
                    chart.periodEndHourUtc = chart.id == "2m_min_temp" ? 12 : 0;
                } else if (chart.id == "tstm_prob" || chart.id == "6hour_precip_chance" || chart.id == "12hour_precip_chance") {
                    chart.mainCycleStep = 6;
                    chart.periodHours = chart.id == "12hour_precip_chance" ? 12 : 6;
                }
            }
        }
        return p;
    }();
    return list;
}

const std::vector<PoolMember>& poolMembers() {
    static const std::vector<PoolMember> list = [] {
        std::vector<PoolMember> out;
        const auto two = [] (int m) { return (m < 10 ? "0" : "") + std::to_string(m); };
        for (int m = 1; m <= 50; m++) {
            out.push_back({"IFS-ENS", "e" + two(m), "I" + two(m)});
        }
        out.push_back({"GEFS", "c00", "G00"});
        for (int m = 1; m <= 30; m++) {
            out.push_back({"GEFS", "p" + two(m), "G" + two(m)});
        }
        out.push_back({"GEPS", "c00", "C00"});
        for (int m = 1; m <= 20; m++) {
            out.push_back({"GEPS", "p" + two(m), "C" + two(m)});
        }
        return out;
    }();
    return list;
}

const Product * product(const std::string& id, const std::string& source) {
    for (const auto& p : products()) {
        if (p.id == id && p.source == source) {
            return &p;
        }
    }
    if (id.compare(0, 4, "gen~") == 0) {   // a chart made on request: built from its template and kept
        static std::mutex mutex;
        static std::map<std::string, Product> made;
        std::lock_guard lock{mutex};
        const auto key = source + "|" + id;
        if (const auto found = made.find(key); found != made.end()) {
            return &found->second;
        }
        std::vector<std::string> parts;
        for (const auto& part : QString::fromStdString(id).split('~')) {
            parts.push_back(part.toStdString());
        }
        const auto generator = parts.size() >= 3 ? generators().find(parts[1]) : generators().end();
        if (generator == generators().end()) {
            return nullptr;
        }
        std::vector<double> values;
        for (size_t i = 2; i < parts.size(); i++) {
            values.push_back(QString::fromStdString(parts[i]).toDouble());
        }
        try {
            auto product = generator->second(values);
            if (product.source != source || product.id != id) {   // the id spells the values the way the template does: another spelling is not that chart
                return nullptr;
            }
            return &(made[key] = std::move(product));
        } catch (const std::exception&) {
            return nullptr;
        }
    }
    return nullptr;
}

std::vector<Template> templates(const std::string& source) {
    const auto ingredientTemplate = [] (const std::string& id, const std::string& title, bool ensemble, const std::string& name) {
        const auto conditions = ingredientConditions(ensemble, name);
        std::vector<double> indices;
        std::vector<std::string> names;
        size_t first = 0, second = 0, third = 0;
        for (size_t i = 0; i < conditions.size(); i++) {
            indices.push_back(static_cast<double>(i));
            names.push_back(conditionText(conditions[i]));
            const std::string ing = ingredients()[conditions[i].ingredient].name;
            if (ing == "MUCAPE" && conditions[i].limit == 500) first = i;
            if (ing == "Sfc-500 mb shear" && conditions[i].limit == 40) second = i;
            if ((ing == "6-h rain" || ing == "Rain rate") && conditions[i].limit == 0.05) third = i;
        }
        auto optional = indices;
        auto optionalNames = names;
        optional.insert(optional.begin(), -1.0);
        optionalNames.insert(optionalNames.begin(), "(nothing more)");
        return Template{id, title, {{"a", "Ingredient 1", "", indices, static_cast<double>(first), names}, {"b", "and ingredient 2", "", optional, static_cast<double>(second), optionalNames},
                                    {"c", "and ingredient 3", "", optional, static_cast<double>(third), optionalNames}}};
    };
    if (std::find(ingredientSources().begin(), ingredientSources().end(), source) != ingredientSources().end()) {
        return {ingredientTemplate("ingr_" + source, "Ingredients met together (CAPE, shear, moisture, rain: where each holds and how many do)", false, source)};
    }
    if (source == "IFS-ENS") {   // the ECMWF ensemble: the chance of an amount of precipitation, in total or in a period
        const std::vector<double> amounts{0.01, 0.05, 0.1, 0.25, 0.5, 0.75, 1, 1.5, 2, 3, 4, 6};
        return {{"ens_total", "Chance of precipitation of an amount or more in total (since the start of the run)", {{"limit", "Total of", "in", amounts, 0.01}}},
                {"ens_period", "Chance of precipitation of an amount or more in a period", {{"limit", "Amount of", "in", amounts, 0.5}, {"hours", "in", "hours", {3, 6, 12, 24, 36, 48, 72, 96, 120, 168}, 24}}}};
    }
    {   // the same two templates for each of the other ensembles that cannot count on a template of its own: the GEPS (running totals) and all the ensembles pooled
        const std::vector<double> amounts{0.01, 0.05, 0.1, 0.25, 0.5, 0.75, 1, 1.5, 2, 3, 4, 6};
        if (source == "GEPS") {
            return {{"geps_total", "Chance of precipitation of an amount or more in total (since the start of the run)", {{"limit", "Total of", "in", amounts, 0.01}}},
                    {"geps_period", "Chance of precipitation of an amount or more in a period", {{"limit", "Amount of", "in", amounts, 0.5}, {"hours", "in", "hours", {3, 6, 12, 24, 36, 48, 72, 96, 120, 168}, 24}}}};
        }
        if (source == "ENS-ALL") {
            return {{"all_period", "Chance of precipitation of an amount or more in a period: all ensembles (ECMWF, GEFS, GEPS)", {{"limit", "Amount of", "in", amounts, 0.5}, {"hours", "in", "hours", {6, 12, 24, 48, 72, 96}, 24}}}};
        }
    }
    if (source != "GEFS") {
        return {};
    }
    const auto one = [] (const char * id, const char * title, const char * label, const char * unit, std::vector<double> choices, double standard) {
        return Template{id, title, {{"limit", label, unit, std::move(choices), standard}}};
    };
    return {
        ingredientTemplate("ingredients", "Chance of ingredients together (CAPE, shear, moisture, rain: the share of the members that have all)", true, "GEFS"),
        {"rain_over", "Chance of rain over a limit in a period", {{"limit", "Rain over", "in", {0.01, 0.1, 0.25, 0.5, 1, 1.5, 2, 3, 4, 5, 6, 8, 10}, 1}, {"hours", "in", "hours", {6, 12, 18, 24, 30, 36, 42, 48, 54, 60, 72, 84, 96, 120, 168, 240}, 24}}},
        {"rain_under", "Chance of rain under a limit in a period (dry)", {{"limit", "Rain under", "in", {0.01, 0.1, 0.25, 0.5, 1}, 0.01}, {"hours", "in", "hours", {6, 12, 18, 24, 30, 36, 42, 48, 54, 60, 72, 84, 96, 120, 168, 240}, 120}}},
        {"ptype", "Chance of a precipitation type in 6 hours", {{"type", "Type", "", {1, 2, 3, 4}, 2, {"rain", "snow", "ice pellets", "freezing rain"}}, {"limit", "with at least", "in", {0.01, 0.1, 0.25, 0.5, 1}, 0.01}}},
        {"heavy_rain", "Heavy rain potential (precipitable water + CAPE + rain)", {{"sigmas", "Precipitable water over", "std. dev. above normal", {1, 1.5, 2, 2.5, 3}, 2}, {"cape", "and MUCAPE over", "J/kg", {100, 250, 500, 1000, 1500, 2000}, 500}, {"qpf", "and rain in 6 hours over", "in", {0.01, 0.1, 0.25, 0.5, 1}, 0.1}}},
        {"stdanom", "Chance of an anomaly of so many standard deviations", {{"what", "Field", "", {1, 2, 3, 4, 5}, 1, {"500 mb height", "700 mb height", "700 mb temperature", "850 mb temperature", "precipitable water"}}, {"direction", "Anomaly", "", {1, 2}, 1, {"below normal", "above normal"}}, {"sigmas", "by more than", "std. dev.", {1, 2, 3, 4, 5}, 1}}},
        one("temp_under", "Chance of a 2 m temperature under a limit", "Temperature under", "F", {-40, -20, 0, 10, 20, 25, 28, 32, 40, 50, 60, 70}, 32),
        one("temp_over", "Chance of a 2 m temperature over a limit", "Temperature over", "F", {70, 80, 90, 95, 100, 105, 110, 115}, 90),
        one("tmin_under", "Chance of a 6 hour minimum temperature under a limit", "Lowest in 6 hours under", "F", {-40, -20, 0, 20, 32, 40}, 32),
        one("tmax_over", "Chance of a 6 hour maximum temperature over a limit", "Highest in 6 hours over", "F", {70, 80, 90, 100, 110, 120}, 90),
        one("tmax_under", "Chance of a 6 hour maximum temperature under a limit", "Highest in 6 hours under", "F", {-40, -20, 0, 20, 32}, 20),
        one("dewpoint_over", "Chance of a dewpoint over a limit", "Dewpoint over", "F", {50, 55, 60, 65, 70, 75, 80}, 65),
        one("heat_over", "Chance of a heat index over a limit", "Heat index over", "F", {90, 100, 105, 110, 115, 120}, 105),
        one("rh_under", "Chance of a relative humidity under a limit", "Humidity under", "%", {5, 10, 15, 20, 30}, 15),
        one("wind_over", "Chance of a 10 m wind over a limit", "Wind over", "kt", {10, 15, 20, 30, 40, 50, 75}, 20),
        one("gust_over", "Chance of wind gusts over a limit", "Gusts over", "kt", {20, 25, 30, 35, 40, 50, 60, 70}, 35),
        one("mslp_under", "Chance of a sea level pressure under a limit", "Pressure under", "mb", {960, 965, 970, 980, 990, 1000}, 990),
        one("mslp_over", "Chance of a sea level pressure over a limit", "Pressure over", "mb", {1020, 1030, 1035, 1040}, 1030),
        one("pw_over", "Chance of precipitable water over a limit", "Precipitable water over", "in", {1, 1.25, 1.5, 1.75, 2, 2.25}, 1.5),
        one("cloud_under", "Chance of a 6 hour mean cloud cover under a limit", "Cloud cover under", "%", {10, 30}, 10),
        one("cloud_over", "Chance of a 6 hour mean cloud cover over a limit", "Cloud cover over", "%", {70, 90}, 90),
        one("cape_over", "Chance of CAPE over a limit", "CAPE over", "J/kg", {250, 500, 1000, 1500, 2000, 3000, 4000, 5000, 6000, 8000}, 1000),
        one("mucape_over", "Chance of the most unstable CAPE over a limit", "MUCAPE over", "J/kg", {250, 500, 1000, 1500, 2000, 3000, 4000}, 500),
        one("z500_over", "Chance of a 500 mb height over a limit", "500 mb height over", "m", {5760, 5820, 5880, 5940, 6000}, 5940),
    };
}

std::string generatedChart(const std::string& templateId, const std::vector<double>& values) {
    return generatedId(templateId, values);
}

std::string sourceLabel(const std::string& source, const std::string& runId) {
    const auto * def = GfsModels::find(source);
    if (def && !def->legacyLabel.empty() && runId.size() >= 10 && (runId < def->legacyBefore || GfsData::fellBack(source, runId))) {   // a run from before the model's files changed
        return def->legacyLabel;
    }
    return def ? def->label : "NOAA/NCEP GFS 0.25 degree";
}

std::vector<std::string> sectorIds(const std::string& source) {
    const auto * def = GfsModels::find(source);
    if (def && def->sectors == GfsModels::Sectors::Conus) {   // the contiguous United States and its regions
        return {"CONUS", "NORTHEAST", "MID-ATLANTIC", "SOUTHEAST", "GREAT-LAKES", "OHIO-VALLEY", "S-PLAINS", "C-PLAINS", "N-PLAINS", "ROCKIES", "SOUTHWEST", "PACIFIC-NW", "CALIFORNIA", "GULF-COAST"};
    }
    std::vector<std::string> all;
    for (const auto& s : sectors()) {
        all.push_back(s.id);
    }
    return all;
}

std::vector<GfsData::Need> needs(const Product& product, int hour) {
    if (product.needs) {
        return product.needs(hour);
    }
    std::vector<GfsData::Need> list;
    for (const auto& w : product.wants) {
        list.push_back({hour, w});
    }
    return list;
}


// ---- ticked onto a chart: lines and barbs
namespace {
    struct Overlay {
        std::string id;
        std::string label;
        std::string group;
        std::vector<std::string> sources;
        std::function<std::vector<GfsData::Need>(int hour)> needs;      // keys are the overlay's own; compose() prefixes them
        std::function<void(Grids&, const Context&)> derive;             // on the overlay's own keys
        bool barbs{false};
        bool fill{false};           // a translucent color fill laid over the chart's own (which stays): the grid "fill|<name>" painted with fillRamp
        Ramp fillRamp;
        std::string fillName;       // the name under the picture's legend and in the header
        std::function<std::vector<GfsData::Need>(int hour)> needsSpecific;   // a humidity for a model that has the specific humidity, not the relative (the program works it out)
        ContourSet contour;
        std::string barbU, barbV;
        QColor barbColor{20, 20, 20};   // the barbs stacked with others: a color for each level
        std::string barbColorName{"black"};
    };

    std::string overlayPrefix(const Overlay& o) {
        return "o_" + o.id + "_";
    }

    const std::vector<Overlay>& overlayCatalog() {
        static const std::vector<Overlay> all = [] {
            std::vector<Overlay> list;
            const auto record = [] (const char * key, const char * variable, const char * level) {
                return [=] (int hour) { return GfsData::Need{hour, {key, variable, level, "", ""}}; };
            };
            {   // sea level pressure
                Overlay o;
                o.id = "mslp";
                o.label = "Sea level pressure";
                o.group = "Lines";
                o.sources = GfsModels::overlayModels();
                o.needs = [=] (int hour) { return std::vector<GfsData::Need>{record("p", "PRMSL", "mean sea level")(hour)}; };
                o.contour.key = "p";
                o.contour.scale = 0.01;
                o.contour.interval = 4;
                o.contour.title = "Sea level pressure (mb)";
                o.contour.highsAndLows = true;
                list.push_back(o);
            }
            for (const auto& [id, label, from, to, interval, edge] : {std::tuple{"thick_1000_500", "1000-500mb thickness", "1000 mb", "500 mb", 6.0, 540.0}, {"thick_1000_850", "1000-850mb thickness", "1000 mb", "850 mb", 3.0, 130.0},
                                                                         {"thick_850_700", "850-700mb thickness", "850 mb", "700 mb", 3.0, 154.0}}) {
                Overlay o;
                o.id = id;
                o.label = label;
                o.group = "Lines";
                o.sources = GfsModels::overlayModels();
                o.needs = [=] (int hour) { return std::vector<GfsData::Need>{record("zl", "HGT", from)(hour), record("zh", "HGT", to)(hour)}; };
                o.derive = [] (Grids& g, const Context&) { g["thick"] = GfsGrid::difference(g["zh"], g["zl"]); };
                o.contour.key = "thick";
                o.contour.scale = 0.1;
                o.contour.interval = interval;
                o.contour.title = std::string{label} + " (dam)";
                o.contour.color = QColor{190, 50, 40};
                o.contour.colorBelow = QColor{40, 90, 190};
                o.contour.split = edge;
                o.contour.dashed = true;
                o.contour.width = 1.3;
                list.push_back(o);
            }
            for (const auto& [level, interval] : {std::pair{"200", 12.0}, {"250", 12.0}, {"300", 12.0}, {"500", 6.0}, {"700", 3.0}, {"850", 3.0}}) {
                Overlay o;
                o.id = std::string{"z"} + level;
                o.label = std::string{level} + "mb height";
                o.group = "Lines";
                o.sources = GfsModels::overlayModels();
                const std::string levelText = std::string{level} + " mb";
                o.needs = [levelText] (int hour) { return std::vector<GfsData::Need>{{hour, {"z", "HGT", levelText, "", ""}}}; };
                o.contour.key = "z";
                o.contour.scale = 0.1;
                o.contour.interval = interval;
                // the same color for a level as its wind barbs have when they are stacked (850 red, 700 green, 500 blue ...)
                static const std::map<std::string, std::pair<QColor, std::string>> levelColors{
                    {"850", {QColor{200, 40, 40}, "red"}}, {"700", {QColor{30, 140, 60}, "green"}}, {"500", {QColor{30, 80, 200}, "blue"}}, {"300", {QColor{130, 50, 170}, "purple"}},
                    {"250", {QColor{200, 60, 150}, "pink"}}, {"200", {QColor{140, 90, 40}, "brown"}}};
                const auto& tint = levelColors.at(level);
                o.contour.title = std::string{level} + "mb height (dam, " + tint.second + ")";
                o.contour.color = tint.first;
                list.push_back(o);
            }
            // the four fields of the upper air, wind, temperature, humidity and height, at the main levels (and the wind and temperature near the ground), each as a color fill laid over the chart's own
            // fill (translucent: the chart's fill stays), as lines, or (the wind) as barbs; the height lines and the barbs are further up
            {
                struct Level {
                    const char * text;     // the level in the file
                    const char * tag;      // in the ids: "850", "10m"
                    int hPa;               // 0 for the ground
                };
                const std::vector<Level> levels{{"850 mb", "850", 850}, {"700 mb", "700", 700}, {"500 mb", "500", 500}, {"300 mb", "300", 300}, {"250 mb", "250", 250}, {"200 mb", "200", 200}};
                const auto translucent = [] (Ramp ramp, int alpha, double clearBelow) {   // the program's scale with some see-through in it, and none under `clearBelow`
                    for (auto& stop : ramp.stops) {
                        stop.second.setAlpha(stop.first < clearBelow ? 0 : alpha);
                    }
                    return ramp;
                };
                const auto heightRamp = [] (double low, double high) {   // meters: blue where low, red where high
                    const double span = high - low;
                    return Ramp{{{low, QColor{"#2b4a9a"}}, {low + span * 0.25, QColor{"#86b4dc"}}, {low + span * 0.5, QColor{"#f1f1e0"}}, {low + span * 0.75, QColor{"#f0b070"}}, {high, QColor{"#a02a2a"}}}};
                };
                const auto humidityFor = [] (const char * level, int hPa) {   // the relative humidity of a level: its own, or from the temperature and the specific humidity
                    return std::pair{
                        std::function<std::vector<GfsData::Need>(int)>{[level] (int hour) { return std::vector<GfsData::Need>{{hour, {"r", "RH", level, "", ""}}}; }},
                        std::function<std::vector<GfsData::Need>(int)>{[level] (int hour) { return std::vector<GfsData::Need>{{hour, {"t", "TMP", level, "", ""}}, {hour, {"q", "SPFH", level, "", ""}}}; }}};
                };
                const auto humidityDerive = [] (int hPa) {
                    return [hPa] (Grids& g, const Context&) {
                        if (g.find("r") != g.end() || g.find("q") == g.end() || g.find("t") == g.end()) {
                            return;
                        }
                        auto r = g.at("t");
                        const auto& q = g.at("q");
                        for (size_t i = 0; i < r.values.size() && i < q.values.size(); i++) {   // (degrees C and kg/kg: the vapor pressure over the saturation pressure)
                            const double t = r.values[i], specific = q.values[i];
                            const double e = specific * hPa / (0.622 + 0.378 * specific), es = 6.112 * std::exp(17.67 * t / (t + 243.5));
                            r.values[i] = std::isfinite(e / es) ? static_cast<float>(std::clamp(100.0 * e / es, 0.0, 100.0)) : std::nanf("");
                        }
                        g["r"] = std::move(r);
                    };
                };
                const auto speedDerive = [] (const char * key) {   // the speed of the u and v in knots, as `key`
                    const std::string name = key;
                    return [name] (Grids& g, const Context&) { g[name] = GfsGrid::scaled(GfsGrid::speed(g.at("u"), g.at("v")), msToKnots); };
                };
                const auto windNeeds = [] (const char * level) {
                    const std::string text = level;
                    return [text] (int hour) { return std::vector<GfsData::Need>{{hour, {"u", "UGRD", text, "", ""}}, {hour, {"v", "VGRD", text, "", ""}}}; };
                };
                // the level of a line: more or fewer of them, as the field changes with the height
                const auto isotachInterval = [] (int hPa) { return hPa >= 500 || hPa == 0 ? (hPa == 0 ? 10.0 : (hPa == 500 ? 20.0 : 20.0)) : 10.0; };
                for (const auto& level : levels) {
                    const std::string tag = level.tag, text = level.text, name = tag + "mb";
                    // wind
                    {
                        Overlay f;
                        f.id = "fill_wind_" + tag;
                        f.label = name + " wind speed";
                        f.group = "Fill";
                        f.sources = GfsModels::overlayModels();
                        f.fill = true;
                        f.fillName = "Wind " + name + " (kt)";
                        f.fillRamp = translucent(windSpeed(), 150, level.hPa >= 850 ? 15.0 : level.hPa >= 700 ? 20.0 : level.hPa >= 500 ? 30.0 : 50.0);   // (only the strong wind of the level is tinted: the jet's, not all the air)
                        f.needs = windNeeds(level.text);
                        f.derive = speedDerive(("fill|" + f.fillName).c_str());
                        list.push_back(f);
                        Overlay l;
                        l.id = "line_wind_" + tag;
                        l.label = name + " wind speed";
                        l.group = "Lines";
                        l.sources = GfsModels::overlayModels();
                        l.needs = windNeeds(level.text);
                        l.derive = speedDerive("s");
                        l.contour.key = "s";
                        l.contour.interval = isotachInterval(level.hPa);
                        l.contour.title = name + " wind speed (kt)";
                        l.contour.color = QColor{110, 60, 160};
                        l.contour.width = 1.2;
                        list.push_back(l);
                    }
                    // temperature
                    {
                        Overlay f;
                        f.id = "fill_temp_" + tag;
                        f.label = name + " temperature";
                        f.group = "Fill";
                        f.sources = GfsModels::overlayModels();
                        f.fill = true;
                        f.fillName = "Temperature " + name + " (C)";
                        f.fillRamp = translucent(temperature(), 120, -1e9);
                        f.needs = [text, key = "fill|" + f.fillName] (int hour) { return std::vector<GfsData::Need>{{hour, {key, "TMP", text, "", ""}}}; };
                        list.push_back(f);
                        Overlay l;
                        l.id = "line_temp_" + tag;
                        l.label = name + " temperature";
                        l.group = "Lines";
                        l.sources = GfsModels::overlayModels();
                        l.needs = [text] (int hour) { return std::vector<GfsData::Need>{{hour, {"t", "TMP", text, "", ""}}}; };
                        l.contour.key = "t";
                        l.contour.interval = 5;
                        l.contour.title = name + " temperature (C)";
                        l.contour.color = QColor{190, 50, 40};
                        l.contour.colorBelow = QColor{40, 90, 190};
                        l.contour.split = 0.0;
                        l.contour.dashed = true;
                        l.contour.width = 1.3;
                        list.push_back(l);
                    }
                    // humidity
                    {
                        const auto [own, specific] = humidityFor(level.text, level.hPa);
                        Overlay f;
                        f.id = "fill_rh_" + tag;
                        f.label = name + " relative humidity";
                        f.group = "Fill";
                        f.sources = GfsModels::overlayModels();
                        f.fill = true;
                        f.fillName = "Humidity " + name + " (%)";
                        f.fillRamp = translucent(humidity(), 140, 50.0);   // (only the moist air is tinted)
                        f.needs = own;
                        f.needsSpecific = specific;
                        const std::string fillKey = "fill|" + f.fillName;
                        const auto derive = humidityDerive(level.hPa);
                        f.derive = [derive, fillKey] (Grids& g, const Context& c) {
                            derive(g, c);
                            if (g.find("r") != g.end()) {
                                g[fillKey] = g.at("r");
                            }
                        };
                        list.push_back(f);
                        Overlay l;
                        l.id = "line_rh_" + tag;
                        l.label = name + " relative humidity";
                        l.group = "Lines";
                        l.sources = GfsModels::overlayModels();
                        l.needs = own;
                        l.needsSpecific = specific;
                        l.derive = derive;
                        l.contour.key = "r";
                        l.contour.interval = 20;
                        l.contour.minimum = 19;
                        l.contour.title = name + " relative humidity (%)";
                        l.contour.color = QColor{30, 140, 70};
                        l.contour.width = 1.2;
                        list.push_back(l);
                    }
                    // height (the lines are the ones further down: z500 ...)
                    {
                        Overlay f;
                        f.id = "fill_hgt_" + tag;
                        f.label = name + " height";
                        f.group = "Fill";
                        f.sources = GfsModels::overlayModels();
                        f.fill = true;
                        f.fillName = "Height " + name + " (dam)";
                        const double mid = level.hPa == 850 ? 1450 : level.hPa == 700 ? 3000 : level.hPa == 500 ? 5600 : level.hPa == 300 ? 9200 : level.hPa == 250 ? 10400 : 11900;
                        const double half = level.hPa == 850 ? 180 : level.hPa == 700 ? 200 : level.hPa == 500 ? 340 : level.hPa == 300 ? 450 : level.hPa == 250 ? 520 : 600;
                        f.fillRamp = translucent(heightRamp((mid - half) / 10.0, (mid + half) / 10.0), 120, -1e9);
                        const std::string fillKey = "fill|" + f.fillName;
                        f.needs = [text] (int hour) { return std::vector<GfsData::Need>{{hour, {"z", "HGT", text, "", ""}}}; };
                        f.derive = [fillKey] (Grids& g, const Context&) { g[fillKey] = GfsGrid::scaled(g.at("z"), 0.1); };
                        list.push_back(f);
                    }
                }
                // the wind and the temperature near the ground
                {
                    Overlay f;
                    f.id = "fill_wind_10m";
                    f.label = "10 m wind speed";
                    f.group = "Fill";
                    f.sources = GfsModels::overlayModels();
                    f.fill = true;
                    f.fillName = "Wind 10 m (kt)";
                    f.fillRamp = translucent(windSpeed(), 150, 8.0);
                    f.needs = windNeeds("10 m above ground");
                    f.derive = speedDerive(("fill|" + f.fillName).c_str());
                    list.push_back(f);
                    Overlay l;
                    l.id = "line_wind_10m";
                    l.label = "10 m wind speed";
                    l.group = "Lines";
                    l.sources = GfsModels::overlayModels();
                    l.needs = windNeeds("10 m above ground");
                    l.derive = speedDerive("s");
                    l.contour.key = "s";
                    l.contour.interval = 10;
                    l.contour.title = "10 m wind speed (kt)";
                    l.contour.color = QColor{110, 60, 160};
                    l.contour.width = 1.2;
                    list.push_back(l);
                    Overlay t;
                    t.id = "fill_temp_2m";
                    t.label = "2 m temperature";
                    t.group = "Fill";
                    t.sources = GfsModels::overlayModels();
                    t.fill = true;
                    t.fillName = "Temperature 2 m (C)";
                    t.fillRamp = translucent(temperature(), 120, -1e9);
                    t.needs = [key = "fill|" + t.fillName] (int hour) { return std::vector<GfsData::Need>{{hour, {key, "TMP", "2 m above ground", "", ""}}}; };
                    list.push_back(t);
                    Overlay tl;
                    tl.id = "line_temp_2m";
                    tl.label = "2 m temperature";
                    tl.group = "Lines";
                    tl.sources = GfsModels::overlayModels();
                    tl.needs = [] (int hour) { return std::vector<GfsData::Need>{{hour, {"t", "TMP", "2 m above ground", "", ""}}}; };
                    tl.contour.key = "t";
                    tl.contour.interval = 5;
                    tl.contour.title = "2 m temperature (C)";
                    tl.contour.color = QColor{190, 50, 40};
                    tl.contour.colorBelow = QColor{40, 90, 190};
                    tl.contour.split = 0.0;
                    tl.contour.dashed = true;
                    tl.contour.width = 1.3;
                    list.push_back(tl);
                    Overlay df;
                    df.id = "fill_dewpoint_2m";
                    df.label = "2 m dewpoint";
                    df.group = "Fill";
                    df.sources = GfsModels::overlayModels();
                    df.fill = true;
                    df.fillName = "Dewpoint 2 m (C)";
                    df.fillRamp = translucent(dewpoint(), 150, 15.0);   // (the moist air of the surface, from about 60 F: under it the chart's own fill shows)
                    df.needs = [key = "fill|" + df.fillName] (int hour) { return std::vector<GfsData::Need>{{hour, {key, "DPT", "2 m above ground", "", ""}}}; };
                    list.push_back(df);
                    Overlay dl;
                    dl.id = "line_dewpoint_2m";
                    dl.label = "2 m dewpoint";
                    dl.group = "Lines";
                    dl.sources = GfsModels::overlayModels();
                    dl.needs = [] (int hour) { return std::vector<GfsData::Need>{{hour, {"d", "DPT", "2 m above ground", "", ""}}}; };
                    dl.contour.key = "d";
                    dl.contour.interval = 5;
                    dl.contour.title = "2 m dewpoint (C)";
                    dl.contour.color = QColor{30, 140, 70};
                    dl.contour.dashed = true;
                    dl.contour.width = 1.3;
                    list.push_back(dl);
                    // the humidity near the ground, from the 2 m temperature and dewpoint (every model has those; not every model has the humidity itself)
                    const auto needs2m = [] (int hour) { return std::vector<GfsData::Need>{{hour, {"t", "TMP", "2 m above ground", "", ""}}, {hour, {"d", "DPT", "2 m above ground", "", ""}}}; };
                    const auto humidity2m = [] (Grids& g, const Context&) {
                        if (g.find("t") == g.end() || g.find("d") == g.end()) {
                            return;
                        }
                        auto r = g.at("t");
                        const auto& d = g.at("d");
                        for (size_t i = 0; i < r.values.size() && i < d.values.size(); i++) {   // (degrees C: the vapor pressure over the saturation pressure, Magnus)
                            const double t = r.values[i], td = d.values[i];
                            const double value = 100.0 * std::exp(17.625 * td / (243.04 + td)) / std::exp(17.625 * t / (243.04 + t));
                            r.values[i] = std::isfinite(value) ? static_cast<float>(std::clamp(value, 0.0, 100.0)) : std::nanf("");
                        }
                        g["r"] = std::move(r);
                    };
                    Overlay hf;
                    hf.id = "fill_rh_2m";
                    hf.label = "2 m relative humidity";
                    hf.group = "Fill";
                    hf.sources = GfsModels::overlayModels();
                    hf.fill = true;
                    hf.fillName = "Humidity 2 m (%)";
                    hf.fillRamp = translucent(humidity(), 140, 50.0);
                    hf.needs = needs2m;
                    const std::string humidityKey = "fill|" + hf.fillName;
                    hf.derive = [humidity2m, humidityKey] (Grids& g, const Context& c) {
                        humidity2m(g, c);
                        if (g.find("r") != g.end()) {
                            g[humidityKey] = g.at("r");
                        }
                    };
                    list.push_back(hf);
                    Overlay hl;
                    hl.id = "line_rh_2m";
                    hl.label = "2 m relative humidity";
                    hl.group = "Lines";
                    hl.sources = GfsModels::overlayModels();
                    hl.needs = needs2m;
                    hl.derive = humidity2m;
                    hl.contour.key = "r";
                    hl.contour.interval = 20;
                    hl.contour.minimum = 19;
                    hl.contour.title = "2 m relative humidity (%)";
                    hl.contour.color = QColor{30, 140, 70};
                    hl.contour.width = 1.2;
                    list.push_back(hl);
                }
            }
            // wind barbs: the 10 m wind, and the wind of the main levels
            for (const auto& [id, label, level] : {std::tuple{"barbs_10m", "10 m wind", "10 m above ground"}, {"barbs_850", "850mb wind", "850 mb"}, {"barbs_700", "700mb wind", "700 mb"},
                                                    {"barbs_500", "500mb wind", "500 mb"}, {"barbs_300", "300mb wind", "300 mb"}, {"barbs_250", "250mb wind", "250 mb"}, {"barbs_200", "200mb wind", "200 mb"}}) {
                Overlay o;
                o.id = id;
                o.label = label;
                o.group = "Wind barbs";
                o.sources = GfsModels::overlayModels();
                o.barbs = true;
                const std::string levelText = level;
                o.needs = [levelText] (int hour) { return std::vector<GfsData::Need>{{hour, {"u", "UGRD", levelText, "", ""}}, {hour, {"v", "VGRD", levelText, "", ""}}}; };
                o.barbU = "u";
                o.barbV = "v";
                static const std::map<std::string, std::pair<QColor, std::string>> levelColors{
                    {"barbs_10m", {QColor{90, 90, 90}, "gray"}}, {"barbs_850", {QColor{200, 40, 40}, "red"}}, {"barbs_700", {QColor{30, 140, 60}, "green"}}, {"barbs_500", {QColor{30, 80, 200}, "blue"}},
                    {"barbs_300", {QColor{130, 50, 170}, "purple"}}, {"barbs_250", {QColor{200, 60, 150}, "pink"}}, {"barbs_200", {QColor{140, 90, 40}, "brown"}}};
                if (const auto found = levelColors.find(id); found != levelColors.end()) {
                    o.barbColor = found->second.first;
                    o.barbColorName = found->second.second;
                }
                list.push_back(o);
            }
            {   // the blend gives the 10 m wind as a speed and a direction
                Overlay o;
                o.id = "barbs_10m";
                o.label = "10 m wind";
                o.group = "Wind barbs";
                o.sources = {"NBM"};
                o.barbs = true;
                o.needs = [] (int hour) {
                    const auto when = std::to_string(hour) + " hour fcst";
                    return std::vector<GfsData::Need>{{hour, {"ws", "WIND", "10 m above ground", when, ""}}, {hour, {"wd", "WDIR", "10 m above ground", when, ""}}};
                };
                o.derive = [] (Grids& g, const Context&) {
                    auto u = g["ws"], v = g["ws"];
                    for (size_t i = 0; i < u.values.size(); i++) {
                        const double speedNow = g["ws"].values[i], from = g["wd"].values[i] * pi / 180.0;
                        u.values[i] = static_cast<float>(-speedNow * std::sin(from));
                        v.values[i] = static_cast<float>(-speedNow * std::cos(from));
                    }
                    g["u"] = std::move(u);
                    g["v"] = std::move(v);
                };
                o.barbU = "u";
                o.barbV = "v";
                list.push_back(o);
            }
            return list;
        }();
        return all;
    }

    const Overlay * overlayNamed(const std::string& id, const std::string& source) {
        for (const auto& o : overlayCatalog()) {
            if (o.id == id && std::find(o.sources.begin(), o.sources.end(), source) != o.sources.end()) {
                return &o;
            }
        }
        return nullptr;
    }
}

std::string category(const Product& product) {
    if (!product.pickerGroup.empty()) {
        return product.pickerGroup;
    }
    std::string id = product.id;
    if (id.compare(0, 4, "gen~") == 0) {
        return "Built charts";
    }
    std::transform(id.begin(), id.end(), id.begin(), [] (unsigned char c) { return static_cast<char>(std::tolower(c)); });   // ("prob_low_IFR")
    const auto contains = [] (const std::string& text, const char * part) { return text.find(part) != std::string::npos; };
    const auto startsWith = [] (const std::string& text, const char * prefix) { return text.compare(0, std::char_traits<char>::length(prefix), prefix) == 0; };
    // the kind of statistic comes first (what the ensemble makes of the members), the field second: the charts of the ensembles would otherwise all sit in "Surface"
    std::string core = id;   // the id without the words that say how it was made: "ens_850_wnd" is an 850 mb wind chart
    for (bool again = true; again;) {
        again = false;
        for (const char * prefix : {"ens_", "all_", "mean_", "member_", "combo_", "blend_", "lpmm_", "pmm_", "eas_"}) {
            if (startsWith(core, prefix)) {
                core = core.substr(std::char_traits<char>::length(prefix));
                again = true;
            }
        }
    }
    if (startsWith(id, "spread_") || startsWith(id, "all_spread_") || contains(id, "_spread")) {
        return "Ensemble: spread, spaghetti and paintballs";
    }
    if (startsWith(id, "swath")) {
        return "Swaths";
    }
    if ((contains(id, "prob_") || (id.size() > 5 && id.compare(id.size() - 5, 5, "_prob") == 0)) && !contains(id, "problem")) {   // the chance of something, by what it is the chance of
        const auto has = [&] (const char * part) { return contains(core, part); };
        if (has("precip") || has("rain") || has("snow") || has("ice") || has("sleet") || has("freezing_rain") || has("pwat") || has("total") || has("qpf")) {
            return "Probability: precipitation";
        }
        if (has("vis") || has("ceil") || has("ifr") || has("vfr")) {
            return "Probability: aviation and visibility";
        }
        if (has("cape") || has("tstm") || has("hlcy") || has("uphl") || has("refd") || has("cref") || has("refc") || has("etop") || has("vwshr") || has("shear") || has("cin") || has("tornado") || has("hail") || has("lightning")) {
            return "Probability: severe and storms";
        }
        return "Probability: temperature and wind";
    }
    if (startsWith(core, "pct") || contains(core, "efi_") || startsWith(core, "efi")) {
        return "Ensemble: percentiles and extremes";
    }
    if (startsWith(core, "thunder") || startsWith(id, "severe_") || startsWith(id, "gefsmlp_")) {   // the SPC's post-processed chances of thunder and severe weather
        return "Probability: severe and storms";
    }
    if (contains(id, "spag") || contains(id, "_iso") || contains(id, "paintball") || (contains(id, "_ht") && id.size() > 8 && std::isdigit(static_cast<unsigned char>(id[0])) && std::count(id.begin(), id.end(), '_') >= 2 && std::isdigit(static_cast<unsigned char>(id[id.find('_') + 1])))) {
        return "Ensemble: spread, spaghetti and paintballs";   // (the last: "500_510_552_ht", a level then the contour values)
    }
    if (startsWith(id, "member_")) {
        return "Ensemble: single members";
    }
    const auto has = [&core] (const char * text) { return core.find(text) != std::string::npos; };
    if (has("trend") || has("chng")) {
        return "Changes and trends";
    }
    if (has("anom")) {
        return "Anomalies";
    }
    if (startsWith(core, "sat_")) {
        return "Satellite";
    }
    if (core == "sst" || core == "waves" || has("wave") || has("wv_ht") || has("swell") || has("dir_per")) {
        return "Ocean and waves";
    }
    // the severe weather charts in kinds: the fuel, the rotation, the storms themselves and the composite indices
    const bool endsWithCin = core.size() >= 3 && core.compare(core.size() - 3, 3, "cin") == 0;   // "sbcin", "mlcin", "ens_cin"
    if (has("cape") || has("cin_") || endsWithCin || has("lifted") || has("lapse") || has("lcl") || has("lfc")) {
        return "Severe: instability";
    }
    if (has("helicity") || has("hlcy") || has("srh") || has("relv") || has("uphl") || has("vwshr") || has("critical_angle") || has("wind_0_6km") ||
        has("shear_0_") || has("effective_shear")) {   // the low-level and storm shear of the severe weather indices (the deep layer shear of the cyclones is "dynamics")
        return "Severe: rotation and shear";
    }
    if (has("stp") || has("scp") || has("ship") || has("ehi") || has("dcp")) {
        return "Severe: composite indices";
    }
    if (has("radar") || has("reflectivity") || has("refd") || has("refc") || has("echo") || has("lightning") || has("updraft") || has("downdraft") || has("hail")) {
        return "Severe: storm structure";
    }
    if (has("dust") || has("aod") || has("pm25") || has("_aq") || has("uv_index") || has("smoke") || has("sulfate") || has("seasalt") || has("aerosol")) {
        return "Air quality and smoke";
    }
    if (core == "thetae_2m" || has("mixr")) {   // moisture at the surface, not the mixing of the layer
        return "Surface";
    }
    // the lowest kilometer and the fire weather indices: the mixing, the ventilation, the dry air
    if (has("pbl") || has("vent") || has("transport") || has("haines") || has("mix_") || has("mixing") || has("fire") || core == "2m_rh_10m_wnd") {
        return "Boundary layer and fire weather";
    }
    if (has("shear") || has("steering") || has("div") || has("stream") || has("thetae") || has("vor") || has("trop") || has("850vor")) {
        return "Tropical and dynamics";
    }
    if (has("vis") || has("ceil") || has("fog") || has("flight")) {
        return "Aviation and visibility";
    }
    if (core == "rh_700_300") {
        return "Upper air";
    }
    if (has("precip") || has("snow") || has("rain") || has("qpf") || has("ptype") || has("thick") || has("pwat") || has("ice")) {
        return "Precipitation and moisture";
    }
    if (core.size() > 3 && std::isdigit(static_cast<unsigned char>(core[0])) && (has("_wnd") || has("_temp") || has("_rh") || has("rh_700") || has("_ht") || core.compare(1, 2, "00") == 0 || core.compare(0, 3, "250") == 0 || core.compare(0, 3, "925") == 0) && core.compare(0, 3, "10m") != 0 && core.compare(0, 2, "2m") != 0) {
        return "Upper air";
    }
    return "Surface";
}

namespace {

// the strongest tint of an overlay fill's scale (the see-through the program gave it)
int fillAlpha(const Ramp& ramp) {
    int alpha = 0;
    for (const auto& stop : ramp.stops) {
        alpha = std::max(alpha, stop.second.alpha());
    }
    return alpha > 0 ? alpha : 150;
}

// the scale's own cut-off: the first value that is tinted when some of the lower ones are clear, else the bottom of the scale
double standardFrom(const Ramp& ramp) {
    for (const auto& stop : ramp.stops) {
        if (stop.second.alpha() > 0) {
            return stop.first;
        }
    }
    return ramp.stops.empty() ? 0.0 : ramp.stops.front().first;
}

// the scale as the preference FILLFROM_<id> has it: clear below the value, the tint from it up
Ramp adjustedFillRamp(const Overlay& o) {
    const auto text = Utility::readPref("FILLFROM_" + o.id, "");
    if (text.empty()) {
        return o.fillRamp;
    }
    Ramp ramp = o.fillRamp;
    const int alpha = fillAlpha(ramp);
    const double from = text == "all" ? -1e9 : std::atof(text.c_str());
    for (auto& stop : ramp.stops) {
        stop.second.setAlpha(stop.first < from ? 0 : alpha);
    }
    return ramp;
}

}  // namespace

FillFrom fillFrom(const std::string& overlayId) {
    FillFrom out;
    for (const auto& o : overlayCatalog()) {
        if (o.id != overlayId || !o.fill || o.fillRamp.stops.empty()) {
            continue;
        }
        out.available = true;
        out.standard = standardFrom(o.fillRamp);
        out.low = o.fillRamp.stops.front().first;
        out.high = o.fillRamp.stops.back().first;
        const auto text = Utility::readPref("FILLFROM_" + o.id, "");
        out.value = text.empty() ? out.standard : text == "all" ? out.low : std::atof(text.c_str());
        const auto open = o.fillName.rfind('('), close = o.fillName.rfind(')');
        out.unit = open != std::string::npos && close != std::string::npos && close > open ? o.fillName.substr(open + 1, close - open - 1) : std::string{};
        break;
    }
    return out;
}

Ramp asFillOverlay(const Ramp& user, const Ramp& overlay) {
    const int alpha = fillAlpha(overlay);
    double from = -1e9;
    const bool hasClear = std::any_of(overlay.stops.begin(), overlay.stops.end(), [] (const auto& stop) { return stop.second.alpha() == 0; });
    for (const auto& stop : overlay.stops) {
        if (hasClear && stop.second.alpha() > 0) {
            from = stop.first;
            break;
        }
    }
    Ramp out = user;
    for (auto& stop : out.stops) {
        stop.second.setAlpha(stop.first < from ? 0 : alpha);
    }
    return out;
}

std::string lineStyleToText(const LineStyle& style) {
    return style.color.name().toStdString() + "|" + QByteArray::number(style.width, 'g', 6).toStdString() + "|" + (style.dashed ? "1" : "0") + "|" + QByteArray::number(style.interval, 'g', 8).toStdString() + "|" + (style.labelled ? "1" : "0");
}

bool lineStyleFromText(const std::string& text, LineStyle& out) {
    const auto parts = QString::fromStdString(text).split('|');
    if (parts.size() != 5 || !QColor::isValidColorName(parts[0])) {
        return false;
    }
    bool okWidth = false, okInterval = false;
    const double width = parts[1].toDouble(&okWidth), interval = parts[3].toDouble(&okInterval);
    if (!okWidth || !okInterval || width <= 0.0 || width > 10.0 || interval < 0.0) {
        return false;
    }
    out.color = QColor{parts[0]};
    out.width = width;
    out.dashed = parts[2] == "1";
    out.interval = interval;
    out.labelled = parts[4] != "0";
    return true;
}

namespace {

LineStyle styleOfSet(const ContourSet& set) {
    return {set.color, set.width, set.dashed, set.interval, set.labelled};
}

// the line as the preference LINESTYLE_<id> has it (the program's when there is none, or it is not a style)
ContourSet styledContour(const Overlay& o) {
    ContourSet set = o.contour;
    LineStyle style;
    if (const auto text = Utility::readPref("LINESTYLE_" + o.id, ""); !text.empty() && lineStyleFromText(text, style)) {
        if (style.color != set.color) {   // the legend names the color in its title ("500mb height (dam, blue)"): a color of the user's is not that word
            static const QRegularExpression colorWord{", [a-z ]+\\)$"};
            auto title = QString::fromStdString(set.title);
            title.replace(colorWord, ")");
            set.title = title.toStdString();
        }
        set.color = style.color;
        set.width = style.width;
        set.dashed = style.dashed;
        set.interval = style.interval > 0.0 ? style.interval : set.interval;
        set.labelled = style.labelled;
    }
    return set;
}

}  // namespace

LineStyleInfo lineStyle(const std::string& overlayId) {
    LineStyleInfo out;
    for (const auto& o : overlayCatalog()) {
        if (o.id != overlayId || o.fill || o.barbs || o.contour.key.empty()) {
            continue;
        }
        out.available = true;
        out.standard = styleOfSet(o.contour);
        const auto set = styledContour(o);
        out.value = styleOfSet(set);
        out.title = o.contour.title;
        break;
    }
    return out;
}

void setLineStyle(const std::string& overlayId, const LineStyle& style) {
    const auto info = lineStyle(overlayId);
    if (!info.available) {
        return;
    }
    Utility::writePref("LINESTYLE_" + overlayId, style == info.standard ? std::string{} : lineStyleToText(style));
}

void clearLineStyle(const std::string& overlayId) {
    Utility::writePref("LINESTYLE_" + overlayId, "");
}

FillScale fillScale(const std::string& overlayId) {
    FillScale out;
    for (const auto& o : overlayCatalog()) {
        if (o.id != overlayId || !o.fill || o.fillRamp.stops.empty()) {
            continue;
        }
        out.available = true;
        out.ramp = o.fillRamp;
        for (auto& stop : out.ramp.stops) {
            stop.second.setAlpha(255);
        }
        out.quantity = overlayId.compare(0, 9, "fill_temp") == 0 || overlayId.compare(0, 13, "fill_dewpoint") == 0 ? Quantity::Temperature : Quantity::Other;
        out.title = o.fillName.empty() ? o.label : o.fillName;
        break;
    }
    return out;
}

std::vector<OverlayChoice> overlayChoices(const std::string& source) {
    std::vector<OverlayChoice> out;
    for (const auto& o : overlayCatalog()) {
        if (std::find(o.sources.begin(), o.sources.end(), source) != o.sources.end()) {
            out.push_back({o.id, o.label, o.group});
        }
    }
    return out;
}

std::vector<MemberChoice> memberChoices(const Product& product, const std::string& runId) {
    std::vector<GfsData::Want> wanted = product.wants;
    if (product.needs) {
        for (const auto& need : product.needs(12)) {
            wanted.push_back(need.want);
        }
    }
    std::vector<MemberChoice> out;
    if (wanted.empty() || product.linesOnly) {
        return out;
    }
    if (product.source == "SPCREFS") {   // the arrays with a member axis: ten members (SPC's viewer names them HRRR, RRFS and the REFS members)
        if (!product.wants.empty() && product.wants.front().stat == "m001") {
            const auto names = UtilitySpcRefs::memberNames(runId);   // the store's own names once one has been opened ("RRFS Ctl", "HRRR -6h" ...)
            for (int m = 1; m <= 10; m++) {
                const auto index = static_cast<size_t>(m - 1);
                out.push_back({"m" + std::string{m < 10 ? "00" : "0"} + std::to_string(m), index < names.size() && !names[index].empty() ? names[index] : "Member " + std::to_string(m)});
            }
        }
        return out;
    }
    if (product.source == "REFS") {
        if (product.id.compare(0, 7, "member_") == 0) {
            for (int m = 1; m <= 5; m++) {
                out.push_back({"m00" + std::to_string(m), "REFS M0" + std::to_string(m)});
            }
        }
        return out;
    }
    if (product.source == "IFS-ENS") {   // the single-member charts: the ensemble's 50 members, no mean (the mean and spread have charts of their own)
        if (product.id.compare(0, 7, "member_") == 0) {
            for (int m = 1; m <= 50; m++) {
                out.push_back({std::string{m < 10 ? "e0" : "e"} + std::to_string(m), "Member " + std::to_string(m)});
            }
        }
        return out;
    }
    if (product.source == "GEFS" || product.source == "GEPS") {
        // the charts made of the mean's fields alone: a spread among the wants (its lines, its derivations) has no one member's version
        for (const auto& w : wanted) {
            if (!w.stat.empty()) {
                return out;
            }
        }
        out.push_back({"", "Ensemble mean"});
        out.push_back({"c00", "Control"});
        for (int m = 1; m <= (product.source == "GEPS" ? 20 : 30); m++) {
            out.push_back({"p" + std::string{m < 10 ? "0" : ""} + std::to_string(m), "Member " + std::to_string(m)});
        }
    }
    return out;
}

Product withMember(const Product& product, const std::string& member, const std::string& runId) {
    if (member.empty()) {
        return product;
    }
    const auto choices = memberChoices(product, runId);
    const auto chosen = std::find_if(choices.begin(), choices.end(), [&member] (const MemberChoice& c) { return c.value == member; });
    if (chosen == choices.end()) {
        return product;
    }
    const bool refs = product.source == "REFS" || product.source == "SPCREFS";
    const bool ens = product.source == "IFS-ENS";
    const auto swap = [refs, ens, member] (GfsData::Want w) {
        if (ens ? (w.stat.size() > 1 && w.stat[0] == 'e') : refs ? w.stat.compare(0, 3, "m00") == 0 : w.stat.empty()) {
            w.stat = member;
        }
        return w;
    };
    Product out = product;
    for (auto& w : out.wants) {
        w = swap(w);
    }
    for (auto * f : {&out.needs, &out.fallbackNeeds}) {
        if (*f) {
            *f = [original = *f, swap] (int hour) {
                auto list = original(hour);
                for (auto& need : list) {
                    need.want = swap(need.want);
                }
                return list;
            };
        }
    }
    const auto named = [&chosen] (const std::string& text) {   // "Mean 500mb Height" -> "Member 7 500mb Height"; the rest gets the member in front
        return text.compare(0, 5, "Mean ") == 0 ? chosen->label + text.substr(4) : chosen->label + ": " + text;
    };
    out.label = refs || ens ? chosen->label + ": " + product.label.substr(product.label.find(": ") == std::string::npos ? 0 : product.label.find(": ") + 2) : named(product.label);
    if (!refs && product.fillTitle.compare(0, 5, "Mean ") == 0) {
        out.fillTitle = chosen->label + product.fillTitle.substr(4);
    }
    return out;
}

bool hasStandardDeviations(const Product& product) {
    return static_cast<bool>(product.sigma);
}

Product withPoolOnly(const Product& product, const std::string& source) {
    if (!product.pool.active() || source.empty()) {
        return product;
    }
    int count = 0;
    for (const auto& member : poolMembers()) {
        count += member.source == source ? 1 : 0;
    }
    const std::string name = source == "IFS-ENS" ? "ECMWF" : source;
    const std::string note = "only the " + std::to_string(count) + " members of the " + name;
    const auto retitle = [name, note] (std::string text) {
        for (const std::string phrase : {"% of the 102 members of the ECMWF, GEFS and GEPS together", "% of all the members, ECMWF, GEFS and GEPS"}) {
            const auto at = text.find(phrase);
            if (at != std::string::npos) {
                text.replace(at, phrase.size(), "% of " + note);
                return text;
            }
        }
        return text + " (" + note + ")";
    };
    Product out = product;
    out.pool.only = source;
    out.label = product.label + " - " + name + " only";
    if (!product.fillTitle.empty()) {
        out.fillTitle = retitle(product.fillTitle);
    }
    if (product.fillTitleFor) {
        const auto inner = product.fillTitleFor;
        out.fillTitleFor = [inner, retitle] (int hour) { return retitle(inner(hour)); };
    }
    return out;
}

Product withStandardDeviations(const Product& product) {
    if (!product.sigma) {
        return product;
    }
    Product out = product;
    out.derive = product.sigma;
    out.ramp = standardAnomaly();
    out.palette.clear();
    out.fillTitle = product.source == "CFS" || product.source == "CFS-ANL" ? "Anomaly from the CFS normal (1999-2010) in standard deviations of the 1991-2020 reanalysis" : "Anomaly from the 1991-2020 mean in standard deviations";
    out.legendStep = 1;
    out.label = product.label + " (std dev)";
    if (!product.sigmaReadKey.empty() && product.sigmaInterval > 0.0) {   // the plain anomaly beside the deviations: dashed, in its own color, so the three read together
        ContourSet anomaly;
        anomaly.key = product.sigmaReadKey;
        anomaly.interval = product.sigmaInterval;
        anomaly.title = "Anomaly (" + product.sigmaUnit + ")";
        anomaly.color = QColor{110, 60, 160};
        anomaly.dashed = true;
        anomaly.uniform = true;
        anomaly.width = 1.2;
        out.contours.push_back(anomaly);
    }
    return out;
}

Product compose(const Product& base, const std::vector<std::string>& ids) {
    std::vector<const Overlay *> used;
    for (const auto& id : ids) {
        const auto * o = overlayNamed(id, base.source);
        if (o == nullptr || std::find(used.begin(), used.end(), o) != used.end()) {
            continue;
        }
        // a line the base chart already draws is not drawn twice
        const bool already = !o->barbs && !o->fill && std::any_of(base.contours.begin(), base.contours.end(), [o] (const ContourSet& c) { return c.title == o->contour.title; });
        if (!already) {
            used.push_back(o);
        }
    }
    if (used.empty()) {
        return base;
    }
    Product p = base;
    const bool seaLevelAsMslet = base.source.compare(0, 4, "RRFS") == 0 || base.source.compare(0, 4, "HRRR") == 0;   // the Rapid Refresh calls its sea level pressure MSLET, the HRRR MSLMA
    const char * seaLevelName = base.source.compare(0, 4, "HRRR") == 0 ? "MSLMA" : "MSLET";
    const auto * baseModel = GfsModels::find(base.source);
    const bool specificHumidity = baseModel != nullptr && baseModel->clone.specificHumidity;   // a model with the specific humidity only: the humidity is worked out
    const auto add = [used, seaLevelAsMslet, seaLevelName, specificHumidity] (std::vector<GfsData::Need> needs, int hour) {
        for (const auto * o : used) {
            for (auto need : (specificHumidity && o->needsSpecific ? o->needsSpecific(hour) : o->needs(hour))) {
                need.want.key = overlayPrefix(*o) + need.want.key;
                if (seaLevelAsMslet && need.want.variable == "PRMSL") {
                    need.want.variable = seaLevelName;
                }
                needs.push_back(std::move(need));
            }
        }
        return needs;
    };
    p.needs = [base, add] (int hour) { return add(GfsChart::needs(base, hour), hour); };
    if (base.fallbackNeeds) {
        p.fallbackNeeds = [base, add] (int hour) {
            auto needs = base.fallbackNeeds(hour);
            return needs.empty() ? needs : add(std::move(needs), hour);
        };
    }
    p.derive = [base, used] (Grids& g, const Context& context) {
        if (base.derive) {
            base.derive(g, context);
        }
        for (const auto * o : used) {
            if (!o->derive) {
                continue;
            }
            const auto prefix = overlayPrefix(*o);
            Grids own;   // the overlay's keys without the prefix, so its derivation is written as if it were alone
            for (const auto& [key, grid] : g) {
                if (key.compare(0, prefix.size(), prefix) == 0) {
                    own[key.substr(prefix.size())] = grid;
                }
            }
            o->derive(own, context);
            for (auto& [key, grid] : own) {
                g[prefix + key] = std::move(grid);
            }
        }
    };
    std::string idText = base.id, labelText = base.label;
    for (const auto * o : used) {
        const auto prefix = overlayPrefix(*o);
        idText += "+" + o->id;
        labelText += (o == used.front() ? ", with " : ", ") + o->label + (o->fill ? " (fill)" : "");
        if (o->fill) {   // a fill over the chart's own: translucent, and the chart's colors stay (one at a time)
            p.overlays.erase(std::remove_if(p.overlays.begin(), p.overlays.end(), [] (const std::pair<std::string, Ramp>& entry) { return entry.first.compare(0, 2, "o_") == 0; }), p.overlays.end());
            p.overlays.push_back({prefix + "fill|" + o->fillName, adjustedFillRamp(*o)});
        } else if (o->barbs) {   // each ticked wind is a set of its own: they stack on the chart's own barbs
            p.barbSets.push_back({prefix + o->barbU, prefix + o->barbV, o->barbColor, o->label, o->barbColorName});
        } else {
            auto set = styledContour(*o);
            set.key = prefix + set.key;
            p.contours.push_back(std::move(set));
        }
    }
    p.id = idText;
    p.label = labelText;
    return p;
}

std::vector<GfsData::Need> fallbackNeeds(const Product& product, int hour) {
    return product.fallbackNeeds ? product.fallbackNeeds(hour) : std::vector<GfsData::Need>{};
}

namespace {
    // a number shown in the user's units
    double shown(Quantity quantity, double value, bool us) {
        if (!us) {
            return value;
        }
        switch (quantity) {
            case Quantity::Temperature: return value * 1.8 + 32.0;
            case Quantity::TemperatureDifference: return value * 1.8;
            case Quantity::Millimeters: return value / 25.4;
            case Quantity::Centimeters: return value / 2.54;
            case Quantity::Meters: return value * 3.28084;   // wave heights in feet
            default: return value;
        }
    }
    QString unitText(Quantity quantity, bool us) {
        switch (quantity) {
            case Quantity::Temperature: return us ? "°F" : "°C";
            case Quantity::TemperatureDifference: return us ? "°F" : "°C";
            case Quantity::Millimeters: return us ? "in" : "mm";
            case Quantity::Centimeters: return us ? "in" : "cm";
            case Quantity::Meters: return us ? "ft" : "m";
            default: return {};
        }
    }
    QString numberText(double rough) {
        const double value = std::round(rough * 1000.0) / 1000.0;   // 3.0000001 inches (from 76.2 millimeters) is 3
        const double a = std::abs(value);
        return QString::number(value, 'f', a >= 10 || a == std::floor(a) ? 0 : a >= 1 ? 1 : 2);
    }
}

GfsGrid::Grid fillOf(const Product& product, const Grids& fetched, const GfsData::Run& run, int forecastHour, const GfsClimate * climate) {
    Grids grids = fetched;
    for (auto& entry : grids) {
        GfsGrid::closeWrapGap(entry.second);
    }
    if (product.derive) {
        product.derive(grids, Context{forecastHour, run, climate, product.source});
    }
    return product.fill ? product.fill(grids) : GfsGrid::Grid{};
}

GfsGrid::Grid deltaInUserUnits(const GfsGrid::Grid& delta, Quantity quantity, bool us, std::string& unit) {
    double factor = 1.0;
    QString text = unitText(quantity, false);
    if (us) {
        switch (quantity) {
            case Quantity::Temperature: case Quantity::TemperatureDifference: factor = 1.8; break;
            case Quantity::Millimeters: factor = 1.0 / 25.4; break;
            case Quantity::Centimeters: factor = 1.0 / 2.54; break;
            case Quantity::Meters: factor = 3.28084; break;
            default: break;
        }
        text = unitText(quantity, true);
    }
    unit = text.toStdString();
    return factor == 1.0 ? delta : GfsGrid::scaled(delta, factor);
}

bool Probe::locate(double fx, double fy, double& lon, double& lat) const {
    const double x = fx * imageWidth, y = fy * imageHeight;
    if (x < areaLeft || x > areaLeft + areaWidth || y < areaTop || y > areaTop + areaHeight) {
        return false;
    }
    const Sector sector{"", west, south, east, north};   // View holds a reference: a temporary would be gone
    const View view{sector, QRectF{areaLeft, areaTop, areaWidth, areaHeight}};
    lon = view.lonAt(x);
    lat = view.latAt(y);
    while (lon > 180.0) {
        lon -= 360.0;
    }
    while (lon < -180.0) {
        lon += 360.0;
    }
    return true;
}

bool Probe::place(double lon, double lat, double& fx, double& fy) const {
    if (areaWidth <= 0.0 || imageWidth <= 0.0 || imageHeight <= 0.0) {
        return false;
    }
    const Sector sector{"", west, south, east, north};   // View holds a reference: a temporary would be gone
    const View view{sector, QRectF{areaLeft, areaTop, areaWidth, areaHeight}};
    const auto point = view.toPixel(lon, lat);
    if (point.x() < areaLeft || point.x() > areaLeft + areaWidth || point.y() < areaTop || point.y() > areaTop + areaHeight) {
        return false;
    }
    fx = point.x() / imageWidth;
    fy = point.y() / imageHeight;
    return true;
}

bool Probe::placeAnywhere(double lon, double lat, double& fx, double& fy) const {
    if (areaWidth <= 0.0 || imageWidth <= 0.0 || imageHeight <= 0.0) {
        return false;
    }
    const Sector sector{"", west, south, east, north};
    const View view{sector, QRectF{areaLeft, areaTop, areaWidth, areaHeight}};
    const auto point = view.toPixel(lon, lat);
    fx = point.x() / imageWidth;
    fy = point.y() / imageHeight;
    return true;
}

QString Probe::read(double fx, double fy) const {
    double lon = 0.0, lat = 0.0;
    if (!locate(fx, fy, lon, lat)) {
        return {};
    }
    const QChar degree{0x00B0};
    QStringList lines;
    lines << QString::number(std::abs(lat), 'f', 2) + degree + (lat >= 0.0 ? " N" : " S") + "   " + QString::number(std::abs(lon), 'f', 2) + degree + (lon >= 0.0 ? " E" : " W");
    for (const auto& field : fields) {
        const float value = field.grid.sample(lon, lat);
        if (std::isnan(value)) {
            continue;
        }
        if (!field.names.empty()) {   // a whole number that is a name
            const long named = std::lround(value);
            if (named >= 0 && named < static_cast<long>(field.names.size())) {
                lines << QString::fromStdString(field.label) + ": " + QString::fromStdString(field.names[static_cast<size_t>(named)]);
            }
            continue;
        }
        const double number = field.convert ? shown(field.quantity, value, us) : value * field.scale;
        lines << QString::fromStdString(field.label) + ": " + numberText(number);
    }
    if (!windU.empty() && !windV.empty()) {
        const double u = windU.sample(lon, lat), v = windV.sample(lon, lat);
        if (!std::isnan(u) && !std::isnan(v)) {
            const double speed = std::hypot(u, v) * 1.94384;
            const double from = std::fmod(std::atan2(-u, -v) * 180.0 / pi + 360.0, 360.0);
            lines << "Wind: " + QString::number(speed, 'f', 0) + " kt from " + QString::number(from, 'f', 0) + degree;
        }
    }
    return lines.join("\n");
}

double shownValue(Quantity quantity, double value, bool us) {
    return shown(quantity, value, us);
}

double storedValue(Quantity quantity, double value, bool us) {
    if (!us) {
        return value;
    }
    switch (quantity) {
        case Quantity::Temperature: return (value - 32.0) / 1.8;
        case Quantity::TemperatureDifference: return value / 1.8;
        case Quantity::Millimeters: return value * 25.4;
        case Quantity::Centimeters: return value * 2.54;
        case Quantity::Meters: return value / 3.28084;
        default: return value;
    }
}

std::string unitLabel(Quantity quantity, bool us) {
    return unitText(quantity, us).toStdString();
}

Ramp defaultRamp(const Product& product, bool magColors) {
    if (magColors && !product.palette.empty()) {   // the model guidance site's color bands in place of ours
        if (const auto * bands = magPalette(product.palette)) {
            return *bands;
        }
    }
    return product.ramp;
}

std::string rampKey(const Ramp& ramp) {
    if (ramp.stops.empty()) {
        return {};
    }
    QByteArray text = ramp.banded ? "b" : "s";
    for (const auto& stop : ramp.stops) {
        text += ":" + QByteArray::number(stop.first, 'g', 8);
    }
    return "r" + std::to_string(ramp.stops.size()) + "_" + QByteArray::number(qHash(text), 16).toStdString();
}

std::string rampToText(const Ramp& ramp) {
    std::string out = std::string{"banded "} + (ramp.banded ? "1" : "0") + "\n";
    for (const auto& [value, color] : ramp.stops) {
        out += QByteArray::number(value, 'g', 10).toStdString() + " " + std::to_string(color.red()) + " " + std::to_string(color.green()) + " " + std::to_string(color.blue()) + " " + std::to_string(color.alpha()) + "\n";
    }
    return out;
}

bool rampFromText(const std::string& text, Ramp& ramp, std::string& error) {
    Ramp parsed;
    int lineNumber = 0;
    for (const auto& raw : QString::fromStdString(text).split('\n')) {
        lineNumber++;
        const auto line = raw.trimmed();
        if (line.isEmpty() || line.startsWith('#')) {
            continue;
        }
        const auto parts = line.split(QRegularExpression{"[\\s,;]+"}, Qt::SkipEmptyParts);
        if (parts[0].compare("banded", Qt::CaseInsensitive) == 0) {
            parsed.banded = parts.size() > 1 && parts[1] != "0" && parts[1].compare("false", Qt::CaseInsensitive) != 0;
            continue;
        }
        bool okValue = false;
        const double value = parts[0].toDouble(&okValue);
        int channel[4] = {0, 0, 0, 255};
        bool okChannels = parts.size() >= 4 && parts.size() <= 5;
        for (int c = 0; okChannels && c < parts.size() - 1; c++) {
            bool ok = false;
            channel[c] = parts[c + 1].toInt(&ok);
            okChannels = ok && channel[c] >= 0 && channel[c] <= 255;
        }
        if (!okValue || !okChannels) {
            error = "line " + std::to_string(lineNumber) + " is not \"value red green blue [alpha]\": " + line.toStdString();
            return false;
        }
        parsed.stops.emplace_back(value, QColor{channel[0], channel[1], channel[2], channel[3]});
    }
    if (parsed.stops.size() < 2) {
        error = "a scale needs at least two stops";
        return false;
    }
    std::stable_sort(parsed.stops.begin(), parsed.stops.end(), [] (const auto& a, const auto& b) { return a.first < b.first; });
    ramp = std::move(parsed);
    return true;
}

// The wind barbs, on a grid of pixels (BarbGrid): the chart's own and the ticked sets, each set from the same origin, the first the longest.
struct BarbsState {
    const Product& product;
    const Grids& grids;
    const Options& options;
    const Sector& sector;
    const QRectF& area;
    const View& view;
    double span;
    QPainter& p;
};

void drawBarbs(BarbsState& state) {
    const Product& product = state.product; const Grids& grids = state.grids; const Options& options = state.options; const Sector& sector = state.sector;
    const QRectF& area = state.area; const View& view = state.view; const double span = state.span; QPainter& p = state.p;
    // wind barbs on a grid of pixels
    if (!options.noBarbs) {
        struct DrawnSet {
            const GfsGrid::Grid * u;
            const GfsGrid::Grid * v;
            QColor color;
        };
        std::vector<DrawnSet> sets;
        if (!product.barbU.empty() && grids.count(product.barbU) && grids.count(product.barbV)) {
            sets.push_back({&grids.at(product.barbU), &grids.at(product.barbV), QColor{20, 20, 20}});
        }
        for (const auto& set : product.barbSets) {
            if (grids.count(set.u) && grids.count(set.v)) {
                sets.push_back({&grids.at(set.u), &grids.at(set.v), set.color});
            }
        }
        const double pixelsPerDegree = area.width() / std::max(0.1, span);
        const auto look = BarbGrid::look(span, pixelsPerDegree);
        for (double lat = std::ceil(std::max(-85.0, sector.south) / look.step) * look.step; !sets.empty() && lat <= std::min(85.0, sector.north); lat += look.step) {
            const double lonStep = BarbGrid::lonStep(look.step, lat);
            for (double lon = std::ceil(sector.west / lonStep) * lonStep; lon <= sector.east; lon += lonStep) {
                const QPointF at = view.toPixel(lon, lat);
                if (!area.contains(at)) {
                    continue;
                }
                // every set from the same origin: the first the longest, each next a little shorter and drawn over it
                for (size_t i = 0; i < sets.size(); i++) {
                    float uu = sets[i].u->sample(lon, lat), vv = sets[i].v->sample(lon, lat);
                    if (std::isnan(uu) || std::isnan(vv)) {
                        uu = sets[i].u->sample(lon + 360.0, lat);
                        vv = sets[i].v->sample(lon + 360.0, lat);
                    }
                    if (std::isnan(uu) || std::isnan(vv)) {
                        continue;
                    }
                    QColor ink = sets[i].color;
                    p.setPen(QPen{QColor{ink.red(), ink.green(), ink.blue(), 230}, sets.size() > 1 ? 1.3 : 1.0});
                    p.setBrush(ink);
                    const double from = std::fmod(std::atan2(-uu, -vv) * 180.0 / pi + 360.0, 360.0);
                    WindBarb::draw(p, at, from, std::hypot(uu, vv) * msToKnots, look.length * (1.0 - 0.12 * static_cast<double>(i)), lat < 0.0);
                }
            }
        }
    }
}

// The numbers printed at points over the fill (Product::pointValues): at the cities, biggest first, then evenly over what is left of the map; never one box on another.
struct PointValuesState {
    const Product& product;
    const Grids& grids;
    const Options& options;
    const Sector& sector;
    const QRectF& area;
    const View& view;
    double span;
    QPainter& p;
    const QFont& font;
    bool us;
};

void drawPointValues(PointValuesState& state) {
    const Product& product = state.product; const Grids& grids = state.grids; const Options& options = state.options; const Sector& sector = state.sector; const QRectF& area = state.area;
    const View& view = state.view; const double span = state.span; QPainter& p = state.p; const QFont& font = state.font; const bool us = state.us;
    // numbers at points ("41 63": the 10th and 90th percentile): at cities, biggest first, then evenly over what is left of the map; never one box on another
    if (!product.pointValues.empty()) {
        bool all = true;
        for (const auto& value : product.pointValues) {
            all = all && grids.count(value.key) > 0;
        }
        if (all) {
            QFont numberFont = font;
            numberFont.setBold(true);
            numberFont.setPointSizeF(std::max(7.0, font.pointSizeF() * 0.9));
            const QFontMetricsF numberMetrics{numberFont};
            const double gap = 5.0;   // between the numbers of one point
            std::vector<QRectF> boxes;
            // one point's numbers side by side, centred on `at`; false (nothing drawn) when no value there or the box would cover another
            const auto place = [&] (const QPointF& at, double lon, double lat) {
                std::vector<QString> texts;
                double width = -gap;
                for (const auto& value : product.pointValues) {
                    const float v = grids.at(value.key).sample(lon, lat);
                    if (std::isnan(v)) {
                        return false;
                    }
                    texts.push_back(QString::number(static_cast<int>(std::lround(shown(product.quantity, v, us)))));
                    width += numberMetrics.horizontalAdvance(texts.back()) + gap;
                }
                const QRectF box{at.x() - width / 2.0 - 3.0, at.y() - numberMetrics.height() / 2.0 - 2.0, width + 6.0, numberMetrics.height() + 4.0};
                if (!area.adjusted(4, 4, -4, -4).contains(box)) {
                    return false;
                }
                for (const auto& other : boxes) {
                    if (other.intersects(box.adjusted(-2, -1, 2, 1))) {
                        return false;
                    }
                }
                boxes.push_back(box);
                if (options.noLabels) {
                    return true;
                }
                p.setPen(Qt::NoPen);
                p.setBrush(QColor{255, 255, 255, 150});
                p.drawRoundedRect(box, 3.0, 3.0);
                double x = at.x() - width / 2.0;
                for (size_t i = 0; i < texts.size(); i++) {
                    QPainterPath path;
                    path.addText(QPointF{x, at.y() + numberMetrics.ascent() / 2.0 - 1.0}, numberFont, texts[i]);
                    p.setPen(QPen{QColor{255, 255, 255, 230}, 2.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin});
                    p.setBrush(Qt::NoBrush);
                    p.drawPath(path);
                    p.setPen(Qt::NoPen);
                    p.setBrush(product.pointValues[i].color);
                    p.drawPath(path);
                    x += numberMetrics.horizontalAdvance(texts[i]) + gap;
                }
                return true;
            };
            // the cities (Options::places, biggest first) that fall inside the view
            std::vector<std::pair<float, float>> inView;
            for (const auto& city : options.places) {
                if (city.first >= sector.west && city.first <= sector.east && city.second >= sector.south && city.second <= sector.north) {
                    inView.push_back(city);
                }
            }
            const double ground = span > 150.0 ? 120.0 : span > 80.0 ? 96.0 : 80.0;   // pixels apart, at the least, so the wider the view the fewer
            std::vector<QPointF> spots;
            for (const auto& city : inView) {
                const auto at = view.toPixel(city.first, city.second);
                bool near = false;
                for (const auto& other : spots) {
                    near = near || std::hypot(at.x() - other.x(), at.y() - other.y()) < ground;
                }
                if (!near && place(at, city.first, city.second)) {
                    spots.push_back(at);
                }
                if (spots.size() >= 60) {
                    break;
                }
            }
            // what the cities left out (oceans, other countries): an even grid, skipping any spot near a number already there
            for (double y = area.top() + ground / 2.0; y < area.bottom(); y += ground) {
                for (double x = area.left() + ground / 2.0; x < area.right(); x += ground) {
                    bool near = false;
                    for (const auto& other : spots) {
                        near = near || std::hypot(x - other.x(), y - other.y()) < ground * 0.8;
                    }
                    if (!near && place({x, y}, view.lonAt(x), view.latAt(y))) {
                        spots.emplace_back(x, y);
                    }
                }
            }
        }
    }
}

// What the stages of the drawing share once the painter exists: the chart, where it is drawn, and the font. A stage function takes a Frame and unpacks only what it uses.
struct Frame {
    const Product& product;
    const Grids& grids;
    const Options& options;
    const Sector& sector;
    const GfsData::Run& run;
    int forecastHour;
    const View& view;
    const QRectF& area;
    double S;
    int margin;
    QImage& image;
    QPainter& p;
    QFont& font;
    const QFontMetricsF& metrics;
    bool us;
    double span;
};

// The place of the largest value of the fill on view (the one the legend marks): a ring and "max" with the number beside it, on the side that has room.
template <class Halo>
void drawMaxMark(Frame& f, const Halo& halo, const QPointF& at, double value) {
    QPainter& p = f.p;
    const QRectF& area = f.area;
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen{QColor{255, 255, 255}, 4.0});
    p.drawEllipse(at, 5.0, 5.0);
    p.setPen(QPen{QColor{20, 20, 20}, 1.8});
    p.drawEllipse(at, 5.0, 5.0);
    const double v = shown(f.product.quantity, value, f.us);
    const QString text = "max " + QString::number(v, 'f', std::abs(v) < 10.0 && f.product.quantity != Quantity::Other ? 1 : 0);
    const double width = text.size() * 8.0 + 8.0;
    const double dx = at.x() + 12.0 + width / 2.0 > area.right() - 4.0 ? -(12.0 + width / 2.0) : 12.0 + width / 2.0;   // to the left of the ring when the right has no room
    const double dy = at.y() - 10.0 < area.top() + 8.0 ? 10.0 : -10.0;
    halo(QPointF{at.x() + dx, at.y() + dy}, text, QColor{20, 20, 20});
}

// The title, the line under it (each name in the color it is drawn in) and the run and valid times at the right.
struct HeaderState {
    const Product& product;
    const GfsData::Run& run;
    int forecastHour;
    QPainter& p;
    QImage& image;
    double S;
    int margin;
    QFont& font;
    bool us;
};

template <class Label>
void drawHeader(HeaderState& state, const Label& drawLabel) {
    const Product& product = state.product; const GfsData::Run& run = state.run; const int forecastHour = state.forecastHour;
    QPainter& p = state.p; QImage& image = state.image; const double S = state.S; const int margin = state.margin; QFont& font = state.font; const bool us = state.us;
    const auto init = QDateTime::fromString(QString::fromStdString(run.id()), "yyyyMMddHH");
    auto initUtc = QDateTime{init.date(), init.time(), QTimeZone::utc()};
    const auto valid = initUtc.addSecs(forecastHour * 3600);
    QFont big = font;
    big.setPixelSize(17);
    QString headline = QString::fromStdString(product.source + "  " + product.label);
    {   // a long title (a chart with several things ticked onto it) gets a smaller type, down to 11, so that it stays clear of the run and valid times at the right; when even that is too long
        // the things ticked on are counted ("with 7 more": the line under it names them), and the end is cut as the last resort
        const double room = image.width() / S - 2.0 * margin - 330.0;
        while (big.pixelSize() > 11 && QFontMetricsF{big}.horizontalAdvance(headline) > room) {
            big.setPixelSize(big.pixelSize() - 1);
        }
        if (QFontMetricsF{big}.horizontalAdvance(headline) > room) {
            if (const auto with = headline.indexOf(", with "); with > 0) {
                QStringList lineAndBarbs, fills;   // (the fill is what colors the map: it stays named, the lines and barbs are counted)
                for (const auto& item : headline.mid(with + 7).split(", ")) {
                    (item.endsWith("(fill)") ? fills : lineAndBarbs) << item;
                }
                headline = headline.left(with) + ", with " + (lineAndBarbs.isEmpty() ? QString{} : QString::number(lineAndBarbs.size()) + " more" + (fills.isEmpty() ? QString{} : " and ")) + fills.join(", ");
            }
            headline = QFontMetricsF{big}.elidedText(headline, Qt::ElideRight, room);
        }
    }
    p.setFont(big);
    p.setPen(QColor{25, 25, 25});
    drawLabel(QPointF{static_cast<double>(margin), 22.0}, headline);
    font.setBold(false);
    p.setFont(font);
    p.setPen(QColor{70, 70, 70});
    const auto unit = unitText(product.quantity, us);
    QString fillTitle = QString::fromStdString(product.fillTitleFor ? product.fillTitleFor(forecastHour) : product.fillTitle);
    if (!unit.isEmpty()) {
        fillTitle += " (" + unit + ")";
    }
    // the line under the title: the fill, then the lines and the barbs, each name in the color it is drawn in ("850mb height (dam, red)" in red), so it reads as a key
    struct Piece {
        QString text;
        QColor color;
    };
    const QColor ink{70, 70, 70};
    std::vector<Piece> pieces;
    pieces.push_back({fillTitle, ink});
    bool anyLines = false;
    for (const auto& set : product.contours) {
        if (set.title.empty()) {
            continue;
        }
        pieces.push_back({anyLines ? ", " : (fillTitle.isEmpty() ? "lines: " : ";  lines: "), ink});
        const auto shade = set.color;
        pieces.push_back({QString::fromStdString(set.title), shade.lightness() > 150 ? shade.darker(150) : shade});
        anyLines = true;
    }
    if (!product.barbU.empty() || !product.barbSets.empty()) {
        if (product.barbSets.empty()) {
            pieces.push_back({";  barbs: kt", ink});
        } else {   // stacked winds: which color is which level
            pieces.push_back({";  barbs, kt: ", ink});
            bool first = true;
            if (!product.barbU.empty()) {
                pieces.push_back({"chart's own (black)", ink});
                first = false;
            }
            for (const auto& set : product.barbSets) {
                if (!first) {
                    pieces.push_back({", ", ink});
                }
                first = false;
                pieces.push_back({QString::fromStdString(set.name + " (" + set.colorName + ")"), set.color.lightness() > 150 ? set.color.darker(150) : set.color});
            }
        }
    }
    {   // the whole line in 11 px when it fits, a smaller type down to 9 when it does not, and the end cut (with an ellipsis) only if that is not enough
        const double room = image.width() / S - 2.0 * margin;
        QFont small = font;
        const auto width = [&pieces] (const QFont& f) {
            double w = 0.0;
            for (const auto& piece : pieces) {
                w += QFontMetricsF{f}.horizontalAdvance(piece.text);
            }
            return w;
        };
        while (small.pixelSize() > 9 && width(small) > room) {
            small.setPixelSize(small.pixelSize() - 1);
        }
        p.setFont(small);
        double x = margin;
        const QFontMetricsF metrics{small};
        for (const auto& piece : pieces) {
            const double w = metrics.horizontalAdvance(piece.text);
            if (x + w > margin + room) {   // the rest does not fit: what still does, and an ellipsis
                p.setPen(ink);
                drawLabel(QPointF{x, 43.0}, metrics.elidedText(piece.text, Qt::ElideRight, std::max(0.0, margin + room - x)));
                break;
            }
            p.setPen(piece.color);
            drawLabel(QPointF{x, 43.0}, piece.text);
            x += w;
        }
        p.setFont(font);
        p.setPen(ink);
    }
    const QString times = "Run " + initUtc.toString("ddd yyyy-MM-dd HH") + "Z   F" + QString::number(forecastHour).rightJustified(3, '0') + "   Valid " + valid.toString("ddd yyyy-MM-dd HH") + "Z";
    drawLabel(QRectF{0, 8, image.width() / S - static_cast<double>(margin), 20}, Qt::AlignRight, times);
}

// What the legend and the keys under the map need of the chart being drawn: references to the render's own values, so the code that draws them can be moved out of render() unchanged.
struct LegendState {
    const Product& product;
    const Grids& grids;
    const Options& options;
    const GfsData::Run& run;
    QPainter& p;
    QImage& image;
    double S;
    const QRectF& area;
    int margin;
    const Ramp& rampRef;
    bool layerKey;
    double deviationLow, deviationHigh;
    double fillMin, fillMax;
    QFont& font;
    const QFontMetricsF& metrics;
    bool us;
};

template <class Label>
void drawLegend(LegendState& state, const Label& drawLabel) {
    const Product& product = state.product; const Grids& grids = state.grids; const Options& options = state.options; const GfsData::Run& run = state.run;
    QPainter& p = state.p; QImage& image = state.image; const double S = state.S; const QRectF& area = state.area; const int margin = state.margin; const Ramp * ramp = &state.rampRef;
    const bool layerKey = state.layerKey; const double deviationLow = state.deviationLow, deviationHigh = state.deviationHigh; const double fillMin = state.fillMin, fillMax = state.fillMax;
    QFont& font = state.font; const QFontMetricsF& metrics = state.metrics; const bool us = state.us;
    // legend: even steps in value, or (legendStep 0) one equal-width block per ramp stop
    const double barLeft = margin + 6.0, barTop = area.bottom() + 12.0, barWidth = area.width() - 12.0, barHeight = 16.0;
    const auto& stops = ramp->stops;
    const double first = stops.front().first, last = stops.back().first;
    const bool stepped = product.legendStep <= 0.0;
    // an export's bar holds only the values the picture or the loop has, the smallest at the left: not for the scales of categories (a step for each stop), which have no range to speak of
    double rangeLow = 0.0, rangeHigh = 0.0;
    bool useRange = false;
    if (options.legendFit && !std::isnan(fillMax)) {
        rangeLow = fillMin;
        rangeHigh = fillMax;
        useRange = true;
    } else if (options.legendRange) {
        rangeLow = options.legendLow;
        rangeHigh = options.legendHigh;
        useRange = true;
    }
    useRange = useRange && rangeHigh > rangeLow && (ramp->banded || !stepped);
    const auto valueAt = [&] (double t) {   // t 0..1 along the bar
        if (useRange) {
            return rangeLow + (rangeHigh - rangeLow) * t;
        }
        if (ramp->banded) {   // one flat block for each band
            return stops[std::min(static_cast<size_t>(t * static_cast<double>(stops.size())), stops.size() - 1)].first;
        }
        if (!stepped) {
            return first + (last - first) * t;
        }
        const double position = t * static_cast<double>(stops.size() - 1);
        const size_t i = std::min(static_cast<size_t>(position), stops.size() - 2);
        return stops[i].first + (stops[i + 1].first - stops[i].first) * (position - static_cast<double>(i));
    };
    const bool noBar = product.linesOnly || layerKey;   // (no scale to show: the lines, or the sets of a paintball, are the chart)
    for (int x = 0; !noBar && x < static_cast<int>(barWidth); x++) {
        const QRgb c = ramp->at(valueAt(x / barWidth));
        // a ramp that begins clear is drawn as the pale ground on the bar
        const int a = qAlpha(c);
        const QColor ground{244, 244, 244};
        p.fillRect(QRectF{barLeft + x, barTop, 1.5, barHeight}, QColor{(qRed(c) * a + ground.red() * (255 - a)) / 255, (qGreen(c) * a + ground.green() * (255 - a)) / 255, (qBlue(c) * a + ground.blue() * (255 - a)) / 255});
    }
    p.setPen(QColor{60, 60, 60});
    p.setBrush(Qt::NoBrush);
    if (layerKey) {   // (the key of the sets is drawn below, with the chance lines)
    } else if (!product.linesOnly) {
        p.drawRect(QRectF{barLeft, barTop, barWidth, barHeight});
    } else if (product.contours.size() > 1) {   // lines of several sets (models compared): a key, a sample of each line's color and what it is
        double x = barLeft;
        for (const auto& set : product.contours) {
            if (set.title.empty()) {   // a line of many alike (a member of fifty) is not in the key
                continue;
            }
            const QString name = QString::fromStdString(set.title);
            const double width = QFontMetricsF{p.font()}.horizontalAdvance(name);
            p.setPen(QPen{set.color, 3.0});
            p.drawLine(QPointF{x, barTop + barHeight / 2.0}, QPointF{x + 26.0, barTop + barHeight / 2.0});
            p.setPen(QColor{60, 60, 60});
            drawLabel(QRectF{x + 32.0, barTop, width + 4.0, barHeight}, Qt::AlignLeft | Qt::AlignVCenter, name);
            x += 32.0 + width + 22.0;
        }
    } else {   // the lines are the chart: a line of explanation where the color bar would be
        drawLabel(QRectF{barLeft, barTop, barWidth, barHeight}, Qt::AlignLeft | Qt::AlignVCenter, QString::fromStdString(product.fillTitle));
    }
    const auto tick = [&] (double t, double value) {
        const double x = barLeft + t * barWidth;
        p.drawLine(QPointF{x, barTop + barHeight}, QPointF{x, barTop + barHeight + 4});
        drawLabel(QRectF{x - 24, barTop + barHeight + 4, 48, 14}, Qt::AlignHCenter, numberText(shown(product.quantity, value, us)));
    };
    if (noBar) {
        // no scale to number
    } else if (ramp->banded) {   // the number at the lower edge of each band, as the model guidance site's legends have it
        // many narrow bands (4 degree ones): a number every few bands so they do not run together; the first stop (-100, "and below") has none
        const size_t every = std::max<size_t>(1, static_cast<size_t>(std::ceil(static_cast<double>(stops.size()) * 40.0 / barWidth)));
        double lastTick = -1000.0;
        for (size_t i = 0; i < stops.size(); i++) {
            if (useRange) {   // the bands inside the range, at their place on a bar of values
                const double t = (stops[i].first - rangeLow) / (rangeHigh - rangeLow);
                if (stops[i].first > -90.0 && t >= 0.0 && t <= 1.0 && t * barWidth - lastTick >= 36.0) {
                    tick(t, stops[i].first);
                    lastTick = t * barWidth;
                }
            } else if (stops[i].first > -90.0 && (i % every == 0 || stops.size() <= 16)) {
                tick(static_cast<double>(i) / static_cast<double>(stops.size()), stops[i].first);
            }
        }
    } else if (stepped) {
        for (size_t i = 0; i < stops.size(); i++) {
            if (i > 0 || stops[i].second.alpha() == 255) {
                tick(static_cast<double>(i) / static_cast<double>(stops.size() - 1), stops[i].first);
            }
        }
    } else {
        // steps are in displayed units: walk them in displayed value and map back
        const double from = useRange ? rangeLow : first, to = useRange ? rangeHigh : last;
        const double lowShown = shown(product.quantity, from, us), highShown = shown(product.quantity, to, us);
        double step = product.legendStep;
        if (useRange) {   // a few numbers across the range: the chart's step made finer when the range is short, coarser when it is long
            for (int i = 0; i < 6 && (highShown - lowShown) / step < 3.0; i++) {
                step /= 2.0;
            }
            for (int i = 0; i < 6 && (highShown - lowShown) / step > 9.0; i++) {
                step *= 2.0;
            }
        }
        for (double value = std::ceil(lowShown / step) * step; value <= highShown + 1e-9; value += step) {
            const double base = (value - lowShown) / (highShown - lowShown);
            tick(base, from + (to - from) * base);
        }
    }
    if (useRange && !noBar) {   // the ends of the bar: the smallest value at the left, the largest at the right
        QFont small = p.font();
        small.setPixelSize(std::max(9, small.pixelSize() - 1));
        small.setBold(true);
        p.setFont(small);
        p.setPen(QColor{60, 60, 60});
        drawLabel(QRectF{barLeft, barTop - 12.0, barWidth / 2.0, 11.0}, Qt::AlignLeft | Qt::AlignVCenter, "min " + numberText(shown(product.quantity, rangeLow, us)));
        drawLabel(QRectF{barLeft + barWidth / 2.0, barTop - 12.0, barWidth / 2.0, 11.0}, Qt::AlignRight | Qt::AlignVCenter, "max " + numberText(shown(product.quantity, rangeHigh, us)));
        p.setFont(font);
    } else if (!noBar && !std::isnan(fillMax)) {   // the largest value of the chart on view: a line through the bar and its number above it
        double at = 0.0;
        for (int x = 0; x < static_cast<int>(barWidth); x++) {   // (the bar's values only rise: the last place that is not above it)
            if (valueAt(x / barWidth) <= fillMax) {
                at = x;
            }
        }
        const double mx = barLeft + at;
        p.setPen(QPen{QColor{150, 0, 0}, 2.0});
        p.drawLine(QPointF{mx, barTop - 3.0}, QPointF{mx, barTop + barHeight});
        QFont small = p.font();
        small.setPixelSize(std::max(9, small.pixelSize() - 1));
        small.setBold(true);
        p.setFont(small);
        const QString label = "max " + numberText(shown(product.quantity, fillMax, us));
        const double width = QFontMetricsF{small}.horizontalAdvance(label) + 4.0;
        const bool flip = mx + width > barLeft + barWidth;
        drawLabel(QRectF{flip ? mx - width - 2.0 : mx + 3.0, barTop - 12.0, width, 11.0}, (flip ? Qt::AlignRight : Qt::AlignLeft) | Qt::AlignVCenter, label);
        p.setFont(font);
    }
    if (deviationHigh > 0.0) {   // a line under the ticks: how big one deviation is here (it is not the same everywhere)
        const auto number = [] (double v) { return QString::number(v, 'f', v < 10.0 ? 1 : 0); };
        p.setPen(QColor{90, 90, 90});
        const QString unitName = QString::fromStdString(product.sigmaUnit);
        const QString low = number(deviationLow), high = number(deviationHigh);
        const QString text = low == high ? "1 std dev = about " + high + " " + unitName
                                         : "1 std dev = " + low + " to " + high + " " + unitName + " over this map (it varies by place; hover for the value here)";
        drawLabel(QRectF{barLeft, barTop + barHeight + 18.0, barWidth, 14}, Qt::AlignLeft, text);
    }
    // a small key for each overlay's colors (the same amounts as the main scale, from light to heavy)
    double keyLeft = barLeft;
    const bool paintball = product.source == "SPCREFS" && !product.overlays.empty() && product.overlays.front().first.rfind("Member ", 0) == 0;
    if (paintball) {   // the members' colors, each with its name (the old REFS screen's key): the first is the chart's own fill
        const auto names = UtilitySpcRefs::memberNames();
        std::vector<QColor> colors{product.ramp.stops.back().second};
        for (const auto& overlay : product.overlays) {
            colors.push_back(overlay.second.stops.back().second);
        }
        const double top = image.height() / S - 15.0;
        const QFontMetricsF metrics{p.font()};
        p.setPen(QColor{70, 70, 70});
        for (size_t i = 0; i < colors.size(); i++) {
            const QString name = i < names.size() ? QString::fromStdString(names[i]) : QString{"Member %1"}.arg(i + 1);
            p.setBrush(QColor{colors[i].red(), colors[i].green(), colors[i].blue()});
            p.setPen(QColor{60, 60, 60});
            p.drawRect(QRectF{keyLeft, top, 9, 9});
            p.setPen(QColor{70, 70, 70});
            drawLabel(QRectF{keyLeft + 13, top - 1, metrics.horizontalAdvance(name) + 4, 12}, Qt::AlignLeft | Qt::AlignVCenter, name);
            keyLeft += 13 + metrics.horizontalAdvance(name) + 14;
        }
        p.setBrush(Qt::NoBrush);
    }
    if (layerKey) {   // each set still drawn, its color and its name
        const double top = barTop + 3.0;
        const QFontMetricsF metrics{p.font()};
        for (const auto& [key, overlayRamp] : product.overlays) {
            if (grids.find(key) == grids.end() || overlayRamp.stops.empty()) {
                continue;
            }
            QString name = QString::fromStdString(key);
            for (const auto& [layerKeyName, layerName] : product.layers) {
                if (layerKeyName == key) {
                    name = QString::fromStdString(layerName);
                }
            }
            name.replace("REFS member ", "REFS ").replace(" (control)", " ctl").replace(" (NSSL)", "").replace("REFS control ", "REFS ctl ");
            const auto& color = overlayRamp.stops.back().second;
            p.setBrush(QColor{color.red(), color.green(), color.blue()});
            p.setPen(QColor{60, 60, 60});
            p.drawRect(QRectF{keyLeft, top, 9, 9});
            p.setPen(QColor{70, 70, 70});
            drawLabel(QRectF{keyLeft + 13, top - 1, metrics.horizontalAdvance(name) + 4, 12}, Qt::AlignLeft | Qt::AlignVCenter, name);
            keyLeft += 13 + metrics.horizontalAdvance(name) + 12;
        }
        p.setBrush(Qt::NoBrush);
        for (const auto& set : product.contours) {   // and a sample of each line there is a name for
            if (set.title.empty()) {
                continue;
            }
            const QString name = QString::fromStdString(set.title);
            p.setPen(QPen{set.color, 2.5});
            p.drawLine(QPointF{keyLeft + 6.0, top + 4.5}, QPointF{keyLeft + 24.0, top + 4.5});
            p.setPen(QColor{70, 70, 70});
            drawLabel(QRectF{keyLeft + 30.0, top - 1, metrics.horizontalAdvance(name) + 4, 12}, Qt::AlignLeft | Qt::AlignVCenter, name);
            keyLeft += 30.0 + metrics.horizontalAdvance(name) + 12;
        }
    }
    for (const auto& [key, overlayRamp] : product.overlays) {
        if (paintball || layerKey) {
            break;
        }
        const QString name = key == "snow" ? "Snow" : key == "mix" ? "Mixed" : QString::fromStdString(key.substr(key.rfind('|') == std::string::npos ? 0 : key.rfind('|') + 1));   // (an overlay's fill is named after its "|")
        const double top = image.height() / S - 15.0, width = 80.0;
        p.setPen(QColor{70, 70, 70});
        const double nameWidth = std::max(44.0, QFontMetricsF{p.font()}.horizontalAdvance(name) + 6.0);   // (an overlay's name is longer than "Snow")
        drawLabel(QRectF{keyLeft, top - 1, nameWidth, 12}, Qt::AlignLeft | Qt::AlignVCenter, name);
        const auto& s = overlayRamp.stops;
        for (int x = 0; x < static_cast<int>(width); x++) {
            const QRgb c = overlayRamp.at(s[1].first + (s.back().first - s[1].first) * x / width);
            p.fillRect(QRectF{keyLeft + nameWidth + x, top, 1.5, 9}, QColor{qRed(c), qGreen(c), qBlue(c)});
        }
        p.setPen(QColor{60, 60, 60});
        p.setBrush(Qt::NoBrush);
        p.drawRect(QRectF{keyLeft + nameWidth, top, width, 9});
        keyLeft += nameWidth + width + 22;
    }
    if (product.id == "nice_weather") {   // the places with no value (outside the blend's grid, or no normal) are left as the map: a key for them
        const double top = image.height() / S - 15.0;
        p.setBrush(QColor{244, 244, 244});
        p.setPen(QColor{60, 60, 60});
        p.drawRect(QRectF{keyLeft, top, 9, 9});
        p.setPen(QColor{70, 70, 70});
        drawLabel(QRectF{keyLeft + 13, top - 1, 60, 12}, Qt::AlignLeft | Qt::AlignVCenter, "No data");
        p.setBrush(Qt::NoBrush);
        keyLeft += 13 + 52;
    }
    p.setPen(QColor{110, 110, 110});
    drawLabel(QRectF{0, image.height() / S - 16.0, image.width() / S - static_cast<double>(margin), 14}, Qt::AlignRight, QString::fromStdString("Data: " + [&product, &run] {
        auto text = sourceLabel(product.source, run.date + run.cycle);
        for (const auto& extra : product.extras) {   // the other models a chart draws from
            if (extra.lag > 0) {   // (an older run of a source that is credited already)
                continue;
            }
            const auto label = extra.credit.empty() ? sourceLabel(extra.source) : extra.credit;
            if (text.find(label) == std::string::npos) {
                text += "; " + label;
            }
        }
        return text;
    }()));
}

// Coastlines and borders from Options::lines, on the map.
void drawCoastlines(Frame& f) {
    const Options& options = f.options;
    const Sector& sector = f.sector;
    const GfsData::Run& run = f.run;
    const View& view = f.view;
    QPainter& p = f.p;
    // coastlines and borders
    p.setPen(QPen{QColor{40, 40, 40, 210}, 0.9});
    for (const auto& line : options.lines) {
        QPainterPath path;
        bool started = false;
        double previous = 0.0;
        for (const auto& pt : line) {
            if (pt.second < sector.south - 3 || pt.second > sector.north + 3) {
                started = false;
                continue;
            }
            // a longitude equivalent to the view's: the lines are -180..180, the view may reach past either
            double lon = pt.first;
            while (lon < sector.west - 25.0) {
                lon += 360.0;
            }
            while (lon >= sector.west + 335.0) {
                lon -= 360.0;
            }
            if (lon > sector.east + 25.0) {   // far off the right edge (the far side of the dateline, say): a line to it would run across the chart
                started = false;
                continue;
            }
            if (started && std::abs(lon - previous) > 180.0) {   // a coast that crosses the date line (Chukotka, Fiji, the Antarctic ring): not a line across the whole chart
                started = false;
            }
            previous = lon;
            const auto at = view.toPixel(lon, pt.second);
            started ? path.lineTo(at) : path.moveTo(at);
            started = true;
        }
        p.drawPath(path);
    }


}

// Streamlines: evenly spaced lines along a wind, over the fill.
void drawStreamlines(Frame& f) {
    const Product& product = f.product;
    const Grids& grids = f.grids;
    const View& view = f.view;
    const QRectF& area = f.area;
    QPainter& p = f.p;
    // streamlines: evenly spaced lines along the wind (the conformal map puts the wind's direction on the screen as it is: east to the right, north up)
    if (!product.streamU.empty() && grids.count(product.streamU) && grids.count(product.streamV)) {
        const auto& su = grids.at(product.streamU);
        const auto& sv = grids.at(product.streamV);
        const double separation = 17.0, testDistance = 8.0, step = 2.5;
        struct Spot {
            QPointF at;
        };
        const double cell = testDistance;
        const int cols = static_cast<int>(area.width() / cell) + 2, rowsN = static_cast<int>(area.height() / cell) + 2;
        std::vector<std::vector<QPointF>> hash(static_cast<size_t>(cols) * static_cast<size_t>(rowsN));
        const auto slot = [&] (const QPointF& pt) { return static_cast<size_t>(static_cast<int>((pt.y() - area.top()) / cell)) * static_cast<size_t>(cols) + static_cast<size_t>(static_cast<int>((pt.x() - area.left()) / cell)); };
        const auto tooClose = [&] (const QPointF& pt, double distance) {
            const int cx = static_cast<int>((pt.x() - area.left()) / cell), cy = static_cast<int>((pt.y() - area.top()) / cell);
            const int reach = static_cast<int>(std::ceil(distance / cell));
            for (int dy = -reach; dy <= reach; dy++) {
                for (int dx = -reach; dx <= reach; dx++) {
                    const int x = cx + dx, y = cy + dy;
                    if (x < 0 || y < 0 || x >= cols || y >= rowsN) {
                        continue;
                    }
                    for (const auto& other : hash[static_cast<size_t>(y) * static_cast<size_t>(cols) + static_cast<size_t>(x)]) {
                        if (std::hypot(other.x() - pt.x(), other.y() - pt.y()) < distance) {
                            return true;
                        }
                    }
                }
            }
            return false;
        };
        const auto wind = [&] (const QPointF& pt, double& ux, double& vy) {   // the unit direction on the screen, false where there is no wind
            const double lon = view.lonAt(pt.x()), lat = view.latAt(pt.y());
            const float uu = su.sample(lon, lat), vv = sv.sample(lon, lat);
            const double speedNow = std::hypot(uu, vv);
            if (std::isnan(uu) || std::isnan(vv) || speedNow < 0.3) {
                return false;
            }
            ux = uu / speedNow;
            vy = -vv / speedNow;
            return true;
        };
        const auto trace = [&] (QPointF from, double sign, std::vector<QPointF>& out) {
            QPointF at = from;
            for (int i = 0; i < 500; i++) {
                double ux, vy;
                if (!wind(at, ux, vy)) {
                    break;
                }
                const QPointF mid = at + QPointF{ux, vy} * (sign * step / 2.0);
                double mx, my;
                if (!wind(mid, mx, my)) {
                    break;
                }
                at += QPointF{mx, my} * (sign * step);
                if (!area.contains(at) || tooClose(at, testDistance)) {
                    break;
                }
                out.push_back(at);
            }
        };
        p.setBrush(Qt::NoBrush);
        const QColor lineColor{25, 25, 25, 215};
        for (double y = area.top() + separation / 2; y < area.bottom(); y += separation) {
            for (double x = area.left() + separation / 2; x < area.right(); x += separation) {
                const QPointF seed{x, y};
                if (tooClose(seed, separation * 0.8)) {
                    continue;
                }
                std::vector<QPointF> forward, backward;
                trace(seed, 1.0, forward);
                trace(seed, -1.0, backward);
                if (forward.size() + backward.size() < 8) {
                    continue;
                }
                std::vector<QPointF> line(backward.rbegin(), backward.rend());
                line.push_back(seed);
                line.insert(line.end(), forward.begin(), forward.end());
                QPainterPath path;
                path.moveTo(line.front());
                for (const auto& pt : line) {
                    path.lineTo(pt);
                    hash[slot(pt)].push_back(pt);
                }
                p.setPen(QPen{lineColor, 1.1});
                p.drawPath(path);
                // an arrowhead at the middle
                const size_t m = line.size() / 2;
                if (m > 2 && m + 2 < line.size()) {
                    const QPointF dir = line[m + 2] - line[m - 2];
                    const double length = std::hypot(dir.x(), dir.y());
                    if (length > 0.1) {
                        const QPointF unit = dir / length, normal{-unit.y(), unit.x()};
                        QPolygonF head;
                        head << line[m] + unit * 4.0 << line[m] - unit * 3.0 + normal * 3.2 << line[m] - unit * 3.0 - normal * 3.2;
                        p.setBrush(lineColor);
                        p.setPen(Qt::NoPen);
                        p.drawPolygon(head);
                        p.setBrush(Qt::NoBrush);
                    }
                }
            }
        }
    }

}

// The storm tracks and their key, the marker of a local area, and the marks along the frame.
template <class Label, class Halo>
void drawTracksMarkerAndTicks(Frame& f, const Label& drawLabel, const Halo& halo) {
    const Options& options = f.options;
    const Sector& sector = f.sector;
    const int forecastHour = f.forecastHour;
    const View& view = f.view;
    const QRectF& area = f.area;
    const double S = f.S;
    QPainter& p = f.p;
    QFont& font = f.font;
    const double span = f.span;
    // the other model's track under it, dashed
    if (!options.trackOther.empty()) {
        QPolygonF other;
        for (const auto& t : options.trackOther) {
            other << view.toPixel(t.lon, t.lat);
        }
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen{QColor{255, 255, 255, 220}, 4.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin});
        p.drawPolyline(other);
        p.setPen(QPen{QColor{20, 90, 220}, 2.0, Qt::DashLine, Qt::RoundCap, Qt::RoundJoin});
        p.drawPolyline(other);
        for (const auto& t : options.trackOther) {
            if (t.hour % 24 == 0) {
                const auto at = view.toPixel(t.lon, t.lat);
                p.setPen(QPen{QColor{20, 90, 220}, 1.5});
                p.setBrush(QColor{20, 90, 220});
                p.drawEllipse(at, 3.0, 3.0);
            }
        }
    }
    // a storm track with the wind radii of the hour shown
    if (!options.track.empty()) {
        const auto destination = [] (double lat, double lon, double bearing, double nm) {
            const double d = nm * 1.852 / 6371.0, b = bearing * pi / 180.0, la = lat * pi / 180.0;
            const double lat2 = std::asin(std::sin(la) * std::cos(d) + std::cos(la) * std::sin(d) * std::cos(b));
            const double lon2 = lon * pi / 180.0 + std::atan2(std::sin(b) * std::sin(d) * std::cos(la), std::cos(d) - std::sin(la) * std::sin(lat2));
            return std::pair{lon2 * 180.0 / pi, lat2 * 180.0 / pi};
        };
        const TrackPoint * now = nullptr;
        for (const auto& t : options.track) {
            if (t.hour == forecastHour) {
                now = &t;
            }
        }
        if (now && options.windRadii) {   // 64 kt over 50 over 34, each quadrant's radius out along its quarter of the circle
            static const QColor colors[3] = {QColor{255, 235, 0, 70}, QColor{255, 140, 0, 90}, QColor{230, 20, 20, 110}};
            for (int row = 0; row < 3; row++) {
                QPolygonF poly;
                bool any = false;
                for (int q = 0; q < 4; q++) {
                    const double r = now->radii[row][q];
                    any = any || r > 0.0;
                    for (int step = 0; step <= 18; step++) {
                        const double bearing = q * 90.0 + step * 5.0;
                        const auto [lon, lat] = destination(now->lat, now->lon, bearing, r);
                        poly << (r > 0.0 ? view.toPixel(lon, lat) : view.toPixel(now->lon, now->lat));
                    }
                }
                if (any) {
                    p.setPen(QPen{colors[row].darker(160), 1.5});
                    p.setBrush(colors[row]);
                    p.drawPolygon(poly);
                }
            }
        }
        QPolygonF line;
        for (const auto& t : options.track) {
            line << view.toPixel(t.lon, t.lat);
        }
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen{QColor{255, 255, 255, 230}, 4.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin});
        p.drawPolyline(line);
        p.setPen(QPen{QColor{20, 20, 20}, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin});
        p.drawPolyline(line);
        QFont small = p.font();
        small.setPixelSize(11);
        small.setBold(true);
        p.setFont(small);
        for (const auto& t : options.track) {
            if (t.hour % 12 != 0 && &t != now) {
                continue;
            }
            const auto at = view.toPixel(t.lon, t.lat);
            const bool current = &t == now;
            p.setPen(QPen{QColor{20, 20, 20}, 1.5});
            p.setBrush(current ? QColor{255, 255, 255} : QColor{20, 20, 20});
            p.drawEllipse(at, current ? 6.0 : 3.5, current ? 6.0 : 3.5);
            if (current || t.hour % 24 == 0) {
                halo(at + QPointF{9.0, -7.0}, QString{"+%1 h  %2 kt  %3 mb"}.arg(t.hour).arg(t.wind).arg(t.pressure), QColor{20, 20, 20});
            }
        }
    }
    if (!options.track.empty() && !options.trackName.empty()) {   // the key of the tracks
        QFont keyFont = p.font();
        keyFont.setPixelSize(12);
        keyFont.setBold(true);
        p.setFont(keyFont);
        const QString first = QString::fromStdString(options.trackName), second = QString::fromStdString(options.trackOtherName);
        const double width = 60.0 + QFontMetricsF{keyFont}.horizontalAdvance(first) + (options.trackOther.empty() ? 0.0 : 40.0 + QFontMetricsF{keyFont}.horizontalAdvance(second));
        const QRectF box{area.right() - width - 8.0, area.top() + 8.0, width, 24.0};
        p.setPen(Qt::NoPen);
        p.setBrush(QColor{255, 255, 255, 215});
        p.drawRoundedRect(box, 4.0, 4.0);
        double x = box.left() + 8.0;
        const double y = box.center().y();
        p.setPen(QPen{QColor{20, 20, 20}, 2.0});
        p.drawLine(QPointF{x, y}, QPointF{x + 22.0, y});
        drawLabel(QPointF{x + 28.0, y + 4.0}, first);
        x += 28.0 + QFontMetricsF{keyFont}.horizontalAdvance(first) + 14.0;
        if (!options.trackOther.empty()) {
            p.setPen(QPen{QColor{20, 90, 220}, 2.0, Qt::DashLine});
            p.drawLine(QPointF{x, y}, QPointF{x + 22.0, y});
            p.setPen(QColor{20, 20, 20});
            drawLabel(QPointF{x + 28.0, y + 4.0}, second);
        }
    }
    p.setClipping(false);
    p.setPen(QPen{QColor{60, 60, 60}, 1.0});
    p.setBrush(Qt::NoBrush);
    p.drawRect(area);
    if (options.hasMarker) {   // the place a local area is round: a ring and its name
        const auto at = view.toPixel(options.markerLon, options.markerLat);
        if (area.contains(at)) {
            p.setClipRect(area);
            p.setPen(QPen{QColor{255, 255, 255}, 4.0});
            p.drawEllipse(at, 6.0, 6.0);
            p.setPen(QPen{QColor{200, 20, 20}, 2.0});
            p.drawEllipse(at, 6.0, 6.0);
            p.drawLine(QPointF{at.x() - 10, at.y()}, QPointF{at.x() + 10, at.y()});
            p.drawLine(QPointF{at.x(), at.y() - 10}, QPointF{at.x(), at.y() + 10});
            if (!options.markerName.empty()) {
                QFont f = p.font();
                f.setBold(true);
                p.setFont(f);
                const QString label = QString::fromStdString(options.markerName);
                p.setPen(QColor{255, 255, 255});
                for (const QPointF d : {QPointF{1, 1}, QPointF{-1, 1}, QPointF{1, -1}, QPointF{-1, -1}}) {
                    drawLabel(at + QPointF{10, -8} + d, label);
                }
                p.setPen(QColor{140, 10, 10});
                drawLabel(at + QPointF{10, -8}, label);
            }
            p.setClipping(false);
        }
    }
    if (options.ticks) {   // latitude and longitude marks along the frame: a step that gives five or six across, numbered along the bottom and the left
        const auto stepFor = [] (double span) {
            static const double steps[] = {0.1, 0.25, 0.5, 1.0, 2.0, 5.0, 10.0, 15.0, 30.0};
            for (const double step : steps) {
                if (span / step <= 7.0) {
                    return step;
                }
            }
            return 30.0;
        };
        const auto numberOf = [] (double v, double step, const char * plus, const char * minus) {
            const double a = std::fabs(v);
            return QString::number(a, 'f', step < 1.0 ? (step < 0.25 ? 2 : 1) : 0) + QString::fromUtf8("\xC2\xB0") + (v >= 0 ? plus : minus);
        };
        QFont f = p.font();
        f.setBold(false);
        f.setPixelSize(11);
        p.setFont(f);
        p.setPen(QColor{40, 40, 40});
        const double lonStep = stepFor(sector.east - sector.west);
        for (double lon = std::ceil(sector.west / lonStep) * lonStep; lon <= sector.east + 1e-9; lon += lonStep) {
            const double x = view.toPixel(lon, sector.south).x();
            if (x < area.left() + 18 || x > area.right() - 18) {
                continue;
            }
            for (const double y : {area.top(), area.bottom()}) {
                const double dir = y == area.top() ? 1.0 : -1.0;
                p.drawLine(QPointF{x, y}, QPointF{x, y + dir * 7.0});
            }
            double shownLon = lon;
            if (shownLon > 180.0) {
                shownLon -= 360.0;
            }
            drawLabel(QRectF{x - 30, area.bottom() - 22, 60, 14}, Qt::AlignHCenter, numberOf(shownLon, lonStep, "E", "W"));
        }
        const double latStep = stepFor(sector.north - sector.south);
        for (double lat = std::ceil(sector.south / latStep) * latStep; lat <= sector.north + 1e-9; lat += latStep) {
            const double y = view.toPixel(sector.west, lat).y();
            if (y < area.top() + 10 || y > area.bottom() - 10) {
                continue;
            }
            for (const double x : {area.left(), area.right()}) {
                const double dir = x == area.left() ? 1.0 : -1.0;
                p.drawLine(QPointF{x, y}, QPointF{x + dir * 7.0, y});
            }
            drawLabel(QRectF{area.left() + 9, y - 7, 60, 14}, Qt::AlignLeft | Qt::AlignVCenter, numberOf(lat, latStep, "N", "S"));
        }
        p.setPen(QPen{QColor{60, 60, 60}, 1.0});
    }

}

// The contour lines of every set, their numbers and the highs and lows; the numbers on the lines are returned, to be drawn after the barbs.
template <class Halo>
std::vector<std::tuple<QPointF, QString, QColor>> drawContours(Frame& f, const Halo& halo, const QImage& map) {
    const Product& product = f.product;
    const Grids& grids = f.grids;
    const Options& options = f.options;
    const Sector& sector = f.sector;
    const View& view = f.view;
    const QRectF& area = f.area;
    const double S = f.S;
    QPainter& p = f.p;
    const double span = f.span;
    // the wider the view, the sparser the lines, the highs and lows, and the barbs
    const double extremeRadius = span > 150.0 ? 12.0 : span > 80.0 ? 8.0 : 6.0;
    std::vector<QRectF> placed;   // where the numbers on the lines already are
    std::vector<std::tuple<QPointF, QString, QColor>> numbersToDraw;   // the numbers on the lines, drawn after the barbs so that no barb is over one
    // where the wind barbs will stand (they are drawn after the lines): the numbers keep clear of them when they can
    const bool barbsShown = !options.noBarbs && ((!product.barbU.empty() && grids.count(product.barbU) && grids.count(product.barbV)) || std::any_of(product.barbSets.begin(), product.barbSets.end(), [&grids] (const auto& set) { return grids.count(set.u) && grids.count(set.v); }));
    const auto barbLook = BarbGrid::look(span, area.width() / std::max(0.1, span));
    const auto nearBarb = [&] (const QPointF& at) {
        const auto [la, lo] = BarbGrid::nearestNode(barbLook.step, view.latAt(at.y()), view.lonAt(at.x()));
        const auto node = view.toPixel(lo, la);
        return std::hypot(node.x() - at.x(), node.y() - at.y()) < barbLook.length * 1.1;
    };
    for (const auto& set : product.contours) {
        const double interval = set.interval * (span > 150.0 ? 2.0 : 1.0);
        GfsGrid::Grid smooth;
        if (set.smoothKm > 0.0) {   // a 3 x 3 average n times spreads as sqrt(2n/3) cells: the passes that reach the radius (at most 60)
            const double cells = set.smoothKm / (std::max(grids.at(set.key).step, 0.001) * 111.0);
            smooth = GfsGrid::smoothed(grids.at(set.key), std::clamp(static_cast<int>(std::lround(1.5 * cells * cells)), 1, 60));
        }
        const GfsGrid::Grid fine = set.smoothKm > 0.0 ? GfsGrid::Grid{} : GfsGrid::reducedForLines(grids.at(set.key));   // a 3 km grid is averaged to about 8 km for its lines
        const auto& grid = set.smoothKm > 0.0 ? smooth : (fine.empty() ? grids.at(set.key) : fine);
        double lo = 1e18, hi = -1e18;
        for (int y = 0; y < map.height(); y += 6) {
            for (int x = 0; x < map.width(); x += 6) {
                const float value = grid.sample(view.lonAt(area.left() + x / S), view.latAt(area.top() + y / S));
                if (!std::isnan(value)) {
                    lo = std::min<double>(lo, value * set.scale);
                    hi = std::max<double>(hi, value * set.scale);
                }
            }
        }
        if (set.onlyBelowZero) {
            hi = std::min(hi, -interval / 2.0);
        }
        for (double level = std::max(std::ceil((lo - set.base) / interval) * interval + set.base, std::ceil((set.minimum - set.base) / interval) * interval + set.base); level <= hi; level += interval) {
            const bool heavy = !set.uniform && std::fmod(std::abs(level - set.base), interval * 5) < 1e-6 || (set.colorBelow.isValid() && std::abs(level - set.split) < 1e-6);
            const QColor color = set.colorBelow.isValid() && level <= set.split ? set.colorBelow : set.color;
            for (const auto& line : GfsGrid::contour(grid, level / set.scale, sector.west, sector.south, sector.east, sector.north)) {
                QPainterPath path;
                bool first = true;
                for (const auto& pt : line) {
                    const auto at = view.toPixel(pt.lon, pt.lat);   // already continuous across west..east
                    first ? path.moveTo(at) : path.lineTo(at);
                    first = false;
                }
                p.setBrush(Qt::NoBrush);
                QPen pen{QColor{color.red(), color.green(), color.blue(), 230}, (heavy ? 1.6 : 1.0) * set.width};
                if (set.dashed) {
                    pen.setStyle(Qt::DashLine);
                }
                if (options.vectorOut != nullptr) {   // handed over: the map draws the line (and its number) at its own zoom
                    VectorLines::Line out;
                    out.color = pen.color();
                    out.width = pen.widthF();
                    out.dashed = set.dashed;
                    for (const auto& pt : line) {
                        out.lonLat.emplace_back(pt.lon, pt.lat);
                    }
                    options.vectorOut->lines.push_back(std::move(out));
                } else {
                    p.setPen(pen);
                    p.drawPath(path);
                }
                // one label near the middle of a line long enough to carry it; not over another number, and, when the line has a free stretch, not over a wind barb
                if (set.labelled && path.length() > 150.0) {
                    QPointF mid = path.pointAtPercent(0.5);
                    const QString text = interval < 1.0 ? QString::number(level, 'f', interval < 0.1 ? 2 : 1) : QString::number(static_cast<int>(std::lround(level)));
                    const auto boxAt = [&text] (const QPointF& at) { return QRectF{at.x() - text.size() * 4.0 - 4.0, at.y() - 9.0, text.size() * 8.0 + 8.0, 18.0}; };
                    const auto free = [&] (const QPointF& at) {
                        const auto b = boxAt(at);
                        return area.adjusted(24, 14, -24, -14).contains(at) && !std::any_of(placed.begin(), placed.end(), [&b] (const QRectF& other) { return other.intersects(b); });
                    };
                    if (barbsShown) {   // along the line from the middle: the first place a number is clear of the others and of the barbs
                        for (const double at : {0.5, 0.4, 0.6, 0.3, 0.7, 0.2, 0.8}) {
                            const auto candidate = path.pointAtPercent(at);
                            if (free(candidate) && !nearBarb(candidate)) {
                                mid = candidate;
                                break;
                            }
                        }
                    }
                    const QRectF box = boxAt(mid);
                    const bool crowded = std::any_of(placed.begin(), placed.end(), [&box] (const QRectF& other) { return other.intersects(box); });   // a number over another is unreadable: skip it
                    if (!crowded && area.adjusted(24, 14, -24, -14).contains(mid)) {
                        placed.push_back(box);
                        if (options.vectorOut != nullptr) {
                            options.vectorOut->labels.push_back({view.lonAt(mid.x()), view.latAt(mid.y()), text, color.darker(130)});
                        } else {
                            numbersToDraw.emplace_back(mid, interval < 1.0 ? QString::number(level, 'f', interval < 0.1 ? 2 : 1) : QString::number(static_cast<int>(std::lround(level))), color.darker(130));   // a fractional interval keeps its decimals
                        }
                    }
                }
            }
        }
        if (set.highsAndLows) {
            for (const auto& e : GfsGrid::extremes(grid, extremeRadius, sector.west, sector.south, sector.east, sector.north)) {
                const auto at = view.toPixel(e.lon, e.lat);
                if (area.adjusted(20, 20, -20, -20).contains(at)) {
                    if (options.vectorOut != nullptr) {
                        options.vectorOut->labels.push_back({e.lon, e.lat, e.high ? "H" : "L", e.high ? QColor{20, 60, 170} : QColor{190, 30, 30}});
                        options.vectorOut->labels.push_back({e.lon, view.latAt(at.y() + 13), QString::number(static_cast<int>(std::lround(e.value * set.scale))), QColor{40, 40, 40}});
                    } else {
                        halo(at, e.high ? "H" : "L", e.high ? QColor{20, 60, 170} : QColor{190, 30, 30});
                        halo(at + QPointF{0, 13}, QString::number(static_cast<int>(std::lround(e.value * set.scale))), QColor{40, 40, 40});
                    }
                }
            }
        }
    }

    return numbersToDraw;
}

// What the stages that make the map need: the chart, where it is drawn and the view.
struct MapFrame {
    const Product& product;
    const Grids& grids;
    const Options& options;
    const Sector& sector;
    const GfsData::Run& run;
    int forecastHour;
    const View& view;
    const QRectF& area;
    double S;
    QImage& image;
    bool us;
};

// The fields of the chart (and the winds, and the lines for a vector export) kept in Options::probe for the hover read-out.
void fillProbe(MapFrame& f, const GfsGrid::Grid& fill, const std::string& windU, const std::string& windV) {
    const Product& product = f.product;
    const Grids& grids = f.grids;
    const Options& options = f.options;
    const Sector& sector = f.sector;
    const GfsData::Run& run = f.run;
    const int forecastHour = f.forecastHour;
    const QRectF& area = f.area;
    const double S = f.S;
    QImage& image = f.image;
    const bool us = f.us;
    if (options.probe) {   // the fields of this chart, kept for the hover read-out
        auto& probe = *options.probe;
        probe = {};
        probe.west = sector.west;
        probe.east = sector.east;
        probe.south = sector.south;
        probe.north = sector.north;
        probe.areaLeft = area.left() * S;
        probe.areaTop = area.top() * S;
        probe.areaWidth = area.width() * S;
        probe.areaHeight = area.height() * S;
        probe.imageWidth = image.width();
        probe.imageHeight = image.height();
        probe.us = us;
        probe.validUtc = QDateTime::fromString(QString::fromStdString(run.id()), "yyyyMMddHH").addSecs(static_cast<qint64>(forecastHour) * 3600);
        probe.validUtc.setTimeZone(QTimeZone::utc());
        if (!product.linesOnly) {
            QString title = QString::fromStdString(product.fillTitleFor ? product.fillTitleFor(forecastHour) : product.fillTitle);
            if (const auto unit = unitText(product.quantity, us); !unit.isEmpty()) {
                title += " (" + unit + ")";
            }
            probe.fields.push_back({title.toStdString(), GfsGrid::reducedTo(GfsGrid::cropped(fill, sector.west, sector.south, sector.east, sector.north), 120000), 1.0, product.quantity, true});
        }
        for (const auto& [key, overlayRamp] : product.overlays) {   // the fills ticked onto the chart (each its own read-out line)
            const auto found = grids.find(key);
            const auto bar = key.find("fill|");
            if (found == grids.end() || bar == std::string::npos) {   // (the key may carry the prefix of the model it is of)
                continue;
            }
            std::string name = key.substr(bar + 5);
            auto quantity = Quantity::Other;
            if (const auto c = name.rfind(" (C)"); c != std::string::npos && c + 4 == name.size()) {   // a temperature: shown in the user's units like the chart's own
                name.erase(c);
                quantity = Quantity::Temperature;
                if (const auto unit = unitText(quantity, us); !unit.isEmpty()) {
                    name += " (" + unit.toStdString() + ")";
                }
            }
            probe.fields.push_back({name, GfsGrid::reducedTo(GfsGrid::cropped(found->second, sector.west, sector.south, sector.east, sector.north), 120000), 1.0, quantity, quantity != Quantity::Other});
        }
        for (const auto& set : product.contours) {
            probe.fields.push_back({set.title.empty() ? set.key : set.title, GfsGrid::reducedTo(GfsGrid::cropped(grids.at(set.key), sector.west, sector.south, sector.east, sector.north), 120000), set.scale, Quantity::Other, false});
        }
        for (const auto& value : product.pointValues) {
            if (grids.count(value.key)) {
                probe.fields.push_back({value.title.empty() ? value.key : value.title, GfsGrid::reducedTo(GfsGrid::cropped(grids.at(value.key), sector.west, sector.south, sector.east, sector.north), 120000), 1.0, product.quantity, true});
            }
        }
        for (const auto& value : product.hoverValues) {
            if (grids.count(value.key)) {
                probe.fields.push_back({value.title, GfsGrid::reducedTo(GfsGrid::cropped(grids.at(value.key), sector.west, sector.south, sector.east, sector.north), 120000), 1.0, Quantity::Other, false, value.names});
            }
        }
        if (options.vectorOut != nullptr) {   // the ticked winds, for a layer that draws the barbs itself
            options.vectorOut->ownBarbs = !product.barbU.empty() && grids.count(product.barbU) && grids.count(product.barbV);
            size_t index = options.vectorOut->ownBarbs ? 1 : 0;
            for (const auto& set : product.barbSets) {
                if (grids.count(set.u) && grids.count(set.v)) {
                    options.vectorOut->windSets.push_back({GfsGrid::reducedTo(GfsGrid::cropped(grids.at(set.u), sector.west, sector.south, sector.east, sector.north), 120000),
                                                           GfsGrid::reducedTo(GfsGrid::cropped(grids.at(set.v), sector.west, sector.south, sector.east, sector.north), 120000), set.color,
                                                           1.0 - 0.12 * static_cast<double>(index)});
                    index++;
                }
            }
        }
        if (!windU.empty() && grids.count(windU) && grids.count(windV)) {
            probe.windU = GfsGrid::reducedTo(GfsGrid::cropped(grids.at(windU), sector.west, sector.south, sector.east, sector.north), 120000);
            probe.windV = GfsGrid::reducedTo(GfsGrid::cropped(grids.at(windV), sector.west, sector.south, sector.east, sector.north), 120000);
        }
    }
}

// What the fill makes: the map in color and the largest and smallest values on view (those the picture shows, and any).
struct FillResult {
    QImage map;
    float fillMax, fillMin, anyMax, anyMin;
    QPointF maxAt;   // where the largest value the picture shows is, in the picture's own pixels (1x)
};

// The fill, a pixel at a time (the ramp is in the grid's units; fully clear parts show the pale ground).
FillResult paintFill(MapFrame& f, const GfsGrid::Grid& fill, const Ramp * ramp) {
    const View& view = f.view;
    const QRectF& area = f.area;
    const double S = f.S;
    QImage map(static_cast<int>(std::lround(area.width() * S)), static_cast<int>(std::lround(area.height() * S)), QImage::Format_ARGB32_Premultiplied);
    map.fill(QColor{244, 244, 244});
    float fillMax = std::nanf(""), fillMin = std::nanf(""), anyMax = std::nanf(""), anyMin = std::nanf("");   // the largest and smallest value on view: marked on the legend
    QPointF maxAt;
    for (int y = 0; y < map.height(); y++) {
        auto * row = reinterpret_cast<QRgb *>(map.scanLine(y));
        const double lat = view.latAt(area.top() + (y + 0.5) / S);
        for (int x = 0; x < map.width(); x++) {
            const float value = fill.sample(view.lonAt(area.left() + (x + 0.5) / S), lat);
            if (!std::isnan(value)) {
                if (std::isnan(anyMax) || value > anyMax) {
                    anyMax = value;
                }
                if (std::isnan(anyMin) || value < anyMin) {
                    anyMin = value;
                }
                const QRgb c = ramp->at(value);
                if (qAlpha(c) >= 128) {   // (what the picture shows: the clear part of a scale, a reflectivity under 5 dBZ, is not in the range)
                    if (std::isnan(fillMax) || value > fillMax) {
                        fillMax = value;
                        maxAt = QPointF{area.left() + (x + 0.5) / S, area.top() + (y + 0.5) / S};
                    }
                    if (std::isnan(fillMin) || value < fillMin) {
                        fillMin = value;
                    }
                }
                const int a = qAlpha(c);
                if (a == 255) {
                    row[x] = qPremultiply(c);
                } else if (a > 0) {   // a ramp that fades in over the ground
                    const QRgb under = row[x];
                    row[x] = qRgb((qRed(c) * a + qRed(under) * (255 - a)) / 255, (qGreen(c) * a + qGreen(under) * (255 - a)) / 255, (qBlue(c) * a + qBlue(under) * (255 - a)) / 255);
                }
            }
        }
    }
    return {std::move(map), fillMax, fillMin, anyMax, anyMin, maxAt};
}

// The fills painted over the first where they have a value (the snow and the mixed precipitation over the rain; the ticked fills).
void paintOverlays(MapFrame& f, QImage& map) {
    const Product& product = f.product;
    const Grids& grids = f.grids;
    const View& view = f.view;
    const QRectF& area = f.area;
    const double S = f.S;
    for (const auto& [key, overlayRamp] : product.overlays) {
        const auto found = grids.find(key);
        if (found == grids.end()) {
            continue;
        }
        for (int y = 0; y < map.height(); y++) {
            auto * row = reinterpret_cast<QRgb *>(map.scanLine(y));
            const double lat = view.latAt(area.top() + (y + 0.5) / S);
            for (int x = 0; x < map.width(); x++) {
                const float value = found->second.sample(view.lonAt(area.left() + (x + 0.5) / S), lat);
                if (std::isnan(value)) {
                    continue;
                }
                const QRgb c = overlayRamp.at(value);
                const int a = qAlpha(c);
                if (a > 0) {
                    const QRgb under = qUnpremultiply(row[x]);
                    row[x] = qRgb((qRed(c) * a + qRed(under) * (255 - a)) / 255, (qGreen(c) * a + qGreen(under) * (255 - a)) / 255, (qBlue(c) * a + qBlue(under) * (255 - a)) / 255);
                }
            }
        }
    }
}

// The chart as it is to be drawn: the user's scales put in, the grids closed round the date line and the chart's own fields derived, and the fill (the override of a maximum or a change replacing the chart's).
struct Prepared {
    Product product;
    Grids grids;
    GfsGrid::Grid fill;
};

Prepared prepare(const Product& drawn, const Grids& fetched, const GfsData::Run& run, int forecastHour, const Options& options) {
    Product product = drawn;
    product.ramp = defaultRamp(drawn, options.magColors);
    if (const auto found = options.palettes.find(rampKey(product.ramp)); found != options.palettes.end()) {   // the user's own scale for this kind of chart
        product.ramp = found->second;
    }
    for (auto& [key, overlayRamp] : product.overlays) {   // the fills ticked onto the chart: the user's scale for the same default, with the tint and the start the overlay has
        if (key.compare(0, 2, "o_") != 0 || overlayRamp.stops.empty()) {
            continue;
        }
        if (const auto found = options.palettes.find(rampKey(overlayRamp)); found != options.palettes.end()) {
            overlayRamp = asFillOverlay(found->second, overlayRamp);
        }
    }
    if (product.ramp.stops.empty()) {   // a chart of lines only has no scale: a clear one so nothing reads an empty list
        product.ramp.stops = {{0.0, QColor{0, 0, 0, 0}}, {1.0, QColor{0, 0, 0, 0}}};
    }
    Grids grids = fetched;
    for (auto& entry : grids) {
        GfsGrid::closeWrapGap(entry.second);
    }
    if (product.derive) {
        product.derive(grids, Context{forecastHour, run, options.climate, product.source});
    }
    if (!product.layers.empty()) {   // a layer taken out leaves its lines off the chart, not the chart empty
        std::erase_if(product.contours, [&grids] (const ContourSet& set) { return grids.find(set.key) == grids.end(); });
    }
    auto fill = product.fill ? product.fill(grids) : GfsGrid::Grid{};
    if (!options.fillOverride.empty()) {   // a maximum or a change: the fill is made, and only it is drawn
        fill = options.fillOverride;
        const auto title = product.fillTitleFor ? product.fillTitleFor(forecastHour) : product.fillTitle;
        product.fillTitle = options.overrideNote + (title.empty() ? "" : ": " + title);
        product.fillTitleFor = nullptr;
        product.contours.clear();
        product.barbU.clear();
        product.barbV.clear();
        product.barbSets.clear();
        product.streamU.clear();
        product.streamV.clear();
        product.overlays.clear();
        product.linesOnly = false;
        if (!options.overrideRamp.stops.empty()) {
            product.ramp = options.overrideRamp;
            product.palette.clear();
        }
        product.quantity = options.overrideQuantity;
    }
    if (product.linesOnly && !product.contours.empty()) {   // nothing to fill: a blank grid of the first line's field
        for (const auto& set : product.contours) {   // (the first line still there: a member taken out leaves the others)
            const auto found = grids.find(set.key);
            if (found != grids.end()) {
                fill = found->second;
                std::fill(fill.values.begin(), fill.values.end(), std::nanf(""));
                break;
            }
        }
    }
    return {std::move(product), std::move(grids), std::move(fill)};
}

QImage render(const Product& drawn, const Sector& sector, const Grids& fetched, const GfsData::Run& run, int forecastHour, const Options& options) {
    const bool timing = std::getenv("WXQT_TIMING") != nullptr;   // WXQT_TIMING=1: the seconds each stage of the drawing takes, to stderr
    auto lapAt = std::chrono::steady_clock::now();
    const auto lap = [&] (const char * what) {
        if (timing) {
            const auto now = std::chrono::steady_clock::now();
            std::fprintf(stderr, "render %-14s %.2f s\n", what, std::chrono::duration<double>(now - lapAt).count());
            lapAt = now;
        }
    };
    auto prepared = prepare(drawn, fetched, run, forecastHour, options);
    Product& product = prepared.product;
    Grids& grids = prepared.grids;
    auto& fill = prepared.fill;
    if (fill.empty()) {
        return {};
    }
    // the wind the hover and a map layer take: the chart's own, else the first ticked
    const std::string windU = !product.barbU.empty() ? product.barbU : (product.barbSets.empty() ? std::string{} : product.barbSets.front().u);
    const std::string windV = !product.barbU.empty() ? product.barbV : (product.barbSets.empty() ? std::string{} : product.barbSets.front().v);
    if (options.fillOut != nullptr) {
        *options.fillOut = fill;
        if (options.windUOut != nullptr && options.windVOut != nullptr && !windU.empty() && grids.count(windU) && grids.count(windV)) {
            *options.windUOut = grids.at(windU);
            *options.windVOut = grids.at(windV);
        }
        if (options.contoursOut != nullptr) {
            for (const auto& set : product.contours) {
                if (const auto found = grids.find(set.key); found != grids.end()) {
                    options.contoursOut->push_back({set, found->second});
                }
            }
        }
        if (options.rampOut != nullptr) {
            *options.rampOut = product.ramp;
        }
        return QImage{1, 1, QImage::Format_ARGB32_Premultiplied};
    }
    for (const auto& set : product.contours) {
        if (grids.find(set.key) == grids.end()) {
            return {};
        }
    }
    const bool us = options.fahrenheit;
    // a chart in standard deviations says how big one deviation is in the plain unit over the view: it differs from place to place, so a range (the 10th to 90th percentile), not a number
    double deviationLow = 0.0, deviationHigh = 0.0;
    if (!product.sigmaSizeKey.empty() && grids.count(product.sigmaSizeKey) && !product.linesOnly) {
        auto values = GfsGrid::cropped(grids.at(product.sigmaSizeKey), sector.west, sector.south, sector.east, sector.north).values;
        values.erase(std::remove_if(values.begin(), values.end(), [] (float v) { return !std::isfinite(v); }), values.end());
        if (values.size() >= 10) {
            std::sort(values.begin(), values.end());
            deviationLow = values[values.size() / 10];
            deviationHigh = values[values.size() * 9 / 10];
        }
    }
    const bool layerKey = !product.layers.empty() && !product.overlays.empty();   // (a paintball of sets: the key of their colors is where the color bar would be)
    const int headerHeight = 54, legendHeight = 62 + (deviationHigh > 0.0 ? 14 : 0), margin = 8;
    const QRectF mapArea{static_cast<double>(margin), static_cast<double>(headerHeight), static_cast<double>(options.width - 2 * margin), 0.0};
    const double mapHeight = std::clamp(View::heightFor(sector, mapArea.width()), 200.0, 1400.0);
    // a larger picture of the same design: everything is placed in the 1x picture's pixels, the painter scales it, the fill is computed at the final pixels. A tall area at a large scale is a
    // great many pixels (a picture of South America 6600 wide is 60 million, 650 MB while it is made): the scale is held to about 40 million pixels
    const double S = std::clamp(options.scale, 1.0, std::max(1.0, std::sqrt(40e6 / (static_cast<double>(options.width) * (headerHeight + mapHeight + legendHeight)))));
    const QRectF area{mapArea.left(), mapArea.top(), mapArea.width(), mapHeight};
    View view{sector, area};
    QImage image(static_cast<int>(std::lround(options.width * S)), static_cast<int>(std::lround((headerHeight + mapHeight + legendHeight) * S)), QImage::Format_ARGB32_Premultiplied);
    image.fill(QColor{250, 250, 250});

    MapFrame mapFrame{product, grids, options, sector, run, forecastHour, view, area, S, image, us};
    fillProbe(mapFrame, fill, windU, windV);
    const Ramp * ramp = &product.ramp;
    auto made = paintFill(mapFrame, fill, ramp);
    QImage map = std::move(made.map);
    float fillMax = made.fillMax, fillMin = made.fillMin, anyMax = made.anyMax, anyMin = made.anyMin;
    lap("fill");
    paintOverlays(mapFrame, map);
    QPainter p{&image};
    p.setRenderHint(QPainter::Antialiasing);
    p.scale(S, S);
    p.drawImage(area, map);
    p.setClipRect(area);

    lap("overlays");
    QFont font = p.font();
    font.setPixelSize(11);
    font.setBold(true);
    p.setFont(font);
    // text goes through here: with Options::noLabels none is drawn (a picture of the colors, lines and barbs alone)
    const auto drawLabel = [&] (auto&&... args) {
        if (!options.noLabels) {
            p.drawText(std::forward<decltype(args)>(args)...);
        }
    };
    const QFontMetricsF metrics{font};
    const auto halo = [&] (const QPointF& at, const QString& text, const QColor& color) {   // a label with a light edge so it reads over the colors
        if (options.noLabels) {
            return;
        }
        const QPointF origin{at.x() - metrics.horizontalAdvance(text) / 2.0, at.y() + metrics.ascent() / 2.0 - 1};
        QPainterPath path;
        path.addText(origin, font, text);
        p.setPen(QPen{QColor{255, 255, 255, 220}, 3.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin});
        p.setBrush(Qt::NoBrush);
        p.drawPath(path);
        p.setPen(Qt::NoPen);
        p.setBrush(color);
        p.drawPath(path);
    };
    const double span = sector.east - sector.west;
    Frame frame{product, grids, options, sector, run, forecastHour, view, area, S, margin, image, p, font, metrics, us, span};
    drawCoastlines(frame);
    drawStreamlines(frame);
    lap("coastlines");
    const auto numbersToDraw = drawContours(frame, halo, map);
    lap("contours");
    {
        BarbsState barbs{product, grids, options, sector, area, view, span, p};
        drawBarbs(barbs);
    }
    for (const auto& [at, text, color] : numbersToDraw) {   // (over the barbs, with their halo: a number is read before a barb is)
        halo(at, text, color);
    }
    if (options.markMax && !std::isnan(fillMax) && !product.linesOnly) {
        drawMaxMark(frame, halo, made.maxAt, fillMax);
    }
    {
        PointValuesState values{product, grids, options, sector, area, view, span, p, font, us};
        drawPointValues(values);
    }
    drawTracksMarkerAndTicks(frame, drawLabel, halo);
    {   // header
        HeaderState header{product, run, forecastHour, p, image, S, margin, font, us};
        drawHeader(header, drawLabel);
    }

    if (std::isnan(fillMax)) {   // nothing drawn in a color: the range of what there is
        fillMax = anyMax;
        fillMin = anyMin;
    }
    if (options.probe && !std::isnan(fillMax)) {   // (the export's legend and a loop's range read these)
        options.probe->hasRange = true;
        options.probe->fillMin = fillMin;
        options.probe->fillMax = fillMax;
    }
    {   // the legend and the key under the map
        LegendState legend{product, grids, options, run, p, image, S, area, margin, *ramp, layerKey, deviationLow, deviationHigh, fillMin, fillMax, font, metrics, us};
        drawLegend(legend, drawLabel);
    }
    return image;
}
}

// a rough outline of the contiguous United States (longitude, latitude), the Great Lakes inside it: what the summary counts (the blend's grid reaches into Canada, Mexico and the sea)
bool GfsChart::insideUnitedStates(double lon, double lat) {
    static const std::vector<std::array<double, 2>> outline{
        {-124.7, 48.4}, {-123.3, 49.0}, {-95.2, 49.0}, {-95.2, 49.4}, {-94.6, 48.7}, {-91.5, 48.1}, {-89.6, 48.0}, {-84.8, 46.9}, {-83.0, 46.0}, {-82.4, 43.0}, {-83.1, 42.0}, {-82.7, 41.5},
        {-79.0, 42.8}, {-76.8, 43.6}, {-75.0, 45.0}, {-71.5, 45.0}, {-70.2, 46.7}, {-69.2, 47.45}, {-67.8, 47.1}, {-67.0, 44.8}, {-70.0, 43.7}, {-70.7, 42.7}, {-70.0, 41.8}, {-71.5, 41.3},
        {-73.7, 40.9}, {-74.0, 40.4}, {-74.9, 38.9}, {-75.5, 38.0}, {-76.0, 36.9}, {-75.5, 35.3}, {-77.0, 34.6}, {-78.5, 33.8}, {-79.9, 32.7}, {-81.3, 31.5}, {-81.4, 30.4}, {-80.5, 28.5},
        {-80.0, 26.8}, {-80.2, 25.5}, {-81.1, 25.1}, {-81.8, 26.1}, {-82.8, 27.8}, {-82.7, 29.2}, {-83.7, 29.9}, {-85.3, 29.6}, {-86.5, 30.4}, {-88.0, 30.3}, {-89.4, 30.2}, {-89.4, 29.0},
        {-91.0, 29.2}, {-93.8, 29.7}, {-94.8, 29.2}, {-96.4, 28.3}, {-97.3, 27.2}, {-97.2, 25.95}, {-99.1, 26.4}, {-99.5, 27.5}, {-100.7, 29.1}, {-101.4, 29.8}, {-102.4, 29.8}, {-103.1, 29.0},
        {-104.5, 29.7}, {-104.9, 30.6}, {-106.5, 31.75}, {-108.2, 31.8}, {-108.2, 31.33}, {-111.1, 31.33}, {-114.8, 32.5}, {-117.1, 32.5}, {-117.3, 33.1}, {-118.4, 33.7}, {-119.3, 34.1},
        {-120.6, 34.5}, {-121.9, 36.6}, {-122.5, 37.7}, {-123.7, 39.0}, {-124.4, 40.4}, {-124.1, 41.9}, {-124.5, 43.0}, {-124.0, 46.2}};
    bool inside = false;
    for (size_t i = 0, j = outline.size() - 1; i < outline.size(); j = i++) {
        const auto& a = outline[i];
        const auto& b = outline[j];
        if ((a[1] > lat) != (b[1] > lat) && lon < (b[0] - a[0]) * (lat - a[1]) / (b[1] - a[1]) + a[0]) {
            inside = !inside;
        }
    }
    return inside;
}

