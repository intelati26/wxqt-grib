// Checks the inland tropical watch / warning reader against an excerpt of the NWS alerts feed (8 October 2026).
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>
#include "hurricane/UtilityTropicalAlerts.h"

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::cerr << "FAILED " << __LINE__ << ": " #cond "\n"; failures++; } } while (0)

int main(int argc, char ** argv) {
    const std::string fixtures = argc > 1 ? argv[1] : "tests/hurricane/fixtures";
    std::ifstream file{fixtures + "/tropical_alerts.json", std::ios::binary};
    std::stringstream stream;
    stream << file.rdbuf();
    const auto areas = UtilityTropicalAlerts::parse(stream.str());
    CHECK(areas.size() == 12);   // twelve with a polygon; the one without a geometry and the flood warning are left out
    std::set<std::string> codes;
    for (const auto& a : areas) {
        codes.insert(a.code);
        CHECK(!a.rings.empty() && a.rings[0].size() >= 3 && a.lat > 20.0 && a.lat < 50.0 && a.lon < -70.0 && a.lon > -100.0);
        CHECK(!a.zone.empty() && !a.expires.empty());
    }
    CHECK(codes == (std::set<std::string>{"HWR", "HWA", "TWR", "TWA", "SSW", "SSA"}));
    CHECK(UtilityTropicalAlerts::codeFor("Flood Warning").empty() && UtilityTropicalAlerts::codeFor("Hurricane Warning") == "HWR");
    CHECK(UtilityTropicalAlerts::rank("SSA") < UtilityTropicalAlerts::rank("TWR") && UtilityTropicalAlerts::rank("TWR") < UtilityTropicalAlerts::rank("HWR") && UtilityTropicalAlerts::rank("XXX") < 0);
    CHECK(UtilityTropicalAlerts::parse("not json").empty());
    if (failures == 0) {
        std::cout << "all tropical alert tests passed\n";
    }
    return failures == 0 ? 0 : 1;
}
