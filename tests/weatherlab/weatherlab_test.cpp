// Tests of the Weather Lab (DeepMind FNV3 / WeatherNext 3) cyclone ensemble reader, on rows cut from a live file (2026-10-08 12Z).
#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>
#include "hurricane/UtilityWeatherLab.h"

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL line %d: %s\n", __LINE__, #c); failures++; } } while (0)

int main(int argc, char ** argv) {
    if (argc < 2) {
        std::printf("usage: weatherlab_test <fixtures folder>\n");
        return 2;
    }
    std::ifstream file{std::string{argv[1]} + "/fnv3_sample.csv"};
    std::stringstream text;
    text << file.rdbuf();
    std::vector<UtilityEcmwfTracks::Storm> storms;
    std::string error;
    CHECK(UtilityWeatherLab::parse(text.str(), storms, error));
    CHECK(!storms.empty() && storms.front().id == "09L" && storms.front().cycle == "2026100812");
    if (!storms.empty()) {
        const auto& storm = storms.front();
        CHECK(storm.name == "AL092026");
        CHECK(storm.members.size() == 3 && storm.members[0].number == 0 && storm.members[2].number == 2);
        CHECK(storm.members[0].type == 4);
        const auto& m = storm.members[0];
        CHECK(m.steps.size() >= 5 && m.steps.front().hour == 0 && m.steps[1].hour == 6);
        CHECK(std::abs(m.steps.front().lat - 23.4) < 1e-9 && std::abs(m.steps.front().lon + 90.6) < 1e-9);          // the 12Z fix; west longitudes stay negative
        CHECK(std::abs(m.steps.front().pressure - 976.0) < 1e-9 && std::abs(m.steps.front().wind - 70.0) < 1e-9);
        CHECK(std::abs(m.steps[1].lat - 24.27) < 1e-9 && std::abs(m.steps[1].wind - 107.0) < 1e-9);
        CHECK(m.steps[1].windLat == m.steps[1].lat);                                                                   // one position in the file
        // a member whose row has no position: kept as a step with the position missing (the map breaks its line there)
        const auto& dead = storm.members[2];
        CHECK(dead.steps.back().hour == 48 && !UtilityEcmwfTracks::has(dead.steps.back().lat) && !UtilityEcmwfTracks::has(dead.steps.back().pressure));
        for (const auto& member : storm.members) {
            for (size_t i = 1; i < member.steps.size(); i++) {
                CHECK(member.steps[i].hour > member.steps[i - 1].hour);   // in time order
            }
        }
    }
    bool other = false;
    for (const auto& storm : storms) {
        other = other || storm.id.size() == 3;
    }
    CHECK(storms.size() >= 2 && other);   // the file holds every storm of the world
    CHECK(UtilityWeatherLab::shortId("AL092026") == "09L" && UtilityWeatherLab::shortId("ep182026") == "18E" && UtilityWeatherLab::shortId("WP272026") == "27W");
    CHECK(UtilityWeatherLab::shortId("CP012026") == "01C" && UtilityWeatherLab::shortId("XX") == "XX");
    CHECK(UtilityWeatherLab::url("FNV3", "2026100812") ==
          "https://deepmind.google.com/science/weatherlab/download/cyclones/FNV3/ensemble/paired/csv/FNV3_2026_10_08T12_00_paired.csv");
    CHECK(UtilityWeatherLab::url("WNV3", "2026100800").find("WNV3_2026_10_08T00_00_paired.csv") != std::string::npos && UtilityWeatherLab::url("FNV3", "bad").empty());
    // a file that is not this one
    CHECK(!UtilityWeatherLab::parse("<html>not found</html>", storms, error) && !error.empty());
    CHECK(!UtilityWeatherLab::parse("init_time,track_id\nx,y\n", storms, error));
    // columns in another order are still found by name
    const std::string shuffled =
        "lon,lat,sample,track_id,init_time,lead_time_hours,maximum_sustained_wind_speed_knots,minimum_sea_level_pressure_hpa\n"
        "-60.5,20.25,7,AL102026,2026-10-08 00:00:00,12,64.5,985.2\n";
    CHECK(UtilityWeatherLab::parse(shuffled, storms, error) && storms.size() == 1 && storms[0].id == "10L" && storms[0].cycle == "2026100800");
    CHECK(storms[0].members[0].number == 7 && storms[0].members[0].steps[0].hour == 12 && std::abs(storms[0].members[0].steps[0].lon + 60.5) < 1e-9 &&
          std::abs(storms[0].members[0].steps[0].pressure - 985.2) < 1e-9);
    // the cyclogenesis file: new-storm clusters only (an existing storm's ATCF id is skipped), the first row of each member, the size of the ensemble
    {
        std::ifstream genesisFile{std::string{argv[1]} + "/genesis_sample.csv"};
        std::stringstream genesisText;
        genesisText << genesisFile.rdbuf();
        std::vector<UtilityWeatherLab::Genesis> clusters;
        int members = 0;
        std::string cycle;
        CHECK(UtilityWeatherLab::parseGenesis(genesisText.str(), clusters, members, cycle, error));
        CHECK(cycle == "2026100812" && members == 1000);                       // samples 0 .. 999
        CHECK(clusters.size() == 2 && clusters[0].track == "7" && clusters[1].track == "9");   // AL092026 is not a new storm
        for (const auto& cluster : clusters) {
            CHECK(cluster.points.size() == 5);                                 // one per member listed, not one per row
            for (const auto& point : cluster.points) {
                CHECK(point.hour > 0 && point.lat > -90.0 && point.lat < 90.0 && UtilityEcmwfTracks::has(point.wind));
            }
        }
        // the earliest row of a member is where it forms
        const std::string twice =
            "init_time,track_id,sample,lead_time_hours,lat,lon,minimum_sea_level_pressure_hpa,maximum_sustained_wind_speed_knots\n"
            "2026-10-08 12:00:00,5,3,48,10.5,150.5,1005.0,30.0\n"
            "2026-10-08 12:00:00,5,3,42,10.0,151.0,1007.0,25.0\n"
            "2026-10-08 12:00:00,5,3,54,11.0,150.0,1003.0,35.0\n"
            "2026-10-08 12:00:00,AL092026,3,0,23.4,-90.6,976.0,70.0\n";
        CHECK(UtilityWeatherLab::parseGenesis(twice, clusters, members, cycle, error) && clusters.size() == 1 && clusters[0].points.size() == 1);
        CHECK(clusters[0].points[0].hour == 42 && std::abs(clusters[0].points[0].lon - 151.0) < 1e-9 && members == 4);
        CHECK(!UtilityWeatherLab::parseGenesis("<html>nope</html>", clusters, members, cycle, error));
        CHECK(UtilityWeatherLab::genesisUrl("2026100812").find("FNV3_LARGE_ENSEMBLE/ensemble/cyclogenesis/csv/FNV3_LARGE_ENSEMBLE_2026_10_08T12_00_cyclogenesis.csv") != std::string::npos);
    }
    std::printf(failures ? "%d failures\n" : "all Weather Lab tests passed\n", failures);
    return failures ? 1 : 0;
}
