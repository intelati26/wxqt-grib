// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "space/SatellitePasses.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <QDateTime>
#include <QStringList>
#include "objects/URL.h"
#include "settings/Location.h"
#include "tropical/UtilityCira.h"
#include "util/PermanentCache.h"
#include "util/Utility.h"

namespace SatellitePasses {
    const vector<Satellite>& satellites() {
        static const vector<Satellite> all{
            {"NOAA-20", 43013, {{"VIIRS", Kind::Imagery, 1530, 0, 0, "true color, infrared, day / night band (3060 km swath)"}, {"ATMS", Kind::Microwave, 1300, 0, 0, "microwave temperature and moisture soundings (2600 km)"}}},
            {"NOAA-21", 54234, {{"VIIRS", Kind::Imagery, 1530, 0, 0, "true color, infrared, day / night band (3060 km swath)"}, {"ATMS", Kind::Microwave, 1300, 0, 0, "microwave temperature and moisture soundings (2600 km)"}}},
            {"Suomi NPP", 37849, {{"VIIRS", Kind::Imagery, 1530, 0, 0, "true color, infrared, day / night band (3060 km swath)"}, {"ATMS", Kind::Microwave, 1300, 0, 0, "microwave temperature and moisture soundings (2600 km)"}}},
            {"Terra", 25994, {{"MODIS", Kind::Imagery, 1165, 0, 0, "true color and the science products (2330 km swath)"}}},
            {"Aqua", 27424, {{"MODIS", Kind::Imagery, 1165, 0, 0, "true color and the science products (2330 km swath)"}}},
            {"Metop-B", 38771, {{"ASCAT", Kind::Wind, 900, 350, 0, "ocean surface wind speed and direction (two 550 km swaths, a gap under the satellite)"}, {"AVHRR", Kind::Imagery, 1447, 0, 0, "infrared and visible imagery (2900 km)"}, {"MHS", Kind::Microwave, 1090, 0, 0, "microwave humidity sounder (2180 km)"}}},
            {"Metop-C", 43689, {{"ASCAT", Kind::Wind, 900, 350, 0, "ocean surface wind speed and direction (two 550 km swaths, a gap under the satellite)"}, {"AVHRR", Kind::Imagery, 1447, 0, 0, "infrared and visible imagery (2900 km)"}, {"MHS", Kind::Microwave, 1090, 0, 0, "microwave humidity sounder (2180 km)"}}},
            {"Metop-SG A1", 65159, {{"METimage", Kind::Imagery, 1335, 0, 0, "visible and infrared imager, 500 m: the next AVHRR (2670 km swath)"}, {"MWS", Kind::Microwave, 1125, 0, 0, "microwave sounder, temperature and moisture (about 2250 km)"}, {"SCA", Kind::Wind, 700, 350, 0, "ocean surface wind, the next ASCAT (two swaths of about 650 km: nominal figures)"}}},
            {"Sentinel-3A", 41335, {{"OLCI", Kind::Imagery, 635, 0, 0, "ocean and land color, 300 m (1270 km swath)"}, {"SLSTR", Kind::Imagery, 700, 0, 0, "sea and land surface temperature, thermal infrared (1400 km)"}}},
            {"Sentinel-3B", 43437, {{"OLCI", Kind::Imagery, 635, 0, 0, "ocean and land color, 300 m (1270 km swath)"}, {"SLSTR", Kind::Imagery, 700, 0, 0, "sea and land surface temperature, thermal infrared (1400 km)"}}},
            {"GCOM-W1", 38337, {{"AMSR2", Kind::Microwave, 725, 0, 0, "microwave imager: rain, wind, sea surface temperature (1450 km)"}}},
            {"GPM", 39574, {{"GMI", Kind::Microwave, 440, 0, 0, "microwave imager, the reference for rain in a storm (885 km)"}}},
            {"DMSP F16", 28054, {{"SSMIS", Kind::Microwave, 850, 0, 0, "microwave imager: the 85-91 GHz eye pictures (1700 km)"}}},
            {"DMSP F17", 29522, {{"SSMIS", Kind::Microwave, 850, 0, 0, "microwave imager: the 85-91 GHz eye pictures (1700 km)"}}},
            {"DMSP F18", 35951, {{"SSMIS", Kind::Microwave, 850, 0, 0, "microwave imager: the 85-91 GHz eye pictures (1700 km)"}}},
            {"RCM-1", 44322, {{"SAR", Kind::Radar, 700, 250, 0, "C band radar: where it can look (steered to either side); only some are scheduled, see NOAA STAR's list"}}},
            {"RCM-2", 44323, {{"SAR", Kind::Radar, 700, 250, 0, "C band radar: where it can look (steered to either side); only some are scheduled, see NOAA STAR's list"}}},
            {"RCM-3", 44324, {{"SAR", Kind::Radar, 700, 250, 0, "C band radar: where it can look (steered to either side); only some are scheduled, see NOAA STAR's list"}}},
            {"Sentinel-1C", 62261, {{"SAR", Kind::Radar, 700, 250, 1, "C band radar looking to the right of the track; only some are scheduled, see NOAA STAR's list"}}},
            {"Sentinel-1D", 66315, {{"SAR", Kind::Radar, 700, 250, 1, "C band radar looking to the right of the track; only some are scheduled, see NOAA STAR's list"}}},
        };
        return all;
    }

    string kindName(Kind kind) {
        return kind == Kind::Imagery ? "Imagery" : kind == Kind::Microwave ? "Microwave" : kind == Kind::Wind ? "Wind (scatterometer)" : "Radar (SAR)";
    }

    SatelliteOrbit::Tle elements(int catalog, string& problem) {
        const PermanentCache store{"sat"};
        const string name = "tle-" + std::to_string(catalog) + ".txt";
        string text = store.read(name);
        long long fetched = 0;
        if (!text.empty() && text.compare(0, 8, "FETCHED ") == 0) {
            fetched = std::atoll(text.c_str() + 8);
        }
        const long long now = QDateTime::currentSecsSinceEpoch();
        if (text.empty() || now - fetched > 12 * 3600) {
            const auto bytes = URL::getText("https://celestrak.org/NORAD/elements/gp.php?CATNR=" + std::to_string(catalog) + "&FORMAT=tle");
            if (bytes.size() > 100 && bytes.find("\n1 ") != string::npos) {
                text = "FETCHED " + std::to_string(now) + "\n" + bytes;
                store.write(name, text);
            } else if (text.empty()) {
                problem = "no element set for satellite " + std::to_string(catalog) + " from CelesTrak";
            }
        }
        const auto all = SatelliteOrbit::parseAll(text.substr(text.find('\n') == string::npos ? 0 : text.find('\n') + 1));
        return all.empty() ? SatelliteOrbit::Tle{} : all.front();
    }

    vector<Place> own() {
        vector<Place> out;
        for (const auto& line : QString::fromStdString(Utility::readPref("SATPASS_PLACES", "")).split(';', Qt::SkipEmptyParts)) {
            const auto f = line.split('|');
            if (f.size() >= 3) {
                out.push_back({f[0].toStdString(), f[1].toDouble(), f[2].toDouble(), false, {}});
            }
        }
        return out;
    }

    void savePlaces(const vector<Place>& places) {
        QStringList parts;
        for (const auto& p : places) {
            parts << QString::fromStdString(p.name).remove('|').remove(';') + "|" + QString::number(p.lat, 'f', 4) + "|" + QString::number(p.lon, 'f', 4);
        }
        Utility::writePref("SATPASS_PLACES", parts.join(';').toStdString());
    }

    vector<Place> places() {
        auto out = own();
        const auto names = Location::listOfNames();
        const auto spots = Location::getListLatLons();
        for (size_t i = 0; i < spots.size(); i++) {
            const auto name = i < names.size() ? names[i] : "Location " + std::to_string(i + 1);
            out.push_back({name, spots[i].lat(), spots[i].lon(), false, "saved location"});
        }
        return out;
    }

    vector<Place> stormPlaces() {
        vector<Place> out;
        vector<UtilityCira::Storm> all;
        string error;
        if (!UtilityCira::activeStorms(all, error)) {
            return out;
        }
        for (const auto& storm : all) {
            UtilityCira::StormPage page;
            string problem;
            if (!UtilityCira::stormPage(storm.id, page, problem)) {
                continue;
            }
            const auto& fix = !page.forecast.empty() ? page.forecast.front() : (!page.history.empty() ? page.history.front() : UtilityCira::Fix{});
            if (fix.lat == 0.0 && fix.lon == 0.0) {
                continue;   // an invest with no fix
            }
            auto name = storm.name.empty() ? storm.id : storm.name + " (" + storm.id + ")";
            out.push_back({name, fix.lat, fix.lon, true, storm.title + (fix.knots > 0 ? ", " + std::to_string(fix.knots) + " kt" : string{})});
        }
        return out;
    }

    Quality quality(const Instrument& instrument, const SatelliteOrbit::Tle& tle, const SatelliteOrbit::Pass& pass) {
        constexpr double earth = 6371.0, mu = 398600.4418;
        Quality q;
        using Grade = Quality::Grade;
        const auto one = [] (double v) { char text[32]; std::snprintf(text, sizeof text, "%.1f", v); return string{text}; };
        const auto whole = [] (double v) { return std::to_string(static_cast<long long>(std::llround(v))); };
        const auto worse = [&q] (Grade g, const string& why) {   // the worst of what is found
            if (q.summary.empty() || static_cast<int>(g) > static_cast<int>(q.grade)) {
                q.grade = g;
                q.summary = why;
            }
        };
        // the element set: SGP4 is about a kilometer at the epoch and grows about two kilometers a day along the track (more after the satellite has been moved)
        q.ageDays = std::fabs(static_cast<double>(pass.time - SatelliteOrbit::unixSeconds(tle.epochJd))) / 86400.0;
        q.positionKm = 1.0 + 2.0 * q.ageDays;
        q.timeSeconds = q.positionKm / 7.0;
        string detail = "Element set: " + one(q.ageDays) + " days from the pass; the satellite may be about " + whole(q.positionKm) + " km (" + whole(q.timeSeconds) + " s) from where this says.\n";
        if (q.ageDays > 7.0) {
            worse(Grade::Poor, "element set " + whole(q.ageDays) + " days from the pass");
        } else if (q.ageDays > 3.0) {
            worse(Grade::Fair, "element set " + whole(q.ageDays) + " days from the pass");
        }
        // the look geometry at this distance from the track
        const double n = tle.meanMotion / 60.0;   // radians a second
        const double height = n > 0.0 ? std::cbrt(mu / (n * n)) - earth : 800.0;
        const double d = std::fabs(pass.distanceKm), half = instrument.halfWidthKm;
        const double lambda = d / earth, top = earth + height;
        const double eta = std::atan2(earth * std::sin(lambda), top - earth * std::cos(lambda));   // the angle off nadir at the satellite
        const double slant = std::sqrt(top * top + earth * earth - 2.0 * earth * top * std::cos(lambda));
        const double incidence = std::asin(std::min(1.0, top / earth * std::sin(eta)));          // at the ground
        const double growth = slant / height / std::max(0.2, std::cos(incidence));               // a pixel across the scan, against one at the middle
        detail += "Place " + whole(d) + " km from the middle of the swath, " + whole(eta * 180.0 / M_PI) + " degrees off nadir, the ground seen at " + whole(incidence * 180.0 / M_PI) + " degrees from the vertical.\n";
        if (instrument.kind == Kind::Radar) {
            const double deg = incidence * 180.0 / M_PI;
            detail += "Incidence angle about " + whole(deg) + " degrees (the radars' modes are about 20 to 50).\n";
            if (deg < 18.0 || deg > 52.0) {
                worse(Grade::Poor, "outside the usual incidence angles (" + whole(deg) + " degrees)");
            } else if (q.grade == Grade::Good) {
                q.grade = Grade::Possible;
                q.summary = "where it could look; scheduled scenes are on NOAA STAR's list";
            }
        } else if (instrument.kind == Kind::Imagery) {
            q.pixelGrowth = growth;
            detail += "A pixel here is about " + one(growth) + " times as wide across the scan as at the middle (geometry only: the instrument may average pixels at the edges).\n";
            if (growth > 3.5 || d > 0.93 * half) {
                worse(Grade::Poor, "edge of the swath: pixels about " + whole(growth) + " times wider");
            } else if (growth > 1.8 || d > 0.8 * half) {
                worse(Grade::Fair, "pixels about " + one(growth) + " times wider");
            }
        } else if (instrument.kind == Kind::Wind) {
            detail += "A scatterometer's wind is least good near the middle (the beams look nearly straight down) and at the far edge.\n";
            if (d < instrument.innerKm + 100.0) {
                worse(Grade::Fair, "near the gap under the satellite");
            } else if (d > half - 75.0) {
                worse(Grade::Fair, "near the edge of the swath");
            }
        } else {   // a conically scanning microwave imager: the same look angle everywhere, so no stretch; only the ends of the swath are uncertain
            detail += "A conical scanner looks at the same angle everywhere: pixels do not stretch.\n";
            if (d > 0.93 * half) {
                worse(Grade::Fair, "edge of the swath");
            }
        }
        if (!pass.inSwath) {
            worse(Grade::Poor, "the place is outside the swath");
        }
        if (q.summary.empty()) {
            q.summary = "Good";
        }
        q.detail = detail;
        return q;
    }

    vector<Row> passList(const vector<Place>& at, const vector<string>& wanted, const vector<Kind>& kinds, double hours, long long from, string& problem) {
        vector<Row> rows;
        for (const auto& sat : satellites()) {
            if (!wanted.empty() && std::find(wanted.begin(), wanted.end(), sat.name) == wanted.end()) {
                continue;
            }
            bool any = false;
            for (const auto& i : sat.instruments) {
                any = any || kinds.empty() || std::find(kinds.begin(), kinds.end(), i.kind) != kinds.end();
            }
            if (!any) {
                continue;
            }
            string message;
            const auto tle = elements(sat.catalog, message);
            if (!tle.valid) {
                problem += (problem.empty() ? "" : "; ") + sat.name + ": " + message;
                continue;
            }
            const SatelliteOrbit::Propagator propagator{tle};
            for (const auto& place : at) {
                for (const auto& i : sat.instruments) {
                    if (!kinds.empty() && std::find(kinds.begin(), kinds.end(), i.kind) == kinds.end()) {
                        continue;
                    }
                    for (auto& pass : SatelliteOrbit::passes(propagator, place.lat, place.lon, from, hours, {i.label, i.halfWidthKm, i.innerKm})) {
                        if (i.side == 1 && pass.distanceKm < 0.0) {
                            continue;   // looks to the right only
                        }
                        rows.push_back({place.name, place.storm, sat.name, i.label, i.kind, pass, quality(i, tle, pass), place.lat, place.lon});
                    }
                }
            }
        }
        std::sort(rows.begin(), rows.end(), [] (const Row& a, const Row& b) { return a.pass.time < b.pass.time; });
        return rows;
    }
}
