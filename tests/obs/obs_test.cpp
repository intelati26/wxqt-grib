// Checks the METAR cache reader and the MADIS mesonet netCDF reader (streamed out of gzip) against real excerpts (8 October 2026).
#include <cmath>
#include <fstream>
#include <iostream>
#include <sstream>
#include "obs/UtilityMadis.h"
#include "obs/UtilityMetarCache.h"
#include "obs/UtilityMetarHistory.h"
#include "util/UtilityGzip.h"

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::cerr << "FAILED " << __LINE__ << ": " #cond "\n"; failures++; } } while (0)
static bool near(double a, double b, double tol = 1e-3) { return std::abs(a - b) <= tol; }

static std::string readFile(const std::string& path) {
    std::ifstream file{path, std::ios::binary};
    std::stringstream stream;
    stream << file.rdbuf();
    return stream.str();
}

static const SurfaceStation * find(const std::vector<SurfaceStation>& all, const std::string& id) {
    for (const auto& s : all) {
        if (s.id == id) return &s;
    }
    return nullptr;
}

int main(int argc, char ** argv) {
    const std::string fixtures = argc > 1 ? argv[1] : "tests/obs/fixtures";

    // METARs
    CHECK(UtilityMetarCache::splitCsv("\"a,b\",c,,\"d\"\"e\"") == (std::vector<std::string>{"a,b", "c", "", "d\"e"}));
    CHECK(UtilityMetarCache::parseTime("2026-10-08T12:15:00.000Z") == 1791461700L && UtilityMetarCache::parseTime("nonsense") == 0);
    const auto metars = UtilityMetarCache::parse(readFile(fixtures + "/metars_sample.csv"));
    CHECK(metars.size() == 47 && find(metars, "KQFX") == nullptr);   // two have the placeholder position -99.99
    const auto * mve = find(metars, "KMVE");
    CHECK(mve != nullptr && mve->airport && mve->network == "METAR" && near(mve->lat, 44.9675) && near(mve->lon, -95.7116) && near(mve->temperature, 9.0) && near(mve->dewPoint, 6.0));
    CHECK(mve->flight == "VFR" && near(mve->visibility, 10.0) && near(mve->altimeter, 30.12) && near(mve->windSpeed, 0.0) && !SurfaceStation::has(mve->windGust) && near(mve->elevation, 319.0));
    CHECK(mve->raw.rfind("METAR KMVE 081215Z", 0) == 0 && near(mve->humidity, 81.5, 1.0));
    bool sawSky = false;
    for (const auto& s : metars) {
        sawSky = sawSky || !s.sky.empty();
    }
    CHECK(sawSky);

    const auto names = UtilityMetarCache::parseNames(R"([{"id":"KOUN","icaoId":"KOUN","site":"Norman\/Max Westheimer","lat":35.2,"state":"OK"},{"id":"32012","icaoId":null,"site":"Caf\u00e9 \"X\"","state":""}])");
    CHECK(names.size() == 2 && names.at("KOUN").name == "Norman/Max Westheimer" && names.at("KOUN").state == "OK" && names.at("32012").name == "Caf\xC3\xA9 \"X\"");

    const auto history = UtilityMetarHistory::parse(readFile(fixtures + "/metar_history_koun.json"));
    CHECK(history.size() == 9 && history.front().seconds < history.back().seconds);
    CHECK(UtilityMetarHistory::url("KOUN", 24) == "https://aviationweather.gov/api/data/metar?ids=KOUN&format=json&hours=24");
    CHECK(history.back().raw.rfind("METAR KOUN", 0) == 0 && near(history.back().altimeter, 1017.7 / 33.8639, 0.01));
    bool sawVariable = false;
    for (const auto& h : history) {
        sawVariable = sawVariable || (!SurfaceStation::has(h.windDirection) && !SurfaceStation::has(h.temperature) && near(h.windGust, 12.0));   // "VRB" and a null temperature stay missing
    }
    CHECK(sawVariable && UtilityMetarHistory::parse("nope").empty());

    // the MADIS file: the same answer however the stream is cut
    const auto packed = readFile(fixtures + "/mesonet_sample.nc.gz");
    std::vector<size_t> sizes;
    std::vector<SurfaceStation> first;
    for (const size_t chunk : {size_t{1} << 20, size_t{5000}, size_t{70000}}) {
        UtilityMadis::MesonetReader reader;
        size_t pieces = 0;
        const bool ok = UtilityGzip::gunzipStream(packed, chunk, [&reader, &pieces] (const char * data, size_t size) { pieces++; return reader.push(data, size); });
        CHECK(ok && !reader.failed() && reader.records() == 60);
        CHECK(chunk >= (size_t{1} << 20) || pieces > 3);
        const auto stations = reader.stations();
        CHECK(stations.size() == 56);   // four of the 60 reports carry no weather value
        if (first.empty()) {
            first = stations;
        } else {
            CHECK(stations.size() == first.size());
        }
    }
    const auto * ap = find(first, "AP164");
    CHECK(ap != nullptr && ap->network == "APRSWXNET" && !ap->airport && near(ap->lat, 45.58783, 1e-4) && near(ap->lon, -122.15817, 1e-4));
    CHECK(ap != nullptr && near(ap->temperature, 285.37222 - 273.15) && near(ap->windDirection, 263.0) && near(ap->windSpeed, 0.0) && ap->seconds == 1791460800L && ap->name.rfind("WB7UVH", 0) == 0);
    const auto * av = find(first, "AV433");
    CHECK(av != nullptr && near(av->windSpeed, 1.78816 * 1.943844, 1e-3) && near(av->windGust, 4.91744 * 1.943844, 1e-3) && near(av->temperature, 299.81668 - 273.15) && near(av->dewPoint, 293.07224 - 273.15));
    CHECK(av != nullptr && near(av->altimeter, 101100.0 / 3386.389, 1e-3) && near(av->humidity, 66.0) && av->quality == 'S' && near(av->lon, 17.28517, 1e-4));
    const auto * lost = find(first, "F1936");   // every value but the pressure missing (3.4e38): still a station, with the pressure
    CHECK(lost != nullptr && !SurfaceStation::has(lost->temperature) && !SurfaceStation::has(lost->windSpeed) && near(lost->altimeter, 101920.0 / 3386.389, 1e-3));
    // gunzip alone gives the same bytes as the stream
    std::string whole;
    CHECK(UtilityGzip::gunzip(packed, whole));
    std::string joined;
    CHECK(UtilityGzip::gunzipStream(packed, 4096, [&joined] (const char * d, size_t n) { joined.append(d, n); return true; }));
    CHECK(whole == joined && whole.size() > 2000000);
    // a sink that stops, and data that is not a netCDF file
    CHECK(!UtilityGzip::gunzipStream(packed, 4096, [] (const char *, size_t) { return false; }));
    UtilityMadis::MesonetReader bad;
    CHECK(!bad.push("not netcdf at all, at all", 24) && bad.failed());
    CHECK(UtilityMadis::listFiles("<a href=\"20261008_1000.gz\">x</a><a href=\"20261008_0900.gz\">y</a><a href=\"foo\">z</a>") == (std::vector<std::string>{"20261008_0900.gz", "20261008_1000.gz"}));

    if (failures == 0) {
        std::cout << "all surface observation parser tests passed\n";
    }
    return failures == 0 ? 0 : 1;
}
