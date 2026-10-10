// Tests of the storm track parser (the hurricane model's ATCF file) and the crop to a track.
#include <cmath>
#include <cstdio>
#include "gfs/GfsChart.h"

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL line %d: %s\n", __LINE__, #c); failures++; } } while (0)

int main() {
    // three thresholds at hour 0, one line at hour 6 (a weaker storm has no 50 or 64 kt radii)
    const std::string text =
        "AL, 09, 2026100812, 03, HFSA, 000, 235N,  906W,  72,  975, XX,  34, NEQ, 0107, 0087, 0079, 0092,  995,   27,  16,   0,   0,    ,   0,    ,  70,   6,           ,  ,   ,    ,   0,   0,   0,   0,       THERMO PARAMS\n"
        "AL, 09, 2026100812, 03, HFSA, 000, 235N,  906W,  72,  975, XX,  50, NEQ, 0031, 0034, 0037, 0045,  995,   27,  16,   0,   0,    ,   0,    ,  70,   6,\n"
        "AL, 09, 2026100812, 03, HFSA, 000, 235N,  906W,  72,  975, XX,  64, NEQ, 0016, 0023, 0019, 0016,  995,   27,  16,   0,   0,    ,   0,    ,  70,   6,\n"
        "AL, 09, 2026100812, 03, HFSA, 006, 245N,  895W,  80,  968, XX,  34, NEQ, 0120, 0100, 0090, 0100,  995,   27,  16,   0,   0,    ,   0,    ,  70,   6,\n"
        "not a track line\n";
    const auto track = GfsChart::parseTrack(text);
    CHECK(track.size() == 2);
    CHECK(track[0].hour == 0 && track[1].hour == 6);
    CHECK(std::abs(track[0].lat - 23.5) < 1e-9 && std::abs(track[0].lon + 90.6) < 1e-9);
    CHECK(track[0].wind == 72 && track[0].pressure == 975);
    CHECK(track[0].radii[0][0] == 107 && track[0].radii[0][3] == 92);
    CHECK(track[0].radii[1][1] == 34 && track[0].radii[2][2] == 19);
    CHECK(track[1].radii[0][0] == 120 && track[1].radii[1][0] == 0);
    CHECK(GfsChart::parseTrack("").empty());
    // the southern hemisphere and the east are positive / negative the other way round
    const auto south = GfsChart::parseTrack("SH, 05, 2026010100, 03, HFSA, 000, 155S, 1500E, 50, 990, XX, 34, NEQ, 0050, 0050, 0050, 0050,\n");
    CHECK(south.size() == 1 && south[0].lat < 0.0 && south[0].lon > 0.0);
    // the crop: the track up to the hour, a margin round it, never wider than the data and never narrower than 8 degrees
    const GfsChart::Sector within{"x", -120, 0, -60, 50};
    const auto crop = GfsChart::cropToTrack(within, track, 6, 4.0);
    CHECK(crop.west >= within.west && crop.east <= within.east && crop.east - crop.west >= 8.0 - 1e-9 && crop.north - crop.south >= 8.0 - 1e-9);
    CHECK(crop.west < -90.6 && crop.east > -89.5 && crop.south < 23.5 && crop.north > 24.5);
    const auto none = GfsChart::cropToTrack(within, {}, 6, 4.0);
    CHECK(none.west == within.west && none.north == within.north);
    std::printf(failures ? "%d failures\n" : "all track tests passed\n", failures);
    return failures ? 1 : 0;
}
