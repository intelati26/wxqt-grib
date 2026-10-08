// Checks the dam registry and the CWMS time series reader against the real registry and an excerpt of a real response (7 October 2026).
#include <cmath>
#include <fstream>
#include <iostream>
#include <sstream>
#include "dams/UtilityDams.h"

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
    const std::string fixtures = argc > 1 ? argv[1] : "tests/dams/fixtures";
    const auto projects = UtilityDams::parseRegistry(readFile(fixtures + "/dams_usace.txt"));
    CHECK(projects.size() == 13);
    const UtilityDams::Project * table = nullptr;
    const UtilityDams::Project * broken = nullptr;
    for (const auto& p : projects) {
        if (p.id == "Table_Rock_Dam") table = &p;
        if (p.id == "BROK") broken = &p;
    }
    CHECK(table != nullptr && table->office == "SWL" && table->name == "Table Rock Dam" && near(table->lat, 36.59539) && near(table->lon, -93.31106) && table->city == "Hollister" && table->state == "MO");
    CHECK(table->outflow == "Table_Rock_Dam-Tailwater.Flow.Inst.1Hour.0.CCP-Comp" && table->pool == "Table_Rock_Dam-Headwater.Elev.Inst.1Hour.0.Decodes-rev");
    CHECK(table->generation.size() == 1 && table->generation[0] == "Table_Rock_Dam-House_Unit.Energy-Gen.Total.1Hour.1Hour.Decodes-rev" && !table->inflow.empty() && !table->power.empty());
    CHECK(near(table->mercator, 180.0 / 3.14159265358979 * std::log(std::tan(3.14159265358979 / 4.0 + 36.59539 * 3.14159265358979 / 360.0)), 1e-9));
    CHECK(broken != nullptr && broken->office == "SWT" && broken->generation.size() == 2 && broken->generation[1] == "BROK-Turbine2.Energy-Gen.Total.1Hour.1Hour.Rev-SCADA" && near(broken->lon, -94.68355));
    CHECK(UtilityDams::parseRegistry("too\tshort\n").empty());

    const auto s = UtilityDams::parseTimeSeries(readFile(fixtures + "/timeseries_table_rock_outflow.json"));
    CHECK(s.units == "cfs" && s.points.size() == 6 && s.points[0].seconds == 1791352800L && near(s.points[0].value, 324.00003900593794, 1e-9) && s.points[5].seconds == 1791370800L);
    CHECK(near(s.points[3].value, 312.0000375612155, 1e-9));
    const auto withNull = UtilityDams::parseTimeSeries(R"({"units":"MWh","values":[[1000000,1.5,0],[2000000,null,0],[3000000,2.5E1,3]]})");
    CHECK(withNull.units == "MWh" && withNull.points.size() == 2 && withNull.points[0].seconds == 1000 && near(withNull.points[1].value, 25.0));
    CHECK(UtilityDams::parseTimeSeries("not json").points.empty());

    UtilityDams::Series a{"MWh", {{3600, 1.0}, {7200, 2.0}}};
    UtilityDams::Series b{"MWh", {{7200, 5.0}, {10800, 7.0}}};
    const auto total = UtilityDams::sum({a, b});
    CHECK(total.points.size() == 3 && near(total.points[1].value, 7.0) && total.points[2].seconds == 10800 && total.units == "MWh");

    double distance = 0;
    CHECK(UtilityDams::nearest(projects, 36.60, -93.30, 10.0, &distance) == table && distance < 2.0);   // a gauge just below Table Rock Dam
    CHECK(UtilityDams::nearest(projects, 40.0, -100.0, 50.0) == nullptr);
    CHECK(near(UtilityDams::kilometers(0, 0, 0, 1), 111.19, 0.1));
    if (failures == 0) {
        std::cout << "all dam parser tests passed\n";
    }
    return failures == 0 ? 0 : 1;
}
