// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

// The 30 year hindcast of the CFS (CFS30).
#include "gfs/GfsChartInternal.h"

namespace GfsChart {
namespace detail {

void cfs30Products(std::vector<Product>& p) {
    const auto stretched = [] (Ramp ramp, double factor) {
        for (auto& stop : ramp.stops) {
            stop.first *= factor;
        }
        return ramp;
    };
    {
        Product x;
        x.source = "CFS30";
        x.id = "cfs30_t2m_precip";
        x.label = "Next days: temperature and precipitation against the normal";
        x.wants = {want("v", "t2m", "30 days"), want("p", "precip", "30 days")};
        x.fill = [] (const Grids& g) { return pick(g, "v"); };
        x.ramp = stretched(temperatureDeparture(), 0.5);   // (a 30 day mean departs less than a week's)
        x.quantity = Quantity::TemperatureDifference;
        x.fillTitleFor = [] (int hour) { return "CFSv2, next " + std::to_string(hour / 24) + " days vs its normal: temperature"; };
        x.legendStep = 1.0;
        ContourSet lines;
        lines.key = "p";
        lines.interval = 0.5;
        lines.title = "precipitation, mm/day (green wetter, brown drier)";
        lines.color = QColor{20, 120, 60};
        lines.colorBelow = QColor{140, 80, 20};
        lines.split = 0.0;
        lines.smoothKm = 250.0;
        lines.width = 1.2;
        lines.uniform = true;
        x.contours = {lines};
        p.push_back(x);
    }
    {
        Product x;
        x.source = "CFS30";
        x.id = "cfs30_precip";
        x.label = "Next days: precipitation against the normal";
        x.wants = {want("v", "precip", "30 days")};
        x.fill = [] (const Grids& g) { return pick(g, "v"); };
        x.ramp = stretched(precipitationDeparture(), 0.04);   // (millimeters a day: 2.4 is a week's 60 mm over 25 days)
        x.quantity = Quantity::Millimeters;
        x.fillTitleFor = [] (int hour) { return "CFSv2, next " + std::to_string(hour / 24) + " days vs its normal: precipitation a day"; };
        x.legendStep = 0.0;
        p.push_back(x);
    }
    // the pressure against its normal, and the values themselves (the mean of the stretch; the rain as the total of it, so as to sit beside the EC46's week and the SEAS5's month)
    {
        Product x;
        x.source = "CFS30";
        x.id = "cfs30_mslp_departure";
        x.label = "Next days: sea level pressure against the normal";
        x.wants = {want("v", "mslp", "30 days")};
        x.fill = [] (const Grids& g) { return pick(g, "v"); };
        x.ramp = stretched(pressureAnomaly(), 0.4);
        x.quantity = Quantity::Other;
        x.fillTitleFor = [] (int hour) { return "CFSv2, next " + std::to_string(hour / 24) + " days vs its normal: sea level pressure (mb)"; };
        x.legendStep = 2.0;
        p.push_back(x);
    }
    {
        Product x;
        x.source = "CFS30";
        x.id = "cfs30_t2m_value";
        x.label = "Next days: mean temperature";
        x.wants = {want("v", "t2m.values", "30 days")};
        x.fill = [] (const Grids& g) { return pick(g, "v"); };
        x.ramp = temperature();
        x.quantity = Quantity::Temperature;
        x.fillTitleFor = [] (int hour) { return "CFSv2, the mean of the next " + std::to_string(hour / 24) + " days: 2 m temperature"; };
        x.legendStep = 5.0;
        p.push_back(x);
    }
    {
        Product x;
        x.source = "CFS30";
        x.id = "cfs30_precip_value";
        x.label = "Next days: total precipitation";
        x.wants = {want("v", "precip.values", "30 days")};
        x.derive = [] (Grids& g, const Context& context) {   // mm a day in the grid: the total of the stretch
            for (auto& value : g.at("v").values) {
                value *= static_cast<float>(context.hour / 24.0);
            }
        };
        x.fill = [] (const Grids& g) { return pick(g, "v"); };
        x.ramp = precipitation();
        x.quantity = Quantity::Millimeters;
        x.fillTitleFor = [] (int hour) { return "CFSv2, the total of the next " + std::to_string(hour / 24) + " days: precipitation"; };
        x.legendStep = 0.0;
        p.push_back(x);
    }
    {
        Product x;
        x.source = "CFS30";
        x.id = "cfs30_mslp";
        x.label = "Next days: mean sea level pressure";
        x.wants = {want("v", "mslp.values", "30 days")};
        x.fill = [] (const Grids& g) { return pick(g, "v"); };
        x.ramp = seaLevelPressure();
        x.quantity = Quantity::Other;
        x.fillTitleFor = [] (int hour) { return "CFSv2, the mean of the next " + std::to_string(hour / 24) + " days: sea level pressure (mb)"; };
        x.legendStep = 5.0;
        p.push_back(x);
    }
    {
        Product x;
        x.source = "CFS30";
        x.id = "cfs30_t2m";
        x.label = "Next days: temperature against the normal";
        x.wants = {want("v", "t2m", "30 days")};
        x.fill = [] (const Grids& g) { return pick(g, "v"); };
        x.ramp = stretched(temperatureDeparture(), 0.5);
        x.quantity = Quantity::TemperatureDifference;
        x.fillTitleFor = [] (int hour) { return "CFSv2, next " + std::to_string(hour / 24) + " days vs its normal: 2 m temperature"; };
        x.legendStep = 1.0;
        p.push_back(x);
    }
}

}  // namespace detail
}  // namespace GfsChart
