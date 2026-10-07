// Checks the pure parsers against rows copied from live NHC files (2026-10-07) and from the HDOB specification's own sample.
#include <cmath>
#include <cstdlib>
#include <iostream>
#include "hurricane/UtilityAtcf.h"
#include "hurricane/UtilityHdob.h"
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

int main() {
    atcf();
    hdob();
    gzip();
    if (failures == 0) {
        std::cout << "all hurricane parser tests passed\n";
    }
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
