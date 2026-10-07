// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef UTILITYNHCGIS_H
#define UTILITYNHCGIS_H

#include <string>
#include <utility>
#include <vector>

using std::string;
using std::vector;

// NHC's GIS products for an active storm: the advisory cone, track and forecast points (the "5day" zip of shapefiles), the forecast and initial wind
// radii (the "fcst" zip) and the watch / warning lines (the KMZ). The layer and attribute names are those of NHC's sample files (nhc.noaa.gov/gis).
// Pure byte and text work, no network.
class UtilityNhcGis {
public:
    using Ring = vector<std::pair<double, double>>;     // (lon, lat)
    struct Point {                                       // a forecast point (pts layer)
        int tau{0};
        double lat{0.0};
        double lon{0.0};
        double maxWind{-1.0};     // kt
        double gust{-1.0};
        double pressure{-1.0};
        string label;             // "5:00 AM Mon"
        string development;       // "Major Hurricane", "Tropical Storm", "Post-Tropical" ...
        string validTime;         // "04/0600"
    };
    struct Cone {
        vector<Ring> polygons;    // the cone (pgn); usually one ring
        vector<Ring> lines;       // the forecast track (lin)
        vector<Point> points;
        string stormName;
        string advisory;          // "20"
        string advisoryDate;      // "500 AM AST Mon Sep 04 2017"
        bool ok{false};
    };
    struct WindRadius {           // one threshold at one forecast hour, a quadrant radius in nm
        int knots{0};
        int tau{0};
        double ne{0}, se{0}, sw{0}, nw{0};
    };
    struct WatchWarning {
        string kind;              // "Hurricane Warning", "Tropical Storm Watch", ...
        string code;              // HWR, HWA, TWR, TWA ... (the style id)
        vector<Ring> lines;
    };
    static Cone parseCone(const string& zip);
    static vector<WindRadius> parseRadii(const string& zip);           // forecastradii and initialradii layers
    static vector<WatchWarning> parseWatchWarnings(const string& kmz); // a KMZ (zip) or a bare KML
    static vector<WatchWarning> parseKml(const string& kml);
    static string colorFor(const string& code);                        // "#rrggbb" per watch / warning type
    static string nameFor(const string& code);
};

#endif  // UTILITYNHCGIS_H
