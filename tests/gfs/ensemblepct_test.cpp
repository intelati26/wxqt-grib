// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

// The percentiles of an ensemble at a place (cfs/EnsemblePercentiles): the percentile rule, the fan, the running total, the chance in a block and the parse of the service's answer, on made-up numbers.
#include <cmath>
#include <cstdio>
#include "cfs/EnsemblePercentiles.h"
#include "misc/NbmText.h"

namespace E = EnsemblePercentiles;
static int failures = 0;
#define CHECK(x) do { if (!(x)) { std::printf("FAILED line %d: %s\n", __LINE__, #x); failures++; } } while (0)
static bool near(float a, double b, double eps = 1e-4) { return std::fabs(static_cast<double>(a) - b) < eps; }

int main() {
    // the rule: sorted, (n-1) positions, linear between; NaNs left out
    CHECK(near(E::percentile({1, 2, 3, 4}, 50), 2.5) && near(E::percentile({4, 1, 3, 2}, 0), 1) && near(E::percentile({4, 1, 3, 2}, 100), 4));
    CHECK(near(E::percentile({1, 2, 3, 4, 5}, 25), 2) && near(E::percentile({1, 2, 3, 4, 5}, 90), 4.6));
    CHECK(near(E::percentile({std::nanf(""), 7.0f, 9.0f}, 50), 8) && std::isnan(E::percentile({}, 50)) && std::isnan(E::percentile({std::nanf("")}, 50)) && near(E::percentile({5}, 70), 5));

    // five members, three times; the last time has one member only (no percentile then), the middle one has four
    E::Series s;
    s.times = {0, 3600, 7200};
    s.members = {{1, 10, 5}, {2, 20, std::nanf("")}, {3, 30, std::nanf("")}, {4, std::nanf(""), std::nanf("")}, {5, 50, std::nanf("")}};
    const auto fan = E::fan(s);
    CHECK(near(fan.value.at(50)[0], 3) && near(fan.value.at(0)[0], 1) && near(fan.value.at(100)[0], 5) && near(fan.mean[0], 3) && fan.count[0] == 5);
    CHECK(fan.count[1] == 4 && near(fan.value.at(50)[1], 25) && near(fan.value.at(10)[1], 13) && near(fan.mean[1], 27.5));   // 10, 20, 30, 50: the 10th is 10 + 0.3 * 10
    CHECK(fan.count[2] == 1 && std::isnan(fan.value.at(50)[2]) && std::isnan(fan.mean[2]));

    // running totals: a member with a gap has no total after it
    E::Series h;
    h.times = {3600, 7200, 10800};
    h.members = {{1, 2, 3}, {0, std::nanf(""), 4}};
    const auto acc = E::accumulated(h);
    CHECK(near(acc.members[0][2], 6) && near(acc.members[0][0], 1) && near(acc.members[1][0], 0) && std::isnan(acc.members[1][1]) && std::isnan(acc.members[1][2]));

    // the chance of a block's total: 3-hour blocks of hourly values ending 03, 06 (UTC); hours at 01..06
    E::Series r;
    for (int i = 1; i <= 6; i++) {
        r.times.push_back(i * 3600.0);
    }
    r.members = {{0.1f, 0.1f, 0.1f, 0.0f, 0.0f, 0.0f},      // 0.3, 0.0
                 {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f},      // 0, 0
                 {0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f},      // 0, 3
                 {0.0f, 0.0f, 0.5f, 0.2f, 0.0f, 0.0f}};     // 0.5, 0.2
    const auto c = E::chanceOfAtLeast(r, 3, 0.25);
    CHECK(c.times.size() == 2 && near(c.times[0], 3 * 3600) && near(c.times[1], 6 * 3600));
    CHECK(near(c.share[0], 0.5) && near(c.share[1], 0.25));   // block 1: 0.3, 0, 0, 0.5 -> two of four; block 2: 0, 0, 3, 0.2 -> one of four
    CHECK(near(c.median[0], 0.15) && near(c.high[1], 2.16));
    E::Series partial = r;   // an hour missing from one member leaves that member out of the block
    partial.members[1][4] = std::nanf("");
    const auto d = E::chanceOfAtLeast(partial, 3, 0.25);
    CHECK(d.times.size() == 2 && near(d.share[1], 1.0 / 3.0));

    // the service's answer: the control and the numbered members, null for a missing value, an error with its reason
    const QByteArray good = R"({"hourly_units":{"time":"unixtime","temperature_2m":"°C"},"hourly":{"time":[100,200,300],"temperature_2m":[1.5,2.5,null],"temperature_2m_member01":[1,2,3],"temperature_2m_member02":[2,3,4],"precipitation":[0,0,0]}})";
    E::Series t;
    std::string why;
    CHECK(E::parse(good, "temperature_2m", t, why) && t.members.size() == 3 && t.times.size() == 3 && std::isnan(t.members[0][2]) && t.units == std::string{"°C"} && near(t.members[1][0], 1));
    CHECK(!E::parse(good, "precipitation", t, why) && why.find("fewer than two") != std::string::npos);   // a single column is not an ensemble
    CHECK(!E::parse(R"({"error":true,"reason":"Latitude must be in range of -90 to 90"})", "temperature_2m", t, why) && why.find("Latitude") != std::string::npos);
    CHECK(!E::parse("not json", "temperature_2m", t, why) && !E::parse(R"({"hourly":{"time":[]}})", "temperature_2m", t, why));
    CHECK(E::url(39.1, -94.6, "gfs025", {"temperature_2m", "precipitation"}) == "https://ensemble-api.open-meteo.com/v1/ensemble?latitude=39.100&longitude=-94.600&hourly=temperature_2m,precipitation&models=gfs025&forecast_days=16&timeformat=unixtime");
    CHECK(E::models().size() >= 4 && E::models().front().id == "gfs025");
    // the NWS blend's probabilistic bulletin (NBP): five character codes, 12 hourly columns split by bars, values by position under the FHR line
    const std::string nbp =
        " KMCI    NBM  NBP GUIDANCE    11/15/2026  1200 UTC\n"
        "    MON 16| TUE 17| WED 18| THU 19| FRI 20| SAT 21| SUN 22| MON 23| TUE 24| \n"
        " UTC    12| 00  12| 00  12| 00  12| 00  12| 00  12| 00  12| 00  12| 00  12| 00 \n"
        " FHR    24| 36  48| 60  72| 84  96|108 120|132 144|156 168|180 192|204 216|228\n"
        " TXNMN  36| 58  40| 60  46| 64  51| 65  53| 69  55| 65  42| 54  36| 51  36| 51\n"
        " TXNP1  33| 56  37| 58  43| 61  48| 61  48| 65  52| 61  34| 45  25| 40  25| 40\n"
        " TXNP5  36| 58  39| 61  45| 64  52| 66  54| 69  56| 65  41| 54  35| 50  35| 50\n"
        " TXNP9  38| 61  42| 63  48| 66  55| 70  56| 73  60| 70  52| 62  47| 62  47| 62\n"
        " Q24P1    |      0|      0|      0|      0|      0|      0|      0|      0|   \n"
        " Q24P9    |      0|      0|     16|      7|     63|    128|      4|     40|   \n"
        " SLPP1 032|029 029|024 022|018 020|021 019|013 008|007 012|015 017|017 019|018\n"
        "                                                                          \n"
        " KMCJ    NBM  NBP GUIDANCE    11/15/2026  1200 UTC\n";
    const auto block = NbmText::parse(nbp);
    CHECK(block.kind == "NBP" && block.station == "KMCI" && block.columns.size() == 18);
    CHECK(block.columns[0].time == block.cycle + 24 * 3600 && block.columns[1].time == block.cycle + 36 * 3600 && block.columns[17].time == block.cycle + 228 * 3600);
    CHECK(block.columns[0].get("TXNP1") == 33 && block.columns[1].get("TXNP9") == 61 && block.columns[17].get("TXNMN") == 51 && block.columns[16].get("TXNP5") == 35);
    CHECK(!block.columns[0].has("Q24P1") && !block.columns[1].has("Q24P1") && block.columns[2].get("Q24P1") == 0 && block.columns[6].get("Q24P9") == 16 && block.columns[12].get("Q24P9") == 128 && block.columns[16].get("Q24P9") == 40);   // (the 24 hour precipitation ends at 12 UTC: its values are under the 12 UTC columns, hundredths of an inch)
    CHECK(block.columns[0].get("SLPP1") == 32 && block.columns[1].get("SLPP1") == 29);
    std::printf(failures == 0 ? "all ensemble percentile tests passed\n" : "%d failures\n", failures);
    return failures == 0 ? 0 : 1;
}
