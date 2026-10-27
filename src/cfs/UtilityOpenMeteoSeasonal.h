// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef UTILITYOPENMETEOSEASONAL_H
#define UTILITYOPENMETEOSEASONAL_H

#include <string>
#include <vector>
#include <QDate>

// The ECMWF's extended range (EC46, 46 days) and seasonal (SEAS5, 7 months) ensembles at a place, from Open-Meteo's seasonal forecast API (no key; "Weather data by Open-Meteo.com", based on data
// and products of the ECMWF, CC BY 4.0). One request gives every member's daily values (the control and 50 perturbed members) and, for a few fields, the ensemble mean of each week (EC46) or
// month (SEAS5) and its departure from the ECMWF's own climate. The ECMWF's data is on a 36 km grid (O320): the place is the nearest cell.
namespace UtilityOpenMeteoSeasonal {
    struct Variable {
        std::string key;          // the API's daily variable: "temperature_2m_mean"
        std::string label;        // "2 m temperature (daily mean)"
        std::string units;        // the API's own unit, as DisplayUnits names it: "deg C", "mm", "hPa", "%", "cm"
        std::string periodKey;    // the weekly / monthly variable of the mean ("temperature_2m_mean"); empty: none
        std::string anomalyKey;   // and of the departure ("temperature_2m_anomaly"); empty: none
        bool delta{false};        // the departure is a difference (a temperature difference is not converted like a temperature)
    };
    const std::vector<Variable>& variables();
    const Variable * find(const std::string& key);

    struct Daily {
        std::vector<QDate> days;
        std::vector<std::vector<float>> members;   // the control first, then members 1 to 50: a value per day (NaN where the model does not reach)
        std::vector<float> mean;                   // the mean of the members
        std::string attribution;
    };
    // model: "ecmwf_ec46" or "ecmwf_seas5". False with the reason when the request failed.
    bool daily(const std::string& model, const std::string& variable, double lat, double lon, Daily& out, std::string& error);

    struct Periods {
        std::vector<QDate> starts;   // the first day of each week (Monday) or month
        std::vector<float> values;   // the ensemble mean
    };
    // weekly (EC46) or monthly (SEAS5) values of the ensemble mean; the periods the model reaches only
    bool periods(bool weekly, const std::string& variable, double lat, double lon, Periods& out, std::string& error);

    // The weekly (EC46) or monthly (SEAS5) mean of the ensemble at the points of a regular grid (one request for up to 250 of them), for a map: row 0 is the north row, columns from the west, the cell centres
    // on whole multiples of `step` from the box's west and north edge. The periods are those the model reaches (about 6 weeks, or 6 months). The grid is at most 1200 points.
    struct PeriodGrid {
        double west{0.0}, north{0.0}, step{1.0};
        int columns{0}, rows{0};
        std::vector<QDate> starts;                // the first day of each week (a Monday) or month
        std::vector<std::vector<float>> values;   // per period: rows x columns, NaN where not given
    };
    bool periodGrid(bool weekly, const std::string& variable, double west, double south, double east, double north, double step, PeriodGrid& out, std::string& error);
}

#endif  // UTILITYOPENMETEOSEASONAL_H
