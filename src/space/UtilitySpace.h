// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef UTILITYSPACE_H
#define UTILITYSPACE_H

#include <string>
#include <vector>

using std::string;
using std::vector;

// NOAA's Space Weather Prediction Center data (services.swpc.noaa.gov, public, no key): the storm scales, the planetary K index with its forecast, the
// GOES X-ray flux and the latest flare, the real-time solar wind and magnetic field, and the alert messages. Pure readers of the JSON; no network.
class UtilitySpace {
public:
    static constexpr double missing = -9999.0;
    static bool has(double v) { return v > missing + 1.0; }

    struct ScaleDay {            // one day of noaa-scales.json: "0" is now, "1" to "3" the forecast days
        string date;             // 2026-10-09
        int r{-1};               // radio blackout scale R0..R5, -1 when the day has no value
        int s{-1};               // solar radiation storm S0..S5
        int g{-1};               // geomagnetic storm G0..G5
        int rMinor{-1};          // percent chance of R1 to R2 (forecast days)
        int rMajor{-1};          // percent chance of R3 or more
        int sProb{-1};           // percent chance of S1 or more
    };
    struct Point {
        long seconds{0};         // UTC
        double value{missing};
        double second{missing};  // a second quantity: the wind density beside its speed, Bz beside Bt
        int kind{0};             // Kp: 0 observed, 1 estimated, 2 predicted
    };
    struct Flare {
        string current;          // "C1.4": the class of the flux now
        string maxClass;         // the largest of the day's last flare
        string maxTime;
        string beginTime;
        string endTime;
    };
    struct Band {                // a predicted value with its range
        long seconds{0};
        double mid{missing};
        double low{missing};
        double high{missing};
    };
    struct Ovation {             // the OVATION aurora model on a 1 degree grid: the chance (percent) of seeing the aurora
        string observation;      // when the solar wind it used was measured
        string forecast;         // the time it is a forecast for
        vector<float> grid;      // 360 columns (longitude 0 to 359 east) by 181 rows (latitude -90 to 90), row-major from -90
        bool ok{false};
        float at(double lat, double lon) const;
    };
    static vector<ScaleDay> parseScales(const string& json);                   // index 0 is now, then +1 .. +3 days (missing days are left out)
    static vector<Point> parseKp(const string& json);                          // the forecast file: observed, estimated and predicted three-hourly values
    static vector<Point> parseXray(const string& json, int stepMinutes = 1);  // the 0.1-0.8 nm flux (W/m2), thinned to one a `step`
    static vector<Point> parseWind(const string& json, int stepMinutes = 5);  // the proton speed (km/s) and density (/cm3) from the active source
    static vector<Point> parseMag(const string& json, int stepMinutes = 5);   // Bt (nT) and Bz GSM (nT)
    // the integral particle flux (pfu) of one energy channel from the GOES files: ">=10 MeV" protons, ">=2 MeV" electrons; 5 minute steps
    static vector<Point> parseFlux(const string& json, const string& energy);
    // the sunspot number by month: value the monthly number, second the smoothed one (missing when the month has none), since `fromYear`
    static vector<Point> parseCycleObserved(const string& json, int fromYear = 1990);
    static vector<Band> parseCyclePredicted(const string& json);              // the predicted smoothed sunspot number and its low / high range, by month
    static Ovation parseOvation(const string& json);
    static Flare parseFlare(const string& json);
    static vector<string> parseAlerts(const string& json, size_t count = 6);  // the first lines of the newest messages ("ALERT: Electron 2MeV Integral Flux exceeded 1,000pfu")
    static string flareClass(double flux);                                    // 2.3e-6 -> "C2.3"
    static int kpScale(double kp);                                            // 0 below Kp 5, else 1..5 for G1..G5 (Kp 5 to 9)
};

#endif  // UTILITYSPACE_H
