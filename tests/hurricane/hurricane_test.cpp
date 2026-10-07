// Checks the pure parsers against rows copied from live NHC files (2026-10-07) and from the HDOB specification's own sample.
#include <cmath>
#include <cstdlib>
#include <iostream>
#include "hurricane/UtilityAtcf.h"
#include "hurricane/UtilityHdob.h"
#include <fstream>
#include <sstream>
#include "hurricane/UtilityEcmwfTracks.h"
#include "hurricane/UtilityEnsembleStats.h"
#include "hurricane/UtilityShips.h"
#include "util/UtilityGzip.h"

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::cerr << "FAILED " << __LINE__ << ": " #cond "\n"; failures++; } } while (0)
static bool near(double a, double b, double tol = 1e-6) { return std::abs(a - b) <= tol; }

static void atcf() {
    const std::string btk =
        "AL, 09, 2026100700,   , BEST,   0, 218N,  953W,  30, 1006, TD,   0,    ,    0,    0,    0,    0, 1009,  150,  40,  40,   0,   L,   0,    ,   0,   0,       NINE, S,\n"
        "AL, 09, 2026100706,   , BEST,   0, 218N,  945W,  35, 1004, TS,  34, NEQ,   40,    0,    0,   40, 1009,  180,  30,  45,   0,   L,   0,    ,   0,   0,     ISAIAS, M,\n"
        "AL, 09, 2026100706,   , BEST,   0, 218N,  945W,  35, 1004, TS,  50, NEQ,    0,    0,    0,    0, 1009,  180,  30,  45,   0,   L,   0,    ,   0,   0,     ISAIAS, M,\n";
    const auto best = UtilityAtcf::bestTrack(btk);
    CHECK(best.size() == 2);   // the repeated time (one row per wind-radii threshold) counts once
    CHECK(near(best[1].lat, 21.8) && near(best[1].lon, -94.5));
    CHECK(best[1].wind == 35 && best[1].pressure == 1004 && best[1].status == "TS" && best[1].name == "ISAIAS");

    // wind radii (34 / 50 / 64 kt by quadrant NE SE SW NW) and the forecast cone
    CHECK(best[1].radii[0][0] == 40 && best[1].radii[0][1] == 0 && best[1].radii[0][2] == 0 && best[1].radii[0][3] == 40 && best[1].radii[1][0] == 0);
    CHECK(near(UtilityAtcf::coneRadiusNm(0), 0.0) && near(UtilityAtcf::coneRadiusNm(12), 25.0) && near(UtilityAtcf::coneRadiusNm(18), 32.0));
    CHECK(near(UtilityAtcf::coneRadiusNm(96), 134.0) && near(UtilityAtcf::coneRadiusNm(108), 167.0) && near(UtilityAtcf::coneRadiusNm(120), 200.0) && near(UtilityAtcf::coneRadiusNm(168), 200.0));

    const std::string guidance =
        "AL, 09, 2026100618, 03, AVNO,   0, 203N,  954W,  28, 1010, XX,  34, NEQ,    0,    0,    0,    0,  -99,  -99,  46,   0,   0,\n"
        "AL, 09, 2026100700, 03, AVNO,   0, 219N,  950W,  30, 1008, XX,  34, NEQ,    0,    0,    0,    0,  -99,  -99,  46,   0,   0,\n"
        "AL, 09, 2026100700, 03, AVNO,  12, 225N,  940W,  35, 1006, XX,  34, NEQ,    0,    0,    0,    0,  -99,  -99,  46,   0,   0,\n"
        "AL, 09, 2026100700, 03, AVNO,  12, 225N,  940W,  35, 1006, XX,  50, NEQ,    0,    0,    0,    0,  -99,  -99,  46,   0,   0,\n"
        "AL, 09, 2026100706, 03, IVCN,   0,   0N,    0W,  30,    0, XX,  34, NEQ,    0,    0,    0,    0,\n"
        "AL, 09, 2026100706, 03, IVCN,  12,   0N,    0W,  30,    0, XX,  34, NEQ,    0,    0,    0,    0,\n";
    const auto tracks = UtilityAtcf::latestTracks(guidance);
    CHECK(tracks.size() == 1);   // IVCN has only 0N 0W "no position" rows
    CHECK(tracks[0].tech == "AVNO" && tracks[0].cycle == "2026100700");   // the newest run only
    CHECK(tracks[0].fixes.size() == 2 && tracks[0].fixes[1].tau == 12);
    CHECK(UtilityAtcf::groupOf("OFCL") == UtilityAtcf::Group::Official);
    CHECK(UtilityAtcf::groupOf("TVCN") == UtilityAtcf::Group::Consensus);
    CHECK(UtilityAtcf::groupOf("HWRF") == UtilityAtcf::Group::Hurricane);
    CHECK(UtilityAtcf::groupOf("AP07") == UtilityAtcf::Group::Ensemble);
    CHECK(UtilityAtcf::groupOf("AC00") == UtilityAtcf::Group::Ensemble);
    CHECK(UtilityAtcf::categoryOf(33) == 0 && UtilityAtcf::categoryOf(34) == 1 && UtilityAtcf::categoryOf(64) == 2);
    CHECK(UtilityAtcf::categoryOf(113) == 5 && UtilityAtcf::categoryOf(137) == 6);
    CHECK(near(UtilityAtcf::hoursBetween("2026100700", "2026100806"), 30.0));
    CHECK(near(UtilityAtcf::hoursBetween("2026123118", "2027010100"), 6.0));
    CHECK(UtilityAtcf::formatTime("2026100718") == "Oct 07 18Z");
    const auto names = UtilityAtcf::parseTechList(
        "NUM TECH ERRS RETIRED COLOR DEFAULTS INT-DEFS RADII-DEFS LONG-NAME\n"
        " 03 OFCL   1      0    28      1        1         1                 NHC official forecast\n");
    CHECK(names.size() == 1 && names.at("OFCL") == "NHC official forecast");
}

static void hdob() {
    // the specification's sample (Katrina, 2005): flight at 709 mb, so XXXX is the extrapolated surface pressure
    const std::string spec =
        "URNT15 KNHC 281426\n"
        "AF302 1712A KATRINA            HDOB 41 20050928\n"
        "142030 2608N 08756W 7093 03047 9333 +192 +134 133083 089 080 999 00\n"
        "142100 2609N 08755W 7091 03054 9330 +166 +146 133106 115 103 999 00\n"
        "$$\n";
    auto messages = UtilityHdob::parse(spec);
    CHECK(messages.size() == 1 && messages[0].mission == "AF302 1712A KATRINA" && messages[0].number == 41);
    CHECK(messages[0].obs.size() == 2);
    const auto& a = messages[0].obs[0];
    CHECK(near(a.lat, 26.0 + 8.0 / 60.0) && near(a.lon, -(87.0 + 56.0 / 60.0)));
    CHECK(near(a.staticPressure, 709.3) && near(a.height, 3047) && near(a.surfacePressure, 933.3));
    CHECK(near(a.temperature, 19.2) && near(a.dewPoint, 13.4));
    CHECK(near(a.windDirection, 133) && near(a.windSpeed, 83) && near(a.peakWind, 89) && near(a.sfmrWind, 80));
    CHECK(!UtilityHdob::has(a.rainRate));   // 999 = missing
    CHECK(UtilityHdob::timeText(a.seconds) == "28 Sep 14:20Z");

    // a high-altitude survey flight: 148.7 mb (not 1148.7), XXXX is the D-value, missing fields are slashes
    const std::string jet =
        "URNT15 KWBC 062319\n"
        "NOAA9 01BBA SURV               HDOB 34 20261006\n"
        "231000 2635N 09603W 1487 14402 0744 -701 //// ////// /// /// /// 09\n";
    messages = UtilityHdob::parse(jet);
    CHECK(messages.size() == 1 && messages[0].obs.size() == 1);
    const auto& b = messages[0].obs[0];
    CHECK(near(b.staticPressure, 148.7) && near(b.dValue, 744) && !UtilityHdob::has(b.surfacePressure));
    CHECK(near(b.temperature, -70.1) && !UtilityHdob::has(b.dewPoint) && !UtilityHdob::has(b.windSpeed));

    // a bulletin across midnight keeps counting
    const std::string midnight =
        "AF304 0101A TEST               HDOB 01 20261006\n"
        "235930 2000N 08000W 7000 03000 9900 +100 +100 090050 060 000 000 00\n"
        "000000 2000N 08000W 7000 03000 9900 +100 +100 090050 060 000 000 00\n";
    messages = UtilityHdob::parse(midnight);
    CHECK(messages[0].obs.size() == 2 && messages[0].obs[1].seconds - messages[0].obs[0].seconds == 30);
}

static void gzip() {
    // "hello hello hello\n", gzip -9 (fixed Huffman with a back-reference)
    const unsigned char bytes[] = {0x1f, 0x8b, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x03, 0xcb, 0x48, 0xcd, 0xc9, 0xc9, 0x57, 0xc8, 0x40,
                                   0x90, 0x5c, 0x00, 0x3b, 0x7c, 0x8a, 0xdf, 0x12, 0x00, 0x00, 0x00};
    std::string out;
    CHECK(UtilityGzip::gunzip(std::string{reinterpret_cast<const char *>(bytes), sizeof bytes}, out));
    CHECK(out == "hello hello hello\n");
    CHECK(!UtilityGzip::gunzip("not gzip at all, not at all......", out));
}

static std::string readFile(const std::string& path) {
    std::ifstream file{path, std::ios::binary};
    std::stringstream stream;
    stream << file.rdbuf();
    return stream.str();
}

// ECMWF's tropical cyclone track BUFR; the expected numbers were read with ECMWF's own bufr_dump (eccodes) from the same files
static void ecmwf(const std::string& fixtures) {
    std::vector<UtilityEcmwfTracks::Storm> storms;
    std::string error;
    CHECK(UtilityEcmwfTracks::parse(readFile(fixtures + "/ecmwf_52_members.bufr"), storms, error));
    CHECK(storms.size() == 1 && storms[0].id == "35W" && storms[0].name == "NOLO" && storms[0].cycle == "2026100700");
    CHECK(storms[0].members.size() == 52);
    const auto& m1 = storms[0].members[0];
    CHECK(m1.number == 1 && m1.type == 4 && m1.steps.size() == 30);
    CHECK(near(m1.steps[0].lat, 25.4, 1e-9) && near(m1.steps[0].lon, 164.2, 1e-9) && near(m1.steps[0].pressure, 987.0, 1e-9));
    CHECK(near(m1.steps[0].wind, 30.9 * 1.943844, 1e-3) && near(m1.steps[0].windLat, 25.5, 1e-9) && near(m1.steps[0].windLon, 164.9, 1e-9));
    CHECK(m1.steps[1].hour == 6 && near(m1.steps[1].lat, 25.7, 1e-9) && near(m1.steps[1].lon, 162.0, 1e-9) && near(m1.steps[1].pressure, 991.0, 1e-9));
    CHECK(storms[0].members[1].steps[0].lat == 25.7 && near(storms[0].members[1].steps[0].lon, 163.9, 1e-9));
    CHECK(storms[0].members[51].type == 0 && storms[0].members[50].type == 1);   // the two unperturbed runs come last

    // weak storms: the positions are missing (all ones), and a one-member storm is not compressed
    CHECK(UtilityEcmwfTracks::parse(readFile(fixtures + "/ecmwf_compressed_3_members.bufr"), storms, error));
    CHECK(storms.size() == 1 && storms[0].id == "71A" && storms[0].members.size() == 3);
    CHECK(!UtilityEcmwfTracks::has(storms[0].members[0].steps[0].lat));
    CHECK(UtilityEcmwfTracks::parse(readFile(fixtures + "/ecmwf_single_member.bufr"), storms, error));
    CHECK(storms.size() == 1 && storms[0].id == "72A" && storms[0].members.size() == 1 && storms[0].members[0].number == 17);
    CHECK(!UtilityEcmwfTracks::parse("not a bufr file", storms, error));

    // IFS files have no leading model identifier; this is the high-resolution run of storm 09L
    CHECK(UtilityEcmwfTracks::parse(readFile(fixtures + "/ecmwf_ifs_hres.bufr"), storms, error));
    CHECK(storms.size() == 1 && storms[0].id == "09L" && storms[0].members.size() == 1 && storms[0].members[0].type == 0);
    const auto& hres = storms[0].members[0];
    CHECK(near(hres.steps[0].lat, 21.4, 1e-9) && near(hres.steps[0].lon, -95.0, 1e-9) && near(hres.steps[0].pressure, 1005.0, 1e-9));
    CHECK(near(hres.steps[0].wind, 13.9 * 1.943844, 1e-3) && near(hres.steps[1].lat, 21.4, 1e-9) && near(hres.steps[1].lon, -94.7, 1e-9));
    CHECK(hres.steps[1].hour == 6 && near(hres.steps[1].pressure, 1006.0, 1e-9));

    // NOAA's GEFS members as an ensemble
    std::vector<UtilityAtcf::Track> guidance;
    for (const char * tech : {"AP01", "AP02", "AC00", "AVNO", "AP9X"}) {
        UtilityAtcf::Track track;
        track.tech = tech;
        track.cycle = "2026100700";
        UtilityAtcf::Fix fix;
        fix.tau = 0;
        fix.lat = 20.0;
        fix.lon = -80.0;
        fix.wind = 45;
        fix.pressure = -1;
        track.fixes.push_back(fix);
        guidance.push_back(track);
    }
    UtilityEcmwfTracks::Storm gefs;
    CHECK(UtilityEnsembleStats::fromGefs(guidance, gefs));
    CHECK(gefs.members.size() == 3 && gefs.members[0].number == 1 && gefs.members[0].type == 4 && gefs.members[2].type == 1 && gefs.cycle == "2026100700");
    CHECK(near(gefs.members[0].steps[0].wind, 45.0) && !UtilityEcmwfTracks::has(gefs.members[0].steps[0].pressure));
    CHECK(UtilityEnsembleStats::compute(gefs, 6).at(0).total == 2);   // the control is not part of the distribution

    // the distribution
    UtilityEcmwfTracks::Storm made;
    made.id = "01L";
    for (int i = 0; i < 5; i++) {   // five perturbed members, wind 30..70 kt at hour 0; the fifth is gone by hour 6
        UtilityEcmwfTracks::Member member;
        member.type = 4;
        UtilityEcmwfTracks::Step a;
        a.hour = 0;
        a.lat = 20.0;
        a.lon = -60.0 + i * 0.0;
        a.wind = 30.0 + i * 10.0;
        a.pressure = 1010.0 - i * 5.0;
        member.steps.push_back(a);
        UtilityEcmwfTracks::Step b = a;
        b.hour = 6;
        if (i == 4) {
            b.lat = b.lon = UtilityEcmwfTracks::missing;
            b.wind = UtilityEcmwfTracks::missing;
        }
        member.steps.push_back(b);
        made.members.push_back(member);
    }
    UtilityEcmwfTracks::Member control;   // not part of the distribution
    control.type = 1;
    made.members.push_back(control);
    const auto stats = UtilityEnsembleStats::compute(made, 6);
    CHECK(stats.size() == 2 && stats[0].total == 5 && stats[0].alive == 5 && stats[1].alive == 4);
    CHECK(near(stats[0].windMedian, 50.0) && near(stats[0].windMin, 30.0) && near(stats[0].windMax, 70.0) && near(stats[0].wind25, 40.0));
    CHECK(near(stats[0].probTs, 0.8) && near(stats[0].probHurricane, 0.2) && near(stats[0].probMajor, 0.0));   // only the 70 kt member is a hurricane
    CHECK(near(stats[1].probAlive, 0.8) && near(stats[1].probTs, 0.6));   // 40, 50, 60 of the 4 alive (30 is below 34), the gone member counts as below
    CHECK(near(stats[0].radius90, 0.0, 1e-6) && near(stats[0].centerLat, 20.0, 1e-6) && near(stats[0].centerLon, -60.0, 1e-6));
    CHECK(near(UtilityEnsembleStats::percentile({1.0, 2.0, 3.0, 4.0}, 0.5), 2.5) && !UtilityEcmwfTracks::has(UtilityEnsembleStats::percentile({}, 0.5)));
}

// the SHIPS text product (a real file: AL09 Isaias, 2026-10-07 06 UTC)
static void ships(const std::string& fixtures) {
    const auto s = UtilityShips::parse(readFile(fixtures + "/ships_al092026.txt"));
    CHECK(s.ok && s.name == "ISAIAS" && s.id == "AL092026" && s.cycle == "2026100706");
    CHECK(s.hours.size() == 17 && s.hours[0] == 0 && s.hours[1] == 6 && s.hours[16] == 168);
    const auto * land = s.row("V (KT) LAND");
    CHECK(land != nullptr && near((*land)[0], 35) && near((*land)[6], 77) && near((*land)[7], 82) && !UtilityShips::has((*land)[13]));   // N/A -> missing
    const auto * shear = s.row("SHEAR (KT)");
    CHECK(shear != nullptr && near((*shear)[0], 14) && near((*shear)[8], 47));
    const auto * sst = s.row("SST (C)");
    CHECK(sst != nullptr && near((*sst)[0], 30.9) && near((*sst)[12], 20.8));
    const auto * mpi = s.row("POT. INT. (KT)");
    CHECK(mpi != nullptr && near((*mpi)[0], 169));
    const auto * vtx = s.row("MODEL VTX (KT)");
    CHECK(vtx != nullptr && near((*vtx)[12], 11) && !UtilityShips::has((*vtx)[13]));   // LOST -> missing
    CHECK(s.stormType.size() == 17 && s.stormType[0] == "TROP" && s.stormType[9] == "EXTP");
    CHECK(near(s.preliminaryRi, 40.7));
    CHECK(s.riLines.size() == 8 && s.riLines[2].knots == 30 && s.riLines[2].hours == 24 && near(s.riLines[2].percent, 13) && near(s.riLines[2].climatology, 6.8));
    CHECK(s.riThresholds.size() == 8 && s.riThresholds[0] == "20/12" && s.riThresholds[7] == "65/72");
    CHECK(s.riMatrix.size() == 6 && s.riMatrix[0].first == "SHIPS-RII" && near(s.riMatrix[0].second[1], 31.5) && near(s.riMatrix[3].second[2], 9.8));
    const std::string listing = "<a href=\"26100618AL0926_ships.txt\">x</a> <a href=\"26100706AL0926_ships.txt\">x</a> <a href=\"26100706AL9226_ships.txt\">x</a>";
    CHECK(UtilityShips::newestFile(listing, "al092026") == "26100706AL0926_ships.txt");
    CHECK(UtilityShips::newestFile(listing, "al102026").empty());
}

int main(int argc, char ** argv) {
    ships(argc > 1 ? argv[1] : "tests/hurricane/fixtures");
    ecmwf(argc > 1 ? argv[1] : "tests/hurricane/fixtures");
    atcf();
    hdob();
    gzip();
    if (failures == 0) {
        std::cout << "all hurricane parser tests passed\n";
    }
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
