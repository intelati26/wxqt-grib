// The Gross to Miserable score of an hour (src/gfs/GrossScore.h) against the user's specification: the expected scores below are what the specification's own Python function
// (gross-weather-index-formula.md, calculate_gross_weather_index) gives for the same hours; the lines moved by the normal are ours (the user, 2026-11-16).
#include <cmath>
#include <cstdio>
#include "gfs/GrossScore.h"

namespace {
    int failures = 0;
    void expect(const char * what, double got, double want, double tolerance = 0.05) {
        if (!(std::fabs(got - want) <= tolerance)) {
            std::printf("FAIL %s: %g, want %g\n", what, got, want);
            failures++;
        }
    }
    GrossScore::Hour hour(double temp, double feels, double humidity, double dew, double wind, double rain = std::nan(""), double icy = std::nan("")) {
        GrossScore::Hour h;
        h.temp = temp;
        h.feels = feels;
        h.humidity = humidity;
        h.dew = dew;
        h.wind = wind;
        h.rainChance = rain;
        h.icyChance = icy;
        return h;
    }
}

int main() {
    // the specification's function, the same hours (its lines 45 and 85 F)
    expect("mild", GrossScore::score(hour(72, 72, 40, 50, 5)), 0.0);
    expect("sticky", GrossScore::score(hour(90, 98, 70, 72, 8)), 37.0);
    expect("heat index 108 with a 68 dewpoint", GrossScore::score(hour(95, 108, 60, 68, 6)), 92.0);
    expect("cold, damp, raining, breezy", GrossScore::score(hour(40, 30, 90, 38, 20, 100)), 58.5);
    expect("snow, cold, no rain counted", GrossScore::score(hour(30, 10, 70, 25, 10)), 52.5);
    expect("rain, wind 25, dewpoint 66", GrossScore::score(hour(70, 70, 80, 66, 25, 100)), 29.5);
    expect("dry heat index 112", GrossScore::score(hour(100, 112, 30, 60, 5)), 90.5);
    expect("wind chill -15", GrossScore::score(hour(20, -15, 60, 10, 30)), 158.0);
    // freezing rain or sleet counts 40 in place of the 15 of rain (the specification's text; its code adds both)
    expect("freezing rain", GrossScore::score(hour(30, 30, 60, 25, 5, 90, 80)), 22.5 + 40.0);
    expect("rain chance under 50", GrossScore::score(hour(70, 70, 60, 50, 5, 49)), 0.0);
    expect("rain chance 50", GrossScore::score(hour(70, 70, 60, 50, 5, 50)), 15.0);
    expect("chilly damp needs both", GrossScore::score(hour(49, 49, 86, 45, 5)), 15.0);
    expect("chilly damp at 85 is not", GrossScore::score(hour(49, 49, 85, 45, 5)), 0.0);
    expect("unknown values count nothing", GrossScore::score(hour(std::nan(""), 70, std::nan(""), 50, std::nan(""))), 0.0);
    // the tiers (0-19 Mild, 20-44 Gross, 45-69 Nasty, 70 and over Miserable)
    expect("tier 19.9", GrossScore::tier(19.9), 1);
    expect("tier 20", GrossScore::tier(20.0), 2);
    expect("tier 44.9", GrossScore::tier(44.9), 2);
    expect("tier 45", GrossScore::tier(45.0), 3);
    expect("tier 69.9", GrossScore::tier(69.9), 3);
    expect("tier 70", GrossScore::tier(70.0), 4);
    // the lines: half way to the normal high from 70 F, the hot line never above 100 F
    double cold = 0, hot = 0;
    GrossScore::lines(70.0, cold, hot);
    expect("normal 70: cold", cold, 45.0);
    expect("normal 70: hot", hot, 85.0);
    GrossScore::lines(40.0, cold, hot);
    expect("normal 40: cold", cold, 30.0);
    expect("normal 40: hot", hot, 70.0);
    GrossScore::lines(106.0, cold, hot);   // Phoenix in July
    expect("normal 106: cold", cold, 63.0);
    expect("normal 106: hot capped", hot, 100.0);
    GrossScore::lines(std::nan(""), cold, hot);
    expect("no normal: cold", cold, 45.0);
    expect("no normal: hot", hot, 85.0);
    // a cold place's ordinary day is not Gross, and the same feels like is in a warm place
    expect("40 F in a place with a 40 F normal", GrossScore::score(hour(40, 38, 50, 30, 5), 30.0, 70.0), 0.0);
    expect("40 F in a place with a 70 F normal", GrossScore::score(hour(40, 38, 50, 30, 5), 45.0, 85.0), 10.5);
    // feels like over 100 counts everywhere, and 105 is nasty in the desert too
    expect("102 in Phoenix", GrossScore::score(hour(102, 102, 10, 40, 5), 63.0, 100.0), 3.0);
    expect("106 in Phoenix", GrossScore::score(hour(106, 106, 10, 40, 5), 63.0, 100.0), 9.0 + 50.0);
    if (failures == 0) {
        std::printf("all Gross score tests passed\n");
    }
    return failures == 0 ? 0 : 1;
}
