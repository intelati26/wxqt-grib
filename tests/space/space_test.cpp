// Checks the SWPC readers against excerpts of the real feeds (8 October 2026).
#include <cmath>
#include <fstream>
#include <iostream>
#include <sstream>
#include "space/UtilitySpace.h"

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
    const std::string f = argc > 1 ? argv[1] : "tests/space/fixtures";
    const auto scales = UtilitySpace::parseScales(readFile(f + "/noaa-scales.json"));
    CHECK(scales.size() == 4 && scales[0].date == "2026-10-08" && scales[0].r == 0 && scales[0].s == 0 && scales[0].g == 0);
    CHECK(scales[1].date == "2026-10-08" && scales[2].date == "2026-10-09");   // "1" is today's forecast, "2" tomorrow's
    CHECK(scales[2].g == 2 && scales[2].rMinor == 40 && scales[2].rMajor == 5 && scales[2].sProb == 10 && scales[2].r == -1);
    CHECK(UtilitySpace::parseScales("garbage").empty());

    const auto kp = UtilitySpace::parseKp(readFile(f + "/kp_forecast.json"));
    CHECK(kp.size() == 8 && kp[0].kind == 0 && near(kp[0].value, 0.33) && kp[3].kind == 1 && kp.back().kind == 2 && near(kp.back().value, 5.67));
    CHECK(kp.back().seconds - kp[0].seconds > 0 && kp[0].seconds == 1790812800L);
    CHECK(UtilitySpace::kpScale(3.67) == 0 && UtilitySpace::kpScale(4.67) == 1 && UtilitySpace::kpScale(5.67) == 2 && UtilitySpace::kpScale(7.0) == 3 && UtilitySpace::kpScale(9.0) == 5);

    const auto xray = UtilitySpace::parseXray(readFile(f + "/xrays.json"), 1);
    CHECK(xray.size() >= 15 && xray.size() <= 20 && xray.back().value > 1e-7);   // the long channel only (twenty of the forty records), the zeros dropped
    const auto thin = UtilitySpace::parseXray("[{\"time_tag\":\"2026-10-08T00:00:00Z\",\"energy\":\"0.1-0.8nm\",\"flux\":1e-6},{\"time_tag\":\"2026-10-08T00:01:00Z\",\"energy\":\"0.1-0.8nm\",\"flux\":5e-5},"
                                              "{\"time_tag\":\"2026-10-08T00:02:00Z\",\"energy\":\"0.1-0.8nm\",\"flux\":2e-6},{\"time_tag\":\"2026-10-08T00:10:00Z\",\"energy\":\"0.1-0.8nm\",\"flux\":3e-6}]", 5);
    CHECK(thin.size() == 2 && near(thin[0].value, 5e-5, 1e-12));   // a five minute step keeps the larger value, so the peak survives
    CHECK(UtilitySpace::flareClass(1.4e-6) == "C1.4" && UtilitySpace::flareClass(2.3e-5) == "M2.3" && UtilitySpace::flareClass(4.5e-8) == "A4.5" && UtilitySpace::flareClass(1.2e-4) == "X1.2" && UtilitySpace::flareClass(0.0).empty());

    const auto wind = UtilitySpace::parseWind(readFile(f + "/wind.json"), 5);
    CHECK(!wind.empty() && wind.front().seconds < wind.back().seconds && wind.back().value > 200.0 && wind.back().value < 1500.0 && UtilitySpace::has(wind.back().second));
    const auto mag = UtilitySpace::parseMag(readFile(f + "/mag.json"), 5);
    CHECK(!mag.empty() && mag.back().value > 0.0 && UtilitySpace::has(mag.back().second));
    for (size_t i = 1; i < wind.size(); i++) {
        CHECK(wind[i].seconds - wind[i - 1].seconds >= 300);
    }

    const auto flare = UtilitySpace::parseFlare(readFile(f + "/flare.json"));
    CHECK(flare.current == "C1.4" && flare.maxClass == "C1.6" && flare.maxTime == "2026-10-08T00:59:00Z");
    CHECK(UtilitySpace::parseFlare("[]").current.empty());

    const auto alerts = UtilitySpace::parseAlerts(readFile(f + "/alerts.json"), 3);
    CHECK(alerts.size() == 3 && alerts[0].find("CONTINUED ALERT: Electron 2MeV Integral Flux exceeded 1,000pfu") != std::string::npos && alerts[0].rfind("2026-10-08 05:01", 0) == 0);
    CHECK(UtilitySpace::parseAlerts("nope").empty());

    if (failures == 0) {
        std::cout << "all space weather parser tests passed\n";
    }
    return failures == 0 ? 0 : 1;
}
