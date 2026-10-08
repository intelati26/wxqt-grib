// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef UTILITYTORNADO_H
#define UTILITYTORNADO_H

#include <string>
#include <vector>

using std::string;
using std::vector;

// The SPC tornado database (www.spc.noaa.gov/wcm/data/1950-YYYY_actual_tornadoes.csv, "SPC Tornado, Hail, and Wind Database Format Specification"): one row
// per tornado, 1950 on. The fields are read by the names of the file's header and mean what the specification says: `mag` the F scale (EF from 2007; -9 unknown),
// `len` miles, `wid` yards, an end point of 0, 0 is "not known", times are CST unless `tz` is 9 (GMT) or 0 (unknown), `loss` is a damage category before 1996 and
// millions of dollars from 1996 (an entry of 0 is not $0). In this file every row is a whole tornado (sg 1); a tornado that crossed states is one row, in the state
// of touchdown. Pure text work, no network.
class UtilityTornado {
public:
    struct Tornado {
        string id;               // om: "1104271700-03" (a count of the year's tornadoes before 2007)
        int year{0};
        int month{0};
        int day{0};
        string time;             // HH:MM:SS
        int timeZone{3};         // 3 = CST, 9 = GMT, 0 = unknown
        string state;            // postal abbreviation of the state of touchdown
        int mag{-9};             // 0..5, -9 unknown
        int injuries{0};
        int fatalities{0};
        double loss{0.0};        // see above
        double startLat{0.0};
        double startLon{0.0};
        double endLat{0.0};
        double endLon{0.0};
        double length{0.0};      // miles
        double width{0.0};       // yards
        int states{1};           // ns: states affected
        int segment{1};          // sg
        int dayOfYear{0};
        bool preliminary{false}; // a point report from the daily reports of the year the database does not cover yet: no end point, a rating only when the office gave one
        bool hasEnd() const { return endLat != 0.0 || endLon != 0.0; }
        bool counts() const { return segment == 1; }      // one row per tornado (the segments of a multi-state track are not tornadoes of their own)
    };
    static vector<Tornado> parse(const string& csv);
    // The SPC's preliminary tornado reports of one convective day (www.spc.noaa.gov/climo/reports/yymmdd_rpts_torn.csv: Time,F_Scale,Location,County,State,Lat,Lon,Comments;
    // the day runs from 12 UTC to 12 UTC, so a time before 1200 belongs to the next calendar day). Each report is one point; a rating of UNK is unrated. The times are UTC.
    static vector<Tornado> parseDailyReport(const string& csv, int year, int month, int day);
    static void nextDay(int& year, int& month, int& day);
    // the rating filter of the screens: level 0 all, 1 to 4 that rating or stronger, 5 only the 5s, 6 only the unrated. A preliminary report with no rating yet
    // passes the levels 1 to 4 (it may be any), and is not an unrated tornado of the official file
    static bool passesRating(const Tornado&, int level);
    static string rating(const Tornado&);                  // "EF3" from 2007, "F3" before; "unrated" for -9
    static string ratingOf(int mag, int year);
    static int dayOfYear(int year, int month, int day);
    static double kilometers(double lat1, double lon1, double lat2, double lon2);
    // the nearest distance (km) from a point to the track: the start, the end and the straight line between them
    static double distanceToTrack(const Tornado&, double lat, double lon);
    // the data file named on the SPC page: the newest "1950-YYYY_actual_tornadoes.csv" (and its ".zip" twin "1950-YYYY_torn.csv.zip")
    static string newestFile(const string& pageHtml);

    enum class Group { DayOfYear, Week, Month, Year, Decade, Heatmap };   // Heatmap: years down, weeks across (a chart, not a bucketing: buckets() treats it as the week)
    enum class Metric { Count, Deaths, Injuries };
    // the sum of the metric in each bucket: day of the year 1..366, week 1..53 (day 1 to 7 is week 1), month 1..12, the year, or the first year of the decade;
    // returned as pairs (bucket, value) in order, only the buckets that have something
    static vector<std::pair<int, double>> buckets(const vector<const Tornado *>&, Group, Metric);
    static double value(const Tornado&, Metric);
};

#endif  // UTILITYTORNADO_H
