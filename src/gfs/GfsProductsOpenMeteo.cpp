// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

// The long range models read through Open-Meteo (EC46, SEAS5).
#include "gfs/GfsChartInternal.h"
#include <regex>

namespace GfsChart {
namespace detail {

void openMeteoProducts(std::vector<Product>& p, const std::string& source) {
    const bool months = source == "SEAS5";
    const std::string prefix = months ? "seas5_" : "ec46_", period = months ? "month" : "week";
    const int hoursEach = months ? 720 : 168;
    struct Spec {
        const char * id;
        const char * label;
        const char * variable;
        Ramp ramp;
        Quantity quantity;
        const char * title;
        double legendStep;
    };
    const std::vector<Spec> specs{
        {"t2m_departure", "2 m Temperature: departure of the week", "temperature_2m_anomaly", temperatureDeparture(), Quantity::TemperatureDifference, "2 m temperature, the week's mean against the ECMWF climate", 2.0},
        {"precip_departure", "Precipitation: departure of the week", "precipitation_anomaly", precipitationDeparture(), Quantity::Millimeters, "Precipitation of the week against the ECMWF climate", 0.0},
        {"mslp_departure", "Sea Level Pressure: departure of the week", "pressure_msl_anomaly", pressureAnomaly(), Quantity::Other, "Sea level pressure, the week's mean against the ECMWF climate (mb)", 5.0},
        {"t2m", "2 m Temperature: mean of the week", "temperature_2m_mean", temperature(), Quantity::Temperature, "2 m temperature, the week's mean", 5.0},
        {"precip", "Precipitation: total of the week", "precipitation_mean", precipitation(), Quantity::Millimeters, "Precipitation of the week", 0.0},
        {"mslp", "Sea Level Pressure: mean of the week", "pressure_msl_mean", seaLevelPressure(), Quantity::Other, "Sea level pressure, the week's mean (mb)", 5.0},
    };
    // a month's departures are smaller than a week's in temperature and pressure and larger in the rain: the scales stretch to match
    const auto stretched = [] (Ramp ramp, double factor) {
        for (auto& stop : ramp.stops) {
            stop.first *= factor;
        }
        return ramp;
    };
    for (const auto& spec : specs) {
        Product x;
        x.source = source;
        x.id = prefix + spec.id;
        x.label = months ? std::regex_replace(std::string{spec.label}, std::regex{"week"}, "month") : std::string{spec.label};
        x.wants = {want("v", spec.variable, "weekly")};
        x.fill = [] (const Grids& g) { return pick(g, "v"); };
        const std::string id = spec.id;
        x.ramp = months ? stretched(spec.ramp, id == "t2m_departure" ? 0.4 : id == "mslp_departure" ? 0.5 : id == "precip_departure" ? 2.0 : id == "precip" ? 2.5 : 1.0) : spec.ramp;
        x.quantity = spec.quantity;
        const std::string title = spec.title;
        x.fillTitleFor = [title, period, hoursEach, months] (int hour) {
            std::string text = title;
            if (months) {
                for (const char * word : {"the week's", "of the week", "the week"}) {   // (the titles speak of a week)
                    for (auto at = text.find(word); at != std::string::npos; at = text.find(word, at + 1)) {
                        const std::string w{word};
                        text.replace(at, w.size(), w == "the week's" ? "the month's" : w == "of the week" ? "of the month" : "the month");
                    }
                }
            }
            return text + ": " + period + " " + std::to_string(hour / hoursEach + 1) + " of the forecast (" + (months ? "calendar months, the first is the month of the run)" : "Monday to Sunday, the first is the week of the run)");
        };
        x.legendStep = spec.legendStep;
        p.push_back(x);
    }
}

}  // namespace detail
}  // namespace GfsChart
