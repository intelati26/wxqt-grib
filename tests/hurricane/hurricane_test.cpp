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
#include "hurricane/UtilityChanges.h"
#include "hurricane/UtilityDropsonde.h"
#include "hurricane/UtilityNhcGis.h"
#include "hurricane/UtilityNhcText.h"
#include "hurricane/UtilityPod.h"
#include "hurricane/UtilityVdm.h"
#include "hurricane/UtilitySeason.h"
#include "hurricane/UtilityShips.h"
#include "util/UtilityGzip.h"
#include "util/UtilityZip.h"
#include "hurricane/UtilityWindProbability.h"
#include "hurricane/UtilityHurdat.h"

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
    // the wind swath: a stationary 60 nm field gives rings that reach 1 degree of latitude north and south of the centre
    {
        UtilityAtcf::Track track;
        for (const int tau : {0, 12, 24}) {
            UtilityAtcf::Fix f;
            f.tau = tau;
            f.lat = 20.0;
            f.lon = -80.0 + tau * 0.1;   // moving east 0.1 degree an hour
            f.radii[0] = {60, 60, 60, 60};
            track.fixes.push_back(f);
        }
        const auto swath = UtilityAtcf::windSwath(track, 0, 3);
        CHECK(swath.size() == 4 + 4 + 1);   // 3-hourly through the two 12 hour intervals, and the last fix
        double minLat = 99, maxLat = -99, minLon = 99, maxLon = -99;
        for (const auto& ring : swath) {
            for (const auto& [lon, lat] : ring) {
                minLat = std::min(minLat, lat); maxLat = std::max(maxLat, lat); minLon = std::min(minLon, lon); maxLon = std::max(maxLon, lon);
            }
        }
        CHECK(near(maxLat, 21.0, 1e-9) && near(minLat, 19.0, 1e-9));
        CHECK(minLon < -81.0 && maxLon > -77.0);   // 60 nm is about 1.06 degrees of longitude at 20N; the track runs from -80 to -77.6
        CHECK(UtilityAtcf::windSwath(track, 1, 3).empty());   // no 50 kt field
        const auto field = UtilityAtcf::windField(0.0, 0.0, {60, 0, 0, 0}, 4);
        CHECK(field.size() == 20 && near(field[0].second, 1.0, 1e-9) && near(field[4].first, 1.0, 1e-9));   // NE arc: due north 60 nm, due east 60 nm
    }
    CHECK(near(UtilityAtcf::coneRadiusNm(0), 0.0) && near(UtilityAtcf::coneRadiusNm(12), 25.0) && near(UtilityAtcf::coneRadiusNm(18), 32.0));
    CHECK(near(UtilityAtcf::coneRadiusNm(12, true), 25.0) && near(UtilityAtcf::coneRadiusNm(48, true), 56.0) && near(UtilityAtcf::coneRadiusNm(120, true), 138.0) && near(UtilityAtcf::coneRadiusNm(84, true), 92.0));
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
    CHECK(UtilityAtcf::formatTime("2026100718") == "2026-10-07 18Z");
    CHECK(UtilityAtcf::shortCategory(30) == "TD" && UtilityAtcf::shortCategory(34) == "TS" && UtilityAtcf::shortCategory(64) == "Cat 1" && UtilityAtcf::shortCategory(83) == "Cat 2");
    CHECK(UtilityAtcf::shortCategory(96) == "Cat 3" && UtilityAtcf::shortCategory(113) == "Cat 4" && UtilityAtcf::shortCategory(137) == "Cat 5" && UtilityAtcf::shortCategory(180) == "Cat 5");
    CHECK(UtilityAtcf::windLabel(85) == "85 kt (Cat 2)" && UtilityAtcf::windLabel(35) == "35 kt (TS)" && UtilityAtcf::windLabel(-1) == "-");
    CHECK(UtilityAtcf::addHours("2026123118", 6) == "2027010100" && UtilityAtcf::addHours("2026100700", 0) == "2026100700");
    CHECK(UtilityAtcf::addHours("2026030100", -1) == "2026022823" && UtilityAtcf::addHours("2024030100", -1) == "2024022923");   // leap year
    CHECK(UtilityAtcf::addHours("2026100700", 168) == "2026101400" && UtilityAtcf::addHours("2026100700", -72) == "2026100400");
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
    CHECK(UtilityHdob::timeText(a.seconds) == "2005-09-28 14:20Z");

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

// the Tropical Cyclone Plan of the Day (a real product: TCPOD 26-128)
static void pod(const std::string& fixtures) {
    const auto p = UtilityPod::parse(readFile(fixtures + "/reprpd_20261006.txt"));
    CHECK(p.ok && p.number == "26-128" && p.valid == "07/1100Z TO 08/1100Z OCTOBER 2026");
    CHECK(p.atlantic.size() == 1 && p.atlantic[0].title.rfind("SUSPECT AREA AL92", 0) == 0);
    CHECK(p.atlantic[0].flights.size() == 6 && !p.noAtlantic && p.noPacific && p.pacific.empty());
    const auto& f1 = p.atlantic[0].flights[0];
    CHECK(f1.ordinal == "ONE" && f1.aircraft == "TEAL 71" && f1.fixTimes == "07/1200Z" && f1.mission == "AFXXX 0209A CYCLONE" && f1.departure == "07/1000Z");
    CHECK(f1.position == "22.1N 94.1W" && f1.hasPosition && near(f1.lat, 22.1) && near(f1.lon, -94.1));
    CHECK(f1.onStation == "07/1130Z TO 07/1500Z" && f1.altitude == "SFC TO 10,000 FT" && f1.type == "FIX" && f1.wra == "WRA ACTIVATION" && f1.remarks == "RESOURCES PERMITTING");
    const auto& f2 = p.atlantic[0].flights[1];
    CHECK(f2.ordinal == "TWO" && f2.aircraft == "NOAA 43" && f2.fixTimes == "07/1800Z" && f2.type == "TAIL DOPPLER RADAR & FIX" && near(f2.lon, -93.5));
    const auto& f3 = p.atlantic[0].flights[2];
    CHECK(f3.ordinal == "THREE" && f3.aircraft == "NOAA 49" && !f3.hasPosition && f3.position == "NA" && f3.type == "SYNOPTIC SURVEILLANCE" && f3.wra == "NO WRA ACTIVATION");
    const auto& f4 = p.atlantic[0].flights[3];
    CHECK(f4.ordinal == "FOUR" && f4.aircraft == "TEAL 72" && f4.fixTimes == "07/2330Z,08/0530Z");
    CHECK(p.atlantic[0].flights[5].aircraft == "TEAL 73" && p.atlantic[0].flights[5].fixTimes == "08/1130Z,1730Z");
    CHECK(p.notes.size() >= 2 && p.notes[0].find("SUCCEEDING DAY OUTLOOK") != std::string::npos && p.notes[0].find("CONTINUE 6-HRLY FIXES INTO AL92") != std::string::npos);
    double lat = 0, lon = 0;
    CHECK(UtilityPod::parsePosition("12.5S 120.0E", lat, lon) && near(lat, -12.5) && near(lon, 120.0) && !UtilityPod::parsePosition("NA", lat, lon));
}

// Vortex data messages in the format of June 2018 onward (real ones: AL02 Bertha, July 2026)
static void vdm(const std::string& fixtures) {
    UtilityVdm::Vdm v;
    CHECK(UtilityVdm::parse(readFile(fixtures + "/vdm_202607220105.txt"), "202607220105", v));
    CHECK(v.stormId == "AL022026" && v.fixTime == "22/00:11:26Z" && !v.test);
    CHECK(near(v.lat, 29.34) && near(v.lon, -87.41) && v.levelMb == 700 && near(v.heightM, 3088) && near(v.pressure, 996) && !v.extrapolated);
    CHECK(near(v.centerWindDir, 193) && near(v.centerWindKt, 10));
    CHECK(v.eyeCharacter.empty() && !UtilityVdm::has(v.inboundSurface.kt));
    CHECK(near(v.inboundFlight.kt, 29) && near(v.inboundFlight.direction, 62) && near(v.inboundFlight.bearing, 329) && near(v.inboundFlight.rangeNm, 33) && v.inboundFlight.time == "00:03:45Z");
    CHECK(near(v.outboundFlight.kt, 51) && near(v.outboundFlight.bearing, 132) && near(v.outboundFlight.rangeNm, 93) && v.outboundFlight.time == "00:34:46Z");
    CHECK(near(v.tempOutsideC, 15) && near(v.tempInsideC, 16) && near(v.dewPointInsideC, 9) && !UtilityVdm::has(v.seaSurfaceC));
    CHECK(v.fixedBy == "1345 / 7" && v.accuracy == "0.01 / .1 nm" && v.aircraft == "NOAA3 0802A BERTHA OB 17" && near(v.maxFlightWind(), 51));
    CHECK(v.remarks == "MAX FL WIND 51 KT 132 / 93 NM 00:34:46Z");
    // 2026-07-22 00:11:26 UTC
    CHECK(v.seconds == 1784679086L);
    CHECK(UtilityVdm::parse(readFile(fixtures + "/vdm_202607212300.txt"), "202607212300", v));
    CHECK(v.extrapolated && near(v.pressure, 997) && v.fixTime == "21/22:18:11Z" && near(v.seaSurfaceC, -9999.0) && v.remarks.find("SLP EXTRAP FROM 700 MB") == 0);
    // a message early in the month that is filed after midnight: the fix day (31) is later than the file day (1), so it is the month before
    CHECK(UtilityVdm::parse("URNT12 KNHC 010005\nVORTEX DATA MESSAGE  AL012026\nA. 31/23:50:00Z\nB. 20.00 deg N 080.00 deg W\nU. AF300 0101A TEST OB 01\n", "202608010005", v));
    CHECK(v.seconds == 1785541800L);   // 2026-07-31 23:50:00 UTC
    CHECK(!UtilityVdm::parse("not a message", "202607220105", v));
    // the communications check that NHC sends now and then is recognised
    const std::string check = "URNT12 KWBC 290000\nVORTEX DATA MESSAGE AL992026\nA. 29/00:00:00Z\nB. 00.00 deg N 000.00 deg W\nU. NOAAX WXWXA TRAIN OB 99\nMAX FL WIND 00 KT 0 / 0 NM 00:00:00Z\nTEST TEST TEST COMM CHECK;\n";
    CHECK(UtilityVdm::parse(check, "202607290000", v) && v.test);
}

// seasons and ACE from HURDAT2 (real records for 2011 and 2025; the expected numbers were computed apart from this code, from the whole file)
static bool sameDays(const std::vector<std::pair<int, double>>& a, const std::vector<std::pair<int, double>>& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); i++) {
        if (a[i].first != b[i].first || std::abs(a[i].second - b[i].second) > 1e-9) return false;
    }
    return true;
}

static void season(const std::string& fixtures) {
    const auto storms = UtilitySeason::parseHurdat2(readFile(fixtures + "/hurdat2_2011_2025.txt"));
    const auto seasons = UtilitySeason::seasons(storms);
    CHECK(seasons.size() == 2 && seasons[0].year == 2011 && seasons[1].year == 2025);
    CHECK(seasons[0].cyclones == 20 && seasons[0].named == 19 && seasons[0].hurricanes == 7 && seasons[0].major == 4 && near(seasons[0].ace, 126.303, 0.001));
    CHECK(seasons[1].cyclones == 13 && seasons[1].named == 13 && seasons[1].hurricanes == 6 && seasons[1].major == 4 && near(seasons[1].ace, 130.773, 0.001));
    const UtilitySeason::Storm * irene = nullptr;
    for (const auto& s : storms) {
        if (s.id == "AL092011") irene = &s;
    }
    CHECK(irene != nullptr && irene->name == "IRENE" && irene->peakWind == 105 && irene->minPressure == 942 && irene->first == "2011082100" && irene->last == "2011083000" && irene->stormStrength);
    CHECK(UtilitySeason::categoryName(irene->peakWind) == "Category 3" && UtilitySeason::categoryName(30) == "Tropical depression");
    // a record off the synoptic hours (a landfall at 09:35) and a depression or extratropical record do not add
    CHECK(near(UtilitySeason::recordAce(12, "HU", 100), 1.0) && near(UtilitySeason::recordAce(1, "HU", 100), 0.0) && near(UtilitySeason::recordAce(0, "TD", 30), 0.0));
    CHECK(near(UtilitySeason::recordAce(6, "EX", 60), 0.0) && near(UtilitySeason::recordAce(6, "SS", 40), 0.16) && near(UtilitySeason::recordAce(18, "TS", 33), 0.0));
    // the cache form is lossless for what the table uses
    const auto again = UtilitySeason::fromCsv(UtilitySeason::csv(storms));
    CHECK(again.size() == storms.size() && sameDays(again[static_cast<size_t>(irene - storms.data())].daily, irene->daily) && !irene->daily.empty() && again[0].id == storms[0].id && near(again[5].ace, storms[5].ace, 1e-6) && UtilitySeason::seasons(again)[1].named == 13);
    CHECK(near(UtilitySeason::mean(seasons, 2011, 2025, &UtilitySeason::Season::named), 16.0));
    // ACE by day: the running total ends at the season's ACE, never falls, and the storm's own days add up to its ACE
    const auto c2011 = UtilitySeason::cumulativeByDay(storms, 2011);
    CHECK(c2011.size() == 367 && near(c2011[366], 126.303, 0.001) && near(c2011[0], 0.0) && c2011[100] == 0.0 && c2011[250] > c2011[200] && c2011[250] <= c2011[366]);
    for (size_t d = 1; d < c2011.size(); d++) {
        CHECK(c2011[d] >= c2011[d - 1]);
    }
    {   // ranking storms: the numbers agree with the season code's, and each order puts its own number first
        const auto hurdatTracks = UtilityHurdat::parse(readFile(fixtures + "/hurdat2_2011_2025.txt"));
        const UtilityHurdat::Track * ireneTrack = nullptr;
        for (const auto& t : hurdatTracks) {
            if (t.id == "AL092011") ireneTrack = &t;
        }
        CHECK(ireneTrack != nullptr && near(UtilityHurdat::ace(*ireneTrack), irene->ace, 1e-6));
        CHECK(ireneTrack != nullptr && UtilityHurdat::lengthKm(*ireneTrack) > 3000.0 && UtilityHurdat::lengthKm(*ireneTrack) < 9000.0);   // Irene: the Caribbean to New England
        CHECK(ireneTrack != nullptr && near(UtilityHurdat::durationDays(*ireneTrack), 9.0, 0.3));                                       // 2011-08-21 to 2011-08-30
        CHECK(UtilityHurdat::sortNames().size() == 7);
        using S = UtilityHurdat::Sort;
        UtilityHurdat::Track weak, strong;
        weak.peakWind = 40; weak.minPressure = 1000; weak.year = 2012;
        strong.peakWind = 120; strong.minPressure = 0; strong.year = 1999;   // no pressure given
        CHECK(UtilityHurdat::sortKey(strong, S::Strongest) > UtilityHurdat::sortKey(weak, S::Strongest));
        CHECK(UtilityHurdat::sortKey(weak, S::LowestPressure) > UtilityHurdat::sortKey(strong, S::LowestPressure));   // an unknown pressure ranks last
        CHECK(UtilityHurdat::sortKey(weak, S::Newest) > UtilityHurdat::sortKey(strong, S::Newest) && UtilityHurdat::sortKey(strong, S::Oldest) > UtilityHurdat::sortKey(weak, S::Oldest));
        CHECK(ireneTrack != nullptr && UtilityHurdat::sortNote(*ireneTrack, S::Ace).rfind("ACE ", 0) == 0 && UtilityHurdat::sortNote(*ireneTrack, S::Longest).find(" km") != std::string::npos &&
              UtilityHurdat::sortNote(*ireneTrack, S::Strongest).empty());
    }
    double irenePerDay = 0.0;
    for (const auto& [day, ace] : irene->daily) {
        irenePerDay += ace;
        CHECK(day >= 233 && day <= 242);   // 21 to 30 August 2011
    }
    CHECK(near(irenePerDay, irene->ace, 1e-9) && irene->daily.size() >= 8);
    const auto clim = UtilitySeason::climatology(storms, 2011, 2025);
    CHECK(clim.years == 15 && near(clim.mean[366], (126.303 + 130.773 + 0.0 * 13) / 15.0, 0.001) && near(clim.highest[366], 130.773, 0.001) && near(clim.lowest[366], 0.0));
    // TIKE: the energy of a record from its wind radii (hand-worked): 34 kt radius 100 nm, 50 kt 50 nm, 64 kt 25 nm in every quadrant, Vmax 100 kt
    {
        const std::array<std::array<int, 4>, 3> radii{{{100, 100, 100, 100}, {50, 50, 50, 50}, {25, 25, 25, 25}}};
        const double pi = 3.14159265358979;
        const double area34 = pi * 100.0 * 100.0, area50 = pi * 50.0 * 50.0, area64 = pi * 25.0 * 25.0;   // four quarter circles are one circle
        const double ms = 0.514444, nm2 = 1852.0 * 1852.0;
        const double expected = 0.5 * nm2 * ((area34 - area50) * std::pow(42.0 * ms, 2) + (area50 - area64) * std::pow(57.0 * ms, 2) + area64 * std::pow(82.0 * ms, 2)) / 1e12;
        CHECK(near(UtilitySeason::recordIke(12, "HU", 100, radii), expected, 1e-6) && expected > 10.0 && expected < 200.0);
        CHECK(near(UtilitySeason::recordIke(3, "HU", 100, radii), 0.0) && near(UtilitySeason::recordIke(12, "TD", 30, radii), 0.0) && near(UtilitySeason::recordIke(12, "EX", 60, radii), 0.0));
        const std::array<std::array<int, 4>, 3> none{};
        CHECK(near(UtilitySeason::recordIke(12, "TS", 40, none), 0.0));
    }
    // the TIKE of the fixture's storms: radii are in HURDAT2 from 2004, so these have it, and it runs up through the year
    {
        const auto tikeIrene = UtilitySeason::cumulativeByDay(storms, 2011, UtilitySeason::Metric::Tike);
        CHECK(irene->hasRadii && irene->tike > 20.0 && near(tikeIrene[366], UtilitySeason::seasons(storms)[0].tike, 1e-6) && tikeIrene[366] > tikeIrene[200]);
        double daySum = 0.0;
        for (const auto& [day, ike] : irene->dailyTike) daySum += ike;
        CHECK(near(daySum, irene->tike, 1e-9));
        const auto again2 = UtilitySeason::fromCsv(UtilitySeason::csv(storms));
        CHECK(near(again2[static_cast<size_t>(irene - storms.data())].tike, irene->tike, 1e-6) && again2[static_cast<size_t>(irene - storms.data())].hasRadii);
        // a record with -999 radii (before 2004) has no TIKE and is not counted as having radii
        const auto old = UtilitySeason::parseHurdat2("AL012001,             OLD,      1,\n20010601, 0000,  , TS, 25.0N,  90.0W,  45, 1000, -999, -999, -999, -999, -999, -999, -999, -999, -999, -999, -999, -999,\n");
        CHECK(old.size() == 1 && !old[0].hasRadii && near(old[0].tike, 0.0) && near(old[0].ace, 0.2025, 1e-6));
    }
    CHECK(UtilitySeason::dayOfYear("20110301") == 60 && UtilitySeason::dayOfYear("20120301") == 61 && UtilitySeason::dayOfYear("20111231") == 365 && UtilitySeason::dayOfYear("2011") == 0);
    // an ATCF best track in the same terms (storm strength at 06Z and 12Z: 35 kt and 40 kt; the 03Z record is not synoptic)
    std::vector<UtilityAtcf::Fix> best(4);
    const char * times[] = {"2026100700", "2026100703", "2026100706", "2026100712"};
    const int winds[] = {30, 32, 35, 40};
    for (size_t i = 0; i < 4; i++) {
        best[i].time = times[i];
        best[i].wind = winds[i];
        best[i].pressure = 1010 - static_cast<int>(i);
        best[i].status = i < 2 ? "TD" : "TS";
        best[i].name = i == 3 ? "ISAIAS" : "NINE";
    }
    const auto now = UtilitySeason::fromBestTrack(best, "al092026");
    CHECK(now.id == "AL092026" && now.name == "ISAIAS" && now.year == 2026 && now.peakWind == 40 && now.minPressure == 1007 && now.stormStrength && near(now.ace, (35.0 * 35 + 40.0 * 40) / 1e4));
    CHECK(UtilitySeason::newestHurdatFile("x hurdat2-1851-2024-040425.txt y hurdat2-1851-2025-02272026.txt hurdat2-1851-2025-091226.txt hurdat2-1851-2025-092326.txt z hurdat2-nepac-1949-2025-092926.txt") ==
          "hurdat2-1851-2025-092326.txt");
    CHECK(UtilitySeason::newestHurdatFile("hurdat2-nepac-1949-2024-031725.txt hurdat2-nepac-1949-2025-02272026.txt hurdat2-nepac-1949-2025-091426.txt hurdat2-nepac-1949-2025-092926.txt hurdat2-1851-2025-092326.txt", "hurdat2-nepac-1949") ==
          "hurdat2-nepac-1949-2025-092926.txt");
}

// NHC's GIS products (NHC's sample files for Irma, advisory 20, 2017) and the zip reader under them; the expected numbers were read with an independent script
static void gis(const std::string& fixtures) {
    std::map<std::string, std::string> files;
    CHECK(UtilityZip::read(readFile(fixtures + "/nhc_5day_al112017_020.zip"), files) && files.size() == 15 && files.contains("al112017-020_5day_pgn.shp"));
    CHECK(files.at("al112017-020_5day_pgn.dbf").size() == 608 && files.at("al112017-020_5day_pgn.shp").size() == 37884);
    CHECK(!UtilityZip::read("not a zip file at all, really not a zip", files));
    const auto cone = UtilityNhcGis::parseCone(readFile(fixtures + "/nhc_5day_al112017_020.zip"));
    CHECK(cone.ok && cone.polygons.size() == 1 && cone.polygons[0].size() == 2358 && near(cone.polygons[0][0].first, -57.17042) && near(cone.polygons[0][0].second, 15.92974));
    CHECK(cone.stormName == "Irma" && cone.advisory == "20" && cone.advisoryDate == "500 AM AST Mon Sep 04 2017");
    CHECK(cone.lines.size() == 1 && cone.lines[0].size() == 9 && near(cone.lines[0][0].first, -52.3) && near(cone.lines[0][0].second, 16.9));
    CHECK(cone.points.size() == 9 && cone.points[0].tau == 0 && near(cone.points[0].maxWind, 100.0) && cone.points[0].development == "Major Hurricane" && cone.points[0].label == "5:00 AM Mon");
    CHECK(cone.points[8].tau == 120 && near(cone.points[8].maxWind, 110.0) && near(cone.points[8].lon, -76.5) && near(cone.points[8].lat, 22.0));
    const auto radii = UtilityNhcGis::parseRadii(readFile(fixtures + "/nhc_fcst_al112017_020.zip"));
    CHECK(radii.size() == 21);   // 3 initial + 18 forecast
    CHECK(radii[0].knots == 34 && radii[0].tau == 0 && near(radii[0].ne, 120) && near(radii[0].se, 80) && near(radii[0].sw, 50) && near(radii[0].nw, 90));
    CHECK(radii.back().knots == 50 && radii.back().tau == 72 && near(radii.back().ne, 80) && near(radii.back().nw, 80));
    const auto ww = UtilityNhcGis::parseWatchWarnings(readFile(fixtures + "/nhc_ww_al112017_020.kmz"));
    CHECK(ww.size() == 3 && ww[0].kind == "Hurricane Watch" && ww[0].code == "HWA" && ww[0].lines.size() == 1 && ww[0].lines[0].size() == 2);
    CHECK(near(ww[0].lines[0][0].first, -62.83) && near(ww[0].lines[0][0].second, 17.87) && near(ww[0].lines[0][1].first, -63.32) && near(ww[0].lines[0][1].second, 18.3));
    // the Tropical Weather Outlook shapefiles: today's (two Pacific areas) and NHC's 2023 sample (one Atlantic area at 70 %)
    const auto outlook = UtilityNhcGis::parseOutlook(readFile(fixtures + "/nhc_gtwo_20261007.zip"));
    CHECK(outlook.size() == 2 && outlook[0].basin == "Pacific" && outlook[0].area == "1" && outlook[0].prob2 == 90 && outlook[0].prob7 == 90 && outlook[0].risk2 == "High" && outlook[0].risk7 == "High");
    CHECK(outlook[0].rings.size() == 1 && outlook[0].rings[0].size() == 300 && near(outlook[0].centerLon, (-108.04 + -97.01) / 2.0, 0.01) && near(outlook[0].centerLat, (11.16 + 18.01) / 2.0, 0.01));
    CHECK(outlook[1].area == "2" && outlook[1].prob2 == 0 && outlook[1].prob7 == 30 && outlook[1].risk2 == "Low" && outlook[1].risk7 == "Low");
    const auto old = UtilityNhcGis::parseOutlook(readFile(fixtures + "/nhc_gtwo_example_2023.zip"));
    CHECK(old.size() == 1 && old[0].basin == "Atlantic" && old[0].prob2 == 70 && old[0].rings[0].size() == 72 && UtilityNhcGis::parseOutlook("not a zip").empty());
    CHECK(UtilityNhcGis::nameFor("TWR") == "Tropical Storm Warning" && UtilityNhcGis::colorFor("HWR") == "#ff0000");
    CHECK(UtilityNhcGis::parseWatchWarnings("<kml><Placemark><name>x</name><styleUrl>#TWA</styleUrl><LineString><coordinates>-80.0,25.0,0 -81.0,26.0,0</coordinates></LineString></Placemark></kml>").size() == 1);
}

// NHC's text products (the public advisory for Isaias, advisory 3) and the comparison between two advisories
static void text(const std::string& fixtures) {
    const auto b = UtilityNhcText::bulletin(readFile(fixtures + "/nhc_tcp_al09_003.html"));
    CHECK(b.rfind("000\nWTNT34 KNHC 070852", 0) == 0 && b.find("Tropical Storm Isaias Advisory Number   3") != std::string::npos && b.find("<") == std::string::npos);
    CHECK(UtilityNhcText::headline(b) == "DEPRESSION BECOMES TROPICAL STORM ISAIAS; FORECAST TO RAPIDLY STRENGTHEN OVER THE NEXT COUPLE OF DAYS");
    const auto sections = UtilityNhcText::sections(b);
    CHECK(sections.size() >= 3 && sections[0].first == "Header" && sections[1].first == "SUMMARY OF 400 AM CDT...0900 UTC...INFORMATION");
    CHECK(sections[1].second.find("MINIMUM CENTRAL PRESSURE...1004 MB") != std::string::npos && sections[2].first == "WATCHES AND WARNINGS");
    CHECK(UtilityNhcText::bulletin("<html>no bulletin</html>").empty() && UtilityNhcText::bulletin("<pre>a &lt; b &amp; c</pre>") == "a < b & c");

    UtilityChanges::Snapshot before;
    before.advisory = "002";
    before.classification = "TD";
    before.wind = 30;
    before.pressure = 1005;
    before.lat = 22.1;
    before.lon = -95.0;
    before.moveDir = 80;
    before.moveSpeed = 5;
    before.forecastPeak = 80;
    before.forecastPeakHour = 60;
    before.ri30 = 9.0;
    auto after = before;
    after.advisory = "003";
    after.classification = "TS";
    after.wind = 35;
    after.pressure = 1004;
    after.lat = 22.0;
    after.lon = -94.1;
    after.moveDir = 75;
    after.moveSpeed = 8;
    after.forecastPeak = 95;
    after.forecastPeakHour = 48;
    after.ri30 = 13.0;
    const auto lines = UtilityChanges::describe(before, after);
    CHECK(lines.size() == 7 && lines[0] == "Classification TD to TS" && lines[1] == "Winds 30 kt (TD) to 35 kt (TS) (+5 kt)" && lines[2] == "Pressure 1005 to 1004 mb (-1)");
    CHECK(lines[3].rfind("Centre moved 9", 0) == 0 && lines[3].find("toward 9") != std::string::npos);   // about 93 km east
    CHECK(lines[4] == "Motion 80 deg at 5 kt to 75 deg at 8 kt" && lines[5] == "NHC forecast peak 80 kt (Cat 1) at 60 h to 95 kt (Cat 2) at 48 h");
    CHECK(lines[6] == "SHIPS-RII chance of a 30 kt rise in 24 h 9 % to 13 %");
    CHECK(UtilityChanges::describe(after, after).empty() && UtilityChanges::describe(UtilityChanges::Snapshot{}, after).empty());
    const auto round = UtilityChanges::parse(UtilityChanges::serialize(after));
    CHECK(round.advisory == "003" && round.classification == "TS" && round.wind == 35 && round.pressure == 1004 && near(round.lat, 22.0) && near(round.lon, -94.1) && round.moveDir == 75 && round.forecastPeak == 95 && near(round.ri30, 13.0));
    CHECK(near(UtilityChanges::distanceKm(0, 0, 0, 1), 111.19, 0.1) && near(UtilityChanges::bearing(0, 0, 1, 0), 0.0) && near(UtilityChanges::bearing(0, 0, 0, 1), 90.0));
}

// Recon dropsondes in the WMO TEMP DROP code (real messages: NOAA9 over the Gulf on 7 October 2026 and NOAA2 into Rachel on 30 September 2026); the expected values were
// decoded by hand from the code tables of NOAA AOML's "NHOP sonde drop format" page
static void drop(const std::string& fixtures) {
    const auto d = UtilityDropsonde::parse(readFile(fixtures + "/drop_202610070050.txt"), "202610070050");
    CHECK(d.ok && near(d.lat, 28.1) && near(d.lon, -84.6) && d.seconds == 1791333000L && d.windInKnots);   // 2026-10-07 00:30 UTC (31313: 80030)
    CHECK(d.mission == "NOAA9 01BBA SURV OB 32");
    CHECK(near(d.releaseLat, 28.13) && near(d.releaseLon, -84.65) && near(d.splashLat, 28.12) && near(d.splashLon, -84.53));
    CHECK(near(d.mblDirection, 200) && near(d.mblSpeed, 13) && d.remarks.find("LAST REPORT DLM WND 27519 008148 WL150") != std::string::npos);   // the wrapped line is rejoined
    const auto at = [&] (double p) -> const UtilityDropsonde::Level * {
        for (const auto& l : d.levels) {
            if (near(l.pressure, p, 0.01)) return &l;
        }
        return nullptr;
    };
    const auto * sfc = d.surface();
    CHECK(sfc != nullptr && near(sfc->pressure, 1009) && near(sfc->temperature, 26.6) && near(sfc->dewPoint, 24.5) && near(sfc->windDirection, 180) && near(sfc->windSpeed, 13));
    const auto * p1000 = at(1000);
    CHECK(p1000 != nullptr && near(p1000->height, 78) && near(p1000->temperature, 26.0) && near(p1000->dewPoint, 24.2) && near(p1000->windDirection, 185) && near(p1000->windSpeed, 15));
    const auto * p925 = at(925);
    CHECK(p925 != nullptr && near(p925->height, 763) && near(p925->temperature, 21.6) && near(p925->dewPoint, 19.7) && near(p925->windDirection, 240) && near(p925->windSpeed, 17));
    const auto * p850 = at(850);
    CHECK(p850 != nullptr && near(p850->height, 1496) && near(p850->temperature, 18.8) && near(p850->dewPoint, 15.7) && near(p850->windDirection, 285) && near(p850->windSpeed, 12));
    const auto * p700 = at(700);
    CHECK(p700 != nullptr && near(p700->height, 3141) && near(p700->temperature, 9.6) && near(p700->windDirection, 295) && near(p700->windSpeed, 19));
    const auto * p500 = at(500);
    CHECK(p500 != nullptr && near(p500->height, 5860) && near(p500->temperature, -4.5) && near(p500->dewPoint, -5.0) && near(p500->windDirection, 275) && near(p500->windSpeed, 18));   // odd tenths: below zero
    const auto * p400 = at(400);
    CHECK(p400 != nullptr && near(p400->height, 7600) && near(p400->temperature, -13.7));
    const auto * p300 = at(300);
    CHECK(p300 != nullptr && near(p300->height, 9730) && near(p300->temperature, -27.9) && near(p300->windDirection, 230) && near(p300->windSpeed, 21));
    const auto * p250 = at(250);
    CHECK(p250 != nullptr && near(p250->height, 11010) && near(p250->temperature, -38.9) && near(p250->dewPoint, -49.9) && near(p250->windSpeed, 20));   // a dew point depression code of 61 is 11 degrees
    const auto * p200 = at(200);
    CHECK(p200 != nullptr && near(p200->height, 12490) && near(p200->temperature, -52.3) && near(p200->windDirection, 295) && near(p200->windSpeed, 42));
    const auto * p150 = at(150);
    CHECK(p150 != nullptr && near(p150->height, 14280) && !UtilityDropsonde::has(p150->temperature) && near(p150->windDirection, 275) && near(p150->windSpeed, 22));   // ///// is missing
    // significant levels (XXBB) and significant wind levels (21212) are merged in, the lowest level first
    const auto * s930 = at(930);
    CHECK(s930 != nullptr && near(s930->temperature, 21.6) && near(s930->dewPoint, 21.6) && !UtilityDropsonde::has(s930->height));
    const auto * w998 = at(998);
    CHECK(w998 != nullptr && near(w998->windDirection, 190) && near(w998->windSpeed, 16) && !UtilityDropsonde::has(w998->temperature));
    const auto * s484 = at(484);
    CHECK(s484 != nullptr && near(s484->temperature, -5.5) && near(s484->dewPoint, -6.2));
    bool ordered = d.levels.size() > 30;
    for (size_t i = 1; i < d.levels.size(); i++) {
        ordered = ordered && d.levels[i].pressure < d.levels[i - 1].pressure;
    }
    CHECK(ordered);

    // the East Pacific drop into Rachel: winds only down to 850 hPa (Id = 8), so the 700 hPa group has two parts, not three
    const auto e = UtilityDropsonde::parse(readFile(fixtures + "/drop_ep_202609301950.txt"), "202609301950");
    CHECK(e.ok && near(e.lat, 19.4) && near(e.lon, -106.7) && e.seconds == 1790796840L);   // 2026-09-30 19:34 UTC
    CHECK(near(e.releaseLat, 19.42) && near(e.releaseLon, -106.68) && near(e.splashLat, 19.47) && near(e.splashLon, -106.72) && near(e.mblDirection, 125) && near(e.mblSpeed, 51));
    const auto * e850 = [&] () -> const UtilityDropsonde::Level * { for (const auto& l : e.levels) if (near(l.pressure, 850, 0.01)) return &l; return nullptr; }();
    const auto * e700 = [&] () -> const UtilityDropsonde::Level * { for (const auto& l : e.levels) if (near(l.pressure, 700, 0.01)) return &l; return nullptr; }();
    CHECK(e850 != nullptr && near(e850->height, 1435) && near(e850->temperature, 17.8) && near(e850->windDirection, 145) && near(e850->windSpeed, 61));
    CHECK(e700 != nullptr && near(e700->height, 3087) && !UtilityDropsonde::has(e700->temperature) && !UtilityDropsonde::has(e700->windSpeed));
    CHECK(e.surface() != nullptr && near(e.surface()->pressure, 1001) && near(e.surface()->temperature, 28.8) && near(e.surface()->dewPoint, 22.8) && near(e.surface()->windSpeed, 44));
    CHECK(near(UtilityDropsonde::minimumPressure(e), e.surfacePressure) && UtilityDropsonde::has(UtilityDropsonde::maxWind(e)) && UtilityDropsonde::maxWind(e) >= 44.0);
    CHECK(!UtilityDropsonde::has(UtilityDropsonde::maxWind(UtilityDropsonde::Drop{})) && !UtilityDropsonde::has(UtilityDropsonde::minimumPressure(UtilityDropsonde::Drop{})));
    CHECK(!UtilityDropsonde::parse("not a drop", "202610070050").ok);
}

static void windProbability(const std::string& fixtures) {
    const auto map = UtilityWindProbability::parse(readFile(fixtures + "/wsp_120hr5km.zip"));
    CHECK(map.ok && map.cycle == "2026100806" && map.bands[0].size() == 11 && map.bands[1].size() == 11 && map.bands[2].size() == 11);
    CHECK(map.bands[0][0].low == 0 && map.bands[0][0].high == 5 && map.bands[0][10].low == 90 && map.bands[0][10].high == 100 && map.bands[0][3].low == 20 && map.bands[0][3].high == 30);
    // 8 October 2026: Isaias, west of Florida, in the Gulf; the 06Z cycle's five day probabilities
    CHECK(UtilityWindProbability::at(map, 0, 25.0, -90.0).text() == ">90%" && UtilityWindProbability::at(map, 1, 25.0, -90.0).text() == "50-60%");
    CHECK(UtilityWindProbability::at(map, 0, 30.16, -85.66).text() == "30-40%" && UtilityWindProbability::at(map, 2, 30.16, -85.66).text() == "<5%");
    CHECK(UtilityWindProbability::at(map, 0, 35.47, -97.52).text() == "none" && !UtilityWindProbability::at(map, 0, 35.47, -97.52).covered);
    CHECK(UtilityWindProbability::at(map, 5, 25.0, -90.0).text() == "none");
    int low = 0, high = 0;
    CHECK(UtilityWindProbability::parseBand("5-10%", low, high) && low == 5 && high == 10 && UtilityWindProbability::parseBand("<5%", low, high) && low == 0 && high == 5 &&
          UtilityWindProbability::parseBand(">90%", low, high) && low == 90 && high == 100 && !UtilityWindProbability::parseBand("none", low, high));
    CHECK(!UtilityWindProbability::parse("not a zip").ok);
    const std::vector<std::pair<double, double>> square{{0, 0}, {10, 0}, {10, 10}, {0, 10}};
    CHECK(UtilityWindProbability::inside(square, 5, 5) && !UtilityWindProbability::inside(square, 5, 15) && !UtilityWindProbability::inside(square, -1, 5));
}

static void hurdat(const std::string& fixtures) {
    const auto tracks = UtilityHurdat::parse(readFile(fixtures + "/hurdat2_2011_2025.txt"));
    const auto storms = UtilitySeason::parseHurdat2(readFile(fixtures + "/hurdat2_2011_2025.txt"));
    CHECK(tracks.size() == storms.size() && tracks.size() == 33);
    const UtilityHurdat::Track * irene = nullptr;
    for (const auto& t : tracks) {
        if (t.id == "AL092011") irene = &t;
    }
    CHECK(irene != nullptr && irene->name == "IRENE" && irene->year == 2011 && irene->peakWind == 105 && irene->minPressure == 942 && irene->stormStrength);
    CHECK(irene->points.size() > 30 && irene->points.front().time == "2011082100" && irene->points.back().time == "2011083000" && irene->points.front().lon < 0.0 && irene->points.front().lat > 10.0);
    for (size_t i = 0; i < tracks.size(); i++) {
        CHECK(tracks[i].id == storms[i].id && tracks[i].peakWind == storms[i].peakWind && tracks[i].minPressure == storms[i].minPressure && tracks[i].stormStrength == storms[i].stormStrength);
    }
    double degrees = 0.0;
    CHECK(UtilityHurdat::parseCoordinate("39.4N", degrees) && near(degrees, 39.4) && UtilityHurdat::parseCoordinate("74.4W", degrees) && near(degrees, -74.4) &&
          UtilityHurdat::parseCoordinate("12.0S", degrees) && near(degrees, -12.0) && UtilityHurdat::parseCoordinate("179.5E", degrees) && near(degrees, 179.5) && !UtilityHurdat::parseCoordinate("74.4", degrees));
}

static void strikes() {
    // three members moving east along 30N: one hits (passing 20 km south of the point at 60 kt), one passes 300 km away, one is too weak
    UtilityEcmwfTracks::Storm storm;
    const auto member = [] (int type, double lat, double wind) {
        UtilityEcmwfTracks::Member m;
        m.type = type;
        for (int h = 0; h <= 48; h += 6) {
            UtilityEcmwfTracks::Step s;
            s.hour = h;
            s.lat = lat;
            s.lon = -90.0 + h * 0.25;   // 1 degree of longitude every 4 hours
            s.wind = wind;
            m.steps.push_back(s);
        }
        return m;
    };
    storm.members = {member(4, 29.8, 60.0), member(4, 32.7, 60.0), member(4, 29.8, 25.0), member(0, 30.0, 90.0)};
    const auto s = UtilityEnsembleStats::strike(storm, 30.0, -84.0, 100.0, 34.0);
    CHECK(s.members == 3 && s.hits == 1 && near(s.share(), 1.0 / 3.0) && near(s.medianHour, 24.0, 0.7));
    const auto weak = UtilityEnsembleStats::strike(storm, 30.0, -84.0, 100.0, 20.0);
    CHECK(weak.hits == 2);
    const auto far = UtilityEnsembleStats::strike(storm, 30.0, -60.0, 100.0, 34.0);   // beyond the end of the tracks
    CHECK(far.hits == 0 && !UtilityEcmwfTracks::has(far.medianHour));
}

int main(int argc, char ** argv) {
    windProbability(argc > 1 ? argv[1] : "tests/hurricane/fixtures");
    strikes();
    hurdat(argc > 1 ? argv[1] : "tests/hurricane/fixtures");
    drop(argc > 1 ? argv[1] : "tests/hurricane/fixtures");
    text(argc > 1 ? argv[1] : "tests/hurricane/fixtures");
    gis(argc > 1 ? argv[1] : "tests/hurricane/fixtures");
    season(argc > 1 ? argv[1] : "tests/hurricane/fixtures");
    vdm(argc > 1 ? argv[1] : "tests/hurricane/fixtures");
    pod(argc > 1 ? argv[1] : "tests/hurricane/fixtures");
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
