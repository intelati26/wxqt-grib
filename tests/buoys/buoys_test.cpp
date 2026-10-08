// Checks the NDBC parsers against excerpts of the real files (7 October 2026).
#include <cmath>
#include <fstream>
#include <iostream>
#include <sstream>
#include "buoys/UtilityBuoys.h"

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::cerr << "FAILED " << __LINE__ << ": " #cond "\n"; failures++; } } while (0)
static bool near(double a, double b, double tol = 1e-6) { return std::abs(a - b) <= tol; }

static std::string readFile(const std::string& path) {
    std::ifstream file{path, std::ios::binary};
    std::stringstream stream;
    stream << file.rdbuf();
    return stream.str();
}

int main(int argc, char ** argv) {
    const std::string fixtures = argc > 1 ? argv[1] : "tests/buoys/fixtures";
    const auto latest = UtilityBuoys::parseObservations(readFile(fixtures + "/latest_obs_excerpt.txt"));
    CHECK(latest.size() == 8);
    const auto& b42001 = latest[2];
    CHECK(b42001.id == "42001" && near(b42001.lat, 25.922) && near(b42001.lon, -89.638) && b42001.seconds == 1791367200L);   // 2026-10-07 10:00 UTC
    CHECK(near(b42001.windDirection, 190) && near(b42001.windSpeed, 2.0) && near(b42001.gust, 3.0) && !UtilityBuoys::has(b42001.waveHeight));
    CHECK(near(b42001.pressure, 1008.3) && near(b42001.tendency, -1.2) && near(b42001.airTemperature, 28.6) && near(b42001.waterTemperature, 29.8) && near(b42001.dewPoint, 24.8));
    CHECK(near(latest[4].tendency, 0.5) && near(latest[4].lon, -129.895));   // a "+0.5" tendency
    CHECK(latest[5].id == "BURL1" && !UtilityBuoys::has(latest[5].waterTemperature) && near(latest[5].windSpeed, 8.8));
    CHECK(latest[0].id == "15009" && near(latest[0].lat, 0.0) && near(latest[0].lon, -3.051));

    // a station's own history: the same columns in another order, newest first
    const auto series = UtilityBuoys::parseObservations(readFile(fixtures + "/realtime2_42001_head.txt"));
    CHECK(series.size() == 12 && series[0].seconds == 1791367200L && series[1].seconds == 1791366600L);
    CHECK(near(series[1].waveHeight, 0.6) && near(series[1].dominantPeriod, 6) && near(series[1].averagePeriod, 4.8) && near(series[1].waveDirection, 41) && near(series[1].pressure, 1008.2));
    CHECK(near(series[0].tendency, -1.2) && !UtilityBuoys::has(series[0].waveHeight) && near(series[0].airTemperature, 28.6));
    CHECK(series[0].id.empty());   // no station column in a station's own file

    const auto stations = UtilityBuoys::parseStations(readFile(fixtures + "/activestations_excerpt.xml"));
    CHECK(stations.size() == 6);
    const auto& s = stations.at("42001");
    CHECK(s.name == "MID GULF - 180 nm South of Southwest Pass, LA" && s.owner == "NDBC" && s.type == "buoy" && s.met && near(s.lat, 25.922) && near(s.lon, -89.638));
    CHECK(!stations.at("13001").met && stations.at("15009").type == "other" && stations.at("15009").name.empty());
    CHECK(UtilityBuoys::parseStations("<station id=\"A&amp;B\" lat=\"1\" lon=\"2\" name=\"Cape &quot;X&quot;\" owner=\"o\" type=\"buoy\" met=\"y\"/>").at("A&amp;B").name == "Cape \"X\"");
    CHECK(near(UtilityBuoys::knots(10.0), 19.43844, 1e-4) && near(UtilityBuoys::feet(2.0), 6.56168, 1e-4) && near(UtilityBuoys::fahrenheit(20.0), 68.0) && !UtilityBuoys::has(UtilityBuoys::knots(UtilityBuoys::missing)));
    CHECK(UtilityBuoys::timeText(1791367200L) == "2026-10-07 10:00Z");
    if (failures == 0) {
        std::cout << "all buoy parser tests passed\n";
    }
    return failures == 0 ? 0 : 1;
}
