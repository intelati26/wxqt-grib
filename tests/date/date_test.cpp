// Tests of the ISO date writer: instants, the calendar, leap years, times before 1970, and the year-less day.
#include <cstdio>
#include "util/UtilityDate.h"

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL line %d: %s\n", __LINE__, #c); failures++; } } while (0)

int main() {
    using namespace UtilityDate;
    CHECK(iso(2026, 10, 8) == "2026-10-08" && iso(1950, 1, 31) == "1950-01-31" && iso(2011, 12, 9) == "2011-12-09");
    CHECK(iso(0L) == "1970-01-01" && isoMinute(0L) == "1970-01-01 00:00Z");
    CHECK(isoMinute(1791367200L) == "2026-10-07 10:00Z");                       // from the buoy test
    CHECK(isoMinute(951782400L) == "2000-02-29 00:00Z");                        // a leap day
    CHECK(isoMinute(951868799L) == "2000-02-29 23:59Z" && isoMinute(951868800L) == "2000-03-01 00:00Z");
    CHECK(isoMinute(4102444799L) == "2099-12-31 23:59Z" && isoMinute(4102444800L) == "2100-01-01 00:00Z");
    CHECK(isoMinute(-1L) == "1969-12-31 23:59Z" && iso(-86400L) == "1969-12-31");   // before the epoch
    const auto c = civil(1791367200L);
    CHECK(c.year == 2026 && c.month == 10 && c.day == 7 && c.secondsOfDay == 36000);
    CHECK(monthDay(10, 8) == "10-08" && monthDay(1, 1) == "01-01");
    CHECK(dayOfYear(1) == "01-01" && dayOfYear(31) == "01-31" && dayOfYear(32) == "02-01" && dayOfYear(59) == "02-28" && dayOfYear(60) == "03-01");
    CHECK(dayOfYear(281) == "10-08" && dayOfYear(365) == "12-31" && dayOfYear(0) == "01-01" && dayOfYear(400) == "12-31");
    std::printf(failures ? "%d failures\n" : "all date tests passed\n", failures);
    return failures ? 1 : 0;
}
