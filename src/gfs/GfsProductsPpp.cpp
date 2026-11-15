// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

// The WPC probabilistic precipitation products (WPC-PPP).
#include "gfs/GfsChartInternal.h"

namespace GfsChart {
namespace detail {

void pppProducts(std::vector<Product>& p) {
    struct Element {
        const char * key;
        const char * name;
        const char * unit;
        std::vector<std::pair<const char *, const char *>> amounts;   // (the file's text of the amount, as it reads)
    };
    const std::vector<Element> elements{
        {"qpf", "Rain", "liquid", {{"0p01", "0.01"}, {"0p10", "0.10"}, {"0p25", "0.25"}, {"0p50", "0.50"}, {"1p00", "1"}, {"2p00", "2"}, {"3p00", "3"}, {"4p00", "4"}, {"6p00", "6"}, {"8p00", "8"}, {"12p0", "12"}}},
        {"sn", "Snow", "snow", {{"0p10", "0.1"}, {"1p00", "1"}, {"2p00", "2"}, {"4p00", "4"}, {"6p00", "6"}, {"8p00", "8"}, {"12p0", "12"}, {"18p0", "18"}}},
        {"zr", "Ice", "freezing rain", {{"0p01", "0.01"}, {"0p10", "0.10"}, {"0p25", "0.25"}, {"0p50", "0.50"}, {"0p75", "0.75"}, {"1p00", "1"}, {"1p25", "1.25"}, {"1p50", "1.5"}, {"2p00", "2"}}}};
    for (const int hours : {24, 36, 48, 72}) {
        for (const auto& e : elements) {
            const auto kinds = [&] (const std::string& kind, const std::string& id, const std::string& label, const std::string& group, const std::string& title, bool chance) {
                Product x;
                x.source = "WPC-PPP";
                x.id = id;
                x.label = label;
                x.pickerGroup = group;
                x.wants = {want("v", std::to_string(hours) + "|" + e.key + "|" + kind, "")};
                x.fill = [] (const Grids& g) { return pick(g, "v"); };
                x.ramp = chance ? probability() : std::string{e.key} == "sn" ? snowfall() : std::string{e.key} == "zr" ? mixedPrecipitation() : precipitation();
                x.quantity = chance ? Quantity::Other : std::string{e.key} == "sn" ? Quantity::Centimeters : Quantity::Millimeters;
                x.fillTitle = title;
                x.legendStep = chance ? 10.0 : 0.0;
                p.push_back(x);
            };
            const std::string h = std::to_string(hours), prefix = "ppp_" + h + "h_" + e.key + "_";
            const std::string amountGroup = std::string{e.name} + ": amount in the period (expected, percentiles)", chanceGroup = std::string{e.name} + ": chance of an amount";
            kinds("expected", prefix + "expected", h + " hr " + e.name + ", expected amount", amountGroup, "WPC " + h + " hour " + e.unit + ", expected amount", false);
            for (const char * percentile : {"05", "10", "25", "50", "75", "90", "95"}) {
                kinds(std::string{"p"} + percentile, prefix + "p" + percentile, h + " hr " + e.name + ", " + percentile + "th percentile", amountGroup,
                      "WPC " + h + " hour " + e.unit + ", " + percentile + "th percentile of the members", false);
            }
            if (std::string{e.key} == "sn") {   // the three medians on one chart, once for each period (the snow's turn: the elements are done)
                Product x;
                x.source = "WPC-PPP";
                x.id = "ppp_" + h + "h_median";
                x.label = h + " hr median: rain, snow and ice";
                x.pickerGroup = "Median of all three (rain, snow, ice)";
                x.wants = {want("r", h + "|qpf|p50", ""), want("s", h + "|sn|p50", ""), want("z", h + "|zr|p50", "")};
                // the rain's amount is the liquid of the period, snow's water included: the type that falls takes the place (ice over snow over rain), 0.1 in of snow and 0.01 in of ice being the least that counts
                x.derive = [] (Grids& g, const Context&) {
                    auto rain = g["r"], snow = g["s"], mix = g["z"];
                    for (size_t i = 0; i < rain.values.size(); i++) {
                        const bool isMix = g["z"].values[i] >= 0.25f;   // millimeters: 0.01 in
                        const bool isSnow = !isMix && g["s"].values[i] >= 0.25f;   // centimeters: 0.1 in
                        rain.values[i] = !isMix && !isSnow ? g["r"].values[i] : 0.0f;
                        snow.values[i] = isSnow ? g["s"].values[i] : 0.0f;
                        mix.values[i] = isMix ? g["z"].values[i] : 0.0f;
                    }
                    g["rain"] = std::move(rain);
                    g["snow"] = std::move(snow);
                    g["mix"] = std::move(mix);
                };
                x.fill = [] (const Grids& g) { return pick(g, "rain"); };
                x.ramp = precipitation();
                x.overlays = {{"snow", snowfall()}, {"mix", mixedPrecipitation()}};
                x.quantity = Quantity::Millimeters;
                x.fillTitle = "WPC " + h + " hour median (green rain in liquid, blue snow, pink ice)";
                x.legendStep = 0.0;
                p.push_back(x);
            }
            for (const auto& amount : e.amounts) {
                kinds(std::string{"prob_"} + amount.first, prefix + "prob_" + amount.first, h + " hr " + e.name + ", chance of " + amount.second + " in or more", chanceGroup,
                      "WPC " + h + " hour " + e.unit + ": chance of " + amount.second + " in or more (%)", true);
            }
        }
    }
}

}  // namespace detail
}  // namespace GfsChart
