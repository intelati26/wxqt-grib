// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef UTILITYACISCLIMATE_H
#define UTILITYACISCLIMATE_H

#include <QColor>
#include <QDate>
#include <QString>
#include <string>
#include <vector>

// The station climate of the Regional Climate Centers' ACIS (data.rcc-acis.org, MultiStnData): for every station of a region, a measure over a period of days, taken against the 1991-2020
// normals ACIS holds for it. What can be asked is the table `products()`: a new measure is one more entry in it (the ACIS element, how the days are put together, the unit and the
// colour bands), nothing else changes. No drawing and no screen here; the grid and the map layer sit on top (climate/GridAnalysis, mapkit/ClimateGridLayer).
namespace AcisClimate {
    struct Region {
        std::string id;           // "conus", "alaska", "hawaii"
        std::string label;
        double west, south, east, north;
        double step;              // degrees of the grid made over it
        double reachKm;           // how far a station's value is carried
        double nearestKm;         // a place with no station this near is left empty
    };
    const std::vector<Region>& regions();

    struct Band {
        double from;              // the lower edge (the first band has none: -infinity)
        QColor color;
        QString label;            // at its lower edge, for the legend ("" for none)
    };
    struct Product {
        std::string id;           // "avgt-dep"
        QString label;
        QString unit;             // "F", "in", "%"
        std::string element;      // the ACIS element: avgt, maxt, mint, pcpn
        enum class Kind {
            MeanDeparture,        // the mean of the days' departures from normal (temperatures)
            SumDeparture,         // the sum of the days' departures (precipitation, in inches)
            PercentOfNormal,      // (normal + departure) / normal over the period, in percent
        } kind;
        std::vector<Band> bands;
        int decimals{1};
    };
    const std::vector<Product>& products();
    const Product * product(const std::string& id);
    int bandOf(const Product&, double value);   // the index of the band a value falls in

    struct Station {
        double lat;
        double lon;
        float value;
        int days;                 // days with a value
        std::string name;
        std::string state;
    };
    struct Result {
        std::vector<Station> stations;
        std::string error;
        QDate from;
        QDate to;
    };
    // The stations of a region for the days from..to (blocking: call off the interface thread; the region is read in tiles on several threads, and kept on disk: a period that ended a few
    // days ago for good, a recent one for three hours). Stations without a normal, or with too many days missing, are left out.
    Result read(const Product&, const Region&, const QDate& from, const QDate& to);

    std::string progressText();   // what the reading is doing, to show while it goes

    // The daily record of one place: the long-record station nearest a point (a ThreadEx "threaded" record of a city where there is one, else the longest record near), from its
    // first day to now: snowfall and precipitation (inches), the highest and lowest temperature (F); a day without a value is NaN. Blocking; kept on disk for twelve hours.
    struct Day {
        QDate date;
        float snow;
        float rain;
        float maxt;
        float mint;
    };
    struct StationRecord {
        std::string name;
        std::string sid;
        double lat{0.0};
        double lon{0.0};
        std::vector<Day> days;
        std::string error;
    };
    StationRecord readStationRecord(double lat, double lon);
}

#endif  // UTILITYACISCLIMATE_H
