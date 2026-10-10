// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef UTILITYBUOYS_H
#define UTILITYBUOYS_H

#include <map>
#include <string>
#include <vector>

using std::string;
using std::vector;

// NOAA's National Data Buoy Center (ndbc.noaa.gov): the latest observation of every reporting station (data/latest_obs/latest_obs.txt), the station list with
// names and positions (activestations.xml) and one station's recent history (data/realtime2/<id>.txt, 45 days). The columns follow the headers of those
// files ("MM" is missing): wind direction (deg), wind speed and gust (m/s), significant wave height (m), dominant and average wave period (s), mean wave
// direction (deg), pressure (hPa) and its 3 h tendency, air, water and dew point temperature (C), visibility (nmi), tide (ft). Pure text work, no network.
class UtilityBuoys {
public:
    static constexpr double missing = -9999.0;
    struct Obs {
        string id;
        double lat{missing};
        double lon{missing};
        long seconds{0};            // UTC seconds since 1970-01-01
        double windDirection{missing};
        double windSpeed{missing};  // m/s
        double gust{missing};       // m/s
        double waveHeight{missing}; // m
        double dominantPeriod{missing};   // s
        double averagePeriod{missing};    // s
        double waveDirection{missing};
        double pressure{missing};   // hPa
        double tendency{missing};   // hPa in 3 hours
        double airTemperature{missing};   // C
        double waterTemperature{missing}; // C
        double dewPoint{missing};         // C
        double visibility{missing};       // nmi
        double tide{missing};             // ft
    };
    struct Station {
        string id;
        double lat{missing};
        double lon{missing};
        string name;
        string owner;
        string type;                // buoy, fixed (a C-MAN or other fixed platform), other, dart ...
        bool met{false};            // reports meteorological data
    };
    static vector<Obs> parseObservations(const string& text);       // latest_obs.txt or a station's realtime2 file (the columns are read from the header)
    static std::map<string, Station> parseStations(const string& xml);
    static bool has(double value) { return value > missing + 1.0; }
    // unit helpers for the US screens
    static double knots(double metersPerSecond) { return has(metersPerSecond) ? metersPerSecond * 1.943844 : missing; }
    static double feet(double meters) { return has(meters) ? meters * 3.280840 : missing; }
    static double fahrenheit(double celsius) { return has(celsius) ? celsius * 9.0 / 5.0 + 32.0 : missing; }
    static string timeText(long seconds);                           // "07 Oct 10:00Z"
};

#endif  // UTILITYBUOYS_H
