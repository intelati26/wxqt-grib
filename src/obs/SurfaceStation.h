// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef SURFACESTATION_H
#define SURFACESTATION_H

#include <string>

// One surface station's newest report, from either source (the airport METARs of the Aviation Weather Center or the mesonet and other
// networks of MADIS), in one set of units: Celsius, knots, statute miles, inches of mercury and millibars. A value a station does not report
// is `missing`.
struct SurfaceStation {
    static constexpr double missing = -9999.0;
    static bool has(double v) { return v > missing + 1.0; }
    std::string id;           // KOUN, or the mesonet's own id
    std::string name;
    std::string network;      // "METAR" for the airports, else the MADIS data provider (MesoWest, HADS, APRSWXNET, RAWS ...)
    std::string state;
    bool airport{false};
    double lat{0.0};
    double lon{0.0};          // east positive
    double mercator{0.0};     // for the map
    double elevation{missing};     // m
    long seconds{0};          // observation time, UTC
    double temperature{missing};   // C
    double dewPoint{missing};      // C
    double humidity{missing};      // %
    double windDirection{missing}; // degrees, the wind comes from
    double windSpeed{missing};     // kt
    double windGust{missing};      // kt
    double visibility{missing};    // mi
    double altimeter{missing};     // inHg
    double seaLevel{missing};      // mb
    std::string flight;       // VFR, MVFR, IFR, LIFR (the airports only)
    std::string weather;      // the METAR's present-weather string
    std::string sky;          // "BKN 2500 ft, OVC 4000 ft"
    std::string raw;          // the METAR text
    char quality{' '};        // MADIS' data descriptor for the temperature: V verified, S screened, C coarse pass, Q questioned, X rejected
};

#endif  // SURFACESTATION_H
