// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef UTILITYFORECASTPOINT_H
#define UTILITYFORECASTPOINT_H

#include <map>
#include <string>
#include <utility>
#include <vector>
#include <QDate>
#include <QDateTime>
#include <QString>
#include <QTimeZone>

// The forecast of one point as the NWS keeps it (the gridded data behind api.weather.gov, the same the NWS "IDSS Forecast Points" page draws): the weekly summary of
// each day (highest and lowest temperature, wind, chance of rain and thunder, dew point, humidity, cloud), the hourly values behind it, and what the convective and
// excessive rainfall outlooks say of the point for the next three days. Everything in the units of the page (degrees F, mph, inches, feet).
namespace UtilityForecastPoint {
    constexpr double missing = -1.0e30;
    inline bool has(double v) { return v > -1.0e29; }

    struct Day {
        QDate date;
        double maxTemp{missing}, minTemp{missing}, minChill{missing}, maxHeat{missing};
        double maxWind{missing}, minWind{missing}, maxGust{missing};
        double maxPop{missing}, maxThunder{missing};
        double maxDew{missing}, minDew{missing}, maxRh{missing}, minRh{missing};
        double maxCloud{missing}, minCloud{missing}, maxWave{missing};
    };

    // an hourly series: the hour (UTC seconds) and its value
    using Series = std::vector<std::pair<qint64, double>>;

    struct Data {
        bool ok{false};
        QString error;
        double lat{0.0}, lon{0.0};
        QString office;     // "OUN"
        QString place;      // "Norman, OK"
        QTimeZone zone;
        QDateTime updated;  // when the NWS made the forecast
        std::vector<Day> days;
        std::map<std::string, Series> hourly;   // "temperature", "windSpeed", "windGust", "probabilityOfPrecipitation", ... (see parameters())
        // the categories of the point in the next three days; empty when none ("not expected")
        QString severe[3];   // "Slight Risk", "General Thunderstorms Risk"
        QString rain[3];     // "Marginal (At Least 5%)"
    };

    struct Parameter {
        const char * key;
        const char * label;
        const char * unit;
        bool bars;   // amounts and chances read as bars, the rest as lines
    };
    // the series the hourly graphs offer, in the order of the page
    const std::vector<Parameter>& parameters();

    // the whole of it for a point (blocking: from a worker thread)
    Data fetch(double lat, double lon);

    // the pure parts, for the tests
    std::vector<std::pair<qint64, double>> expand(const std::string& validTime, double value, qint64 horizonEnd);   // "2026-10-09T12:00:00+00:00/PT2H" -> the hours
    long durationHours(const std::string& iso);   // "PT2H" 2, "P4D" 96, "P4DT12H" 108, "PT30M" 1
    std::vector<Day> summarize(const std::map<std::string, Series>& hourly, const QTimeZone& zone, const QDate& first, int count);
}

#endif  // UTILITYFORECASTPOINT_H
