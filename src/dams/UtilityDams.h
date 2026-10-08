// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef UTILITYDAMS_H
#define UTILITYDAMS_H

#include <string>
#include <vector>

using std::string;
using std::vector;

// U.S. Army Corps of Engineers hydropower dams whose hourly data the Corps' public CWMS Data API serves (cwms-data.usace.army.mil/cwms-data): pool and tailwater
// elevation, total release, turbine flow, inflow and the energy generated. The list (resourceCreation/res/dams_usace.txt, made by createDamRegistry.py) holds,
// for each dam, its position and the names of the time series to read. Pure text work, no network.
class UtilityDams {
public:
    static constexpr double missing = -9999.0;
    struct Project {
        string office;           // SWL, SWT
        string id;               // Table_Rock_Dam
        string name;             // Table Rock Dam
        double lat{0.0};
        double lon{0.0};         // east positive
        string city;
        string state;
        string pool;             // series names, empty when the dam has none
        string tailwater;
        string outflow;          // total release
        string power;            // flow through the turbines
        string inflow;
        vector<string> generation;   // energy generated per hour; several units are added together
        double mercator{0.0};    // for the map
    };
    struct Point {
        long seconds{0};         // UTC seconds since 1970-01-01
        double value{missing};
    };
    struct Series {
        string units;
        vector<Point> points;    // oldest first; missing values left out
    };
    static vector<Project> parseRegistry(const string& text);
    // a CWMS Data API time series (JSON, version 2): "units" and "values": [[milliseconds, value, quality], ...]
    static Series parseTimeSeries(const string& json);
    // the sum of several series at the times they share
    static Series sum(const vector<Series>&);
    static const Project * nearest(const vector<Project>&, double lat, double lon, double maxKm, double * distanceKm = nullptr);
    static double kilometers(double lat1, double lon1, double lat2, double lon2);
    static bool has(double v) { return v > missing + 1.0; }
};

#endif  // UTILITYDAMS_H
