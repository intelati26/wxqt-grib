// Checks the SPC tornado database reader against rows of the real file (1950-2025_actual_tornadoes.csv).
#include <cmath>
#include <fstream>
#include <iostream>
#include <sstream>
#include "tornado/UtilityTornado.h"

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
    const std::string f = argc > 1 ? argv[1] : "tests/tornado/fixtures";
    using T = UtilityTornado;
    const auto all = T::parse(readFile(f + "/tornadoes_sample.csv"));
    CHECK(all.size() == 44);
    // the first row of the file: 1 October 1950, Oklahoma, F1, 15.8 miles, 10 yards, from 36.73 N 102.52 W to 36.88 N 102.30 W
    const auto& first = all[0];
    CHECK(first.id == "192" && first.year == 1950 && first.month == 10 && first.day == 1 && first.state == "OK" && first.mag == 1 && first.timeZone == 3 && first.time == "21:00:00");
    CHECK(near(first.startLat, 36.73) && near(first.startLon, -102.52) && near(first.endLat, 36.88) && near(first.endLon, -102.3) && near(first.length, 15.8) && near(first.width, 10.0) && first.hasEnd());
    CHECK(first.dayOfYear == 274 && T::rating(first) == "F1" && first.counts());
    // the second: no end point (0, 0), three injuries
    CHECK(!all[1].hasEnd() && all[1].injuries == 3 && all[1].state == "NC" && near(all[1].length, 2.0) && near(all[1].width, 880.0));
    // a 2011 tornado is an EF rating; the unrated (-9) are "unrated"
    const T::Tornado * twoState = nullptr;
    const T::Tornado * ef5 = nullptr;
    for (const auto& t : all) {
        if (t.id == "1111161152-03") twoState = &t;
        if (t.mag == 5 && !ef5) ef5 = &t;
    }
    CHECK(twoState != nullptr && twoState->states == 2 && twoState->state == "AL" && twoState->mag == 2 && T::rating(*twoState) == "EF2" && near(twoState->length, 61.51) && twoState->counts());
    CHECK(ef5 != nullptr && ef5->fatalities >= 0 && T::ratingOf(-9, 2024) == "unrated" && T::ratingOf(3, 2006) == "F3" && T::ratingOf(3, 2007) == "EF3");
    // days of the year and distances
    CHECK(T::dayOfYear(2011, 4, 27) == 117 && T::dayOfYear(2012, 4, 27) == 118 && T::dayOfYear(2011, 12, 31) == 365 && T::dayOfYear(2011, 13, 1) == 0);
    CHECK(near(T::kilometers(36.73, -102.52, 36.88, -102.3), 25.0, 1.0));
    // the distance to a track: the start, the end, or the line between them
    T::Tornado line;
    line.startLat = 35.0; line.startLon = -97.0; line.endLat = 35.0; line.endLon = -96.0;   // 91 km due east
    CHECK(near(T::distanceToTrack(line, 35.0, -96.5), 0.0, 0.5) && near(T::distanceToTrack(line, 35.1, -96.5), 11.1, 0.4) && near(T::distanceToTrack(line, 35.0, -98.0), 91.0, 1.5));
    T::Tornado dot;
    dot.startLat = 35.0; dot.startLon = -97.0;
    CHECK(!dot.hasEnd() && near(T::distanceToTrack(dot, 35.0, -97.0), 0.0));
    // buckets
    std::vector<const T::Tornado *> list;
    for (const auto& t : all) list.push_back(&t);
    const auto years = T::buckets(list, T::Group::Year, T::Metric::Count);
    int total = 0;
    for (const auto& [year, n] : years) total += static_cast<int>(n);
    CHECK(total == 44 && years.front().first == 1950 && years.back().first == 2025);
    const auto decades = T::buckets(list, T::Group::Decade, T::Metric::Count);
    CHECK(decades.front().first == 1950 && decades.back().first == 2020);
    const auto april27 = T::buckets(list, T::Group::DayOfYear, T::Metric::Count);
    double on27 = 0.0;
    for (const auto& [day, n] : april27) if (day == 117) on27 += n;
    CHECK(on27 >= 10.0);   // the 27 April 2011 rows of the sample (a non-leap year: day 117)
    const auto months = T::buckets(list, T::Group::Month, T::Metric::Count);
    CHECK(months.size() >= 3 && T::buckets(list, T::Group::Week, T::Metric::Count).size() >= 3);
    int deaths = 0;
    for (const auto& t : all) deaths += t.fatalities;
    double sum = 0.0;
    for (const auto& [y, n] : T::buckets(list, T::Group::Year, T::Metric::Deaths)) sum += n;
    CHECK(static_cast<int>(sum) == deaths);
    // the file name on the SPC page, and a file that is not this one
    CHECK(T::newestFile("<a href=\"data/1950-2024_actual_tornadoes.csv\">x</a> <a href=\"data/1950-2025_actual_tornadoes.csv\">y</a>") == "1950-2025_actual_tornadoes.csv" && T::newestFile("nothing").empty());
    CHECK(T::parse("a,b,c\n1,2,3\n").empty());
    // the preliminary reports of one convective day (28 April 2026): 12 UTC to 12 UTC, so the times before 1200 are the next calendar day
    const auto daily = T::parseDailyReport(readFile(f + "/daily_260428.csv"), 2026, 4, 28);
    CHECK(daily.size() == 15 && daily[0].preliminary && daily[0].year == 2026 && daily[0].month == 4 && daily[0].day == 28 && daily[0].state == "TX" && near(daily[0].startLat, 33.55) && near(daily[0].startLon, -98.04));
    CHECK(daily[0].time == "19:49:00" && daily[0].timeZone == 9 && daily[0].mag == -9 && !daily[0].hasEnd() && daily[0].dayOfYear == 118);
    CHECK(daily[8].day == 29 && daily[8].time == "00:10:00" && daily[8].dayOfYear == 119 && daily[14].day == 29 && daily[0].id != daily[1].id && daily[0].counts());
    CHECK(T::parseDailyReport(readFile(f + "/daily_empty.csv"), 2026, 10, 8).empty() && T::parseDailyReport("<html>nope</html>", 2026, 4, 28).empty());
    const auto rated = T::parseDailyReport("Time,F_Scale,Location,County,State,Lat,Lon,Comments\n2100,EF2,X,Y,OK,35.1,-97.2,\"a, b\"\n2110,UNK,X,Y,OK,35.2,-97.3,c\n", 2026, 5, 3);
    CHECK(rated.size() == 2 && rated[0].mag == 2 && rated[1].mag == -9 && T::rating(rated[0]) == "EF2");
    CHECK(T::passesRating(rated[0], 2) && !T::passesRating(rated[0], 3) && T::passesRating(rated[1], 3) && !T::passesRating(rated[1], 6) && T::passesRating(rated[1], 0) && !T::passesRating(rated[1], 5));
    CHECK(T::passesRating(all[0], 1) && !T::passesRating(all[0], 2) && T::passesRating(all[0], 0) && !T::passesRating(all[0], 6) && T::passesRating(*ef5, 5));
    int y = 2026, m = 12, d = 31;
    T::nextDay(y, m, d);
    CHECK(y == 2027 && m == 1 && d == 1);
    y = 2028; m = 2; d = 28;
    T::nextDay(y, m, d);
    CHECK(m == 2 && d == 29);
    T::nextDay(y, m, d);
    CHECK(m == 3 && d == 1);
    if (failures == 0) {
        std::cout << "all tornado database tests passed\n";
    }
    return failures == 0 ? 0 : 1;
}
