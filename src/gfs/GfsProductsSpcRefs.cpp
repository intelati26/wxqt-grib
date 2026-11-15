// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

// The SPC's REFS charts (SPCREFS): the products, hours and types of the viewer.
#include "gfs/GfsChartInternal.h"
#include "spcrefs/UtilitySpcRefs.h"

namespace GfsChart {
namespace detail {

void spcRefsProducts(std::vector<Product>& p) {
    // SPC's own colors for the paintball of each member, in the store's order: HRRR, RRFS control, REFS M01, M02, M03, M05, then HRRR and RRFS control 6 and 12 hours older
    static const unsigned memberColors[] = {0x4c4c4c, 0x007399, 0x009985, 0x009947, 0x009909, 0x469900, 0x7f7f7f, 0x4cd3ff, 0xb3b3b3, 0xb3ecff};
    // the lines SPC's viewer draws over a chart: the neighborhood chances of the fields (10, 30, 50, 70 and 90 per cent) and the freezing and 60 F lines of the mean temperature
    using Lines = std::vector<std::pair<std::string, std::string>>;   // the array and what its lines say
    static const std::map<std::string, Lines> chances = [] {
        std::map<std::string, Lines> out;
        const auto both = [&out] (const std::vector<std::string>& charts, const Lines& lines) {
            for (const auto& id : charts) {
                out[id] = lines;
            }
        };
        both({"cref_pb_t040"}, {{"cref_nmep_r013_t040", "40 dBZ chance"}});
        for (const std::string h : {"04h_max_overlap", "24h_max_overlap12h"}) {
            const std::string n = h == "04h_max_overlap" ? "04h" : "24h";
            both({"maxref1km_" + h + "_max", "maxref1km_mucape_" + h + "_pb_t>040"}, {{"maxref1km_mucape_" + h + "_nmep_r013_t>040", "40 dBZ chance"}});
            both({"uh25_" + h + "_max"}, {{"uh25_" + h + "_nmep_r013_t>99.85p", "UH > 99.85 percentile chance"}, {"uh25_" + h + "_nmep_r013_t>99.95p", "UH > 99.95 percentile chance"}});
            both({"uh25_" + h + "_pb_t>99.85p"}, {{"uh25_" + h + "_nmep_r013_t>99.85p", "chance"}});
            both({"uh25_" + h + "_pb_t>99.95p"}, {{"uh25_" + h + "_nmep_r013_t>99.95p", "chance"}});
            both({"wmax_" + h + "_max"}, {{"wmax_" + h + "_nmep_r013_t020", "20 m/s chance"}, {"wmax_" + h + "_nmep_r013_t030", "30 m/s chance"}});
            both({"wmax_" + h + "_pb_t020"}, {{"wmax_" + h + "_nmep_r013_t020", "chance"}});
            both({"s10m_" + h + "_max"}, {{"s10m_" + h + "_nmep_r013_t030", "30 kt chance"}, {"s10m_" + h + "_nmep_r013_t050", "50 kt chance"}});
            both({"s10m_" + h + "_pb_t030"}, {{"s10m_" + h + "_nmep_r013_t030", "chance"}});
            (void) n;
        }
        both({"uh25min_04h_min_overlap_min"}, {{"uh25min_04h_min_overlap_nmep_r013_t<-35", "UH < -35 chance"}, {"uh25min_04h_min_overlap_nmep_r013_t<-70", "UH < -70 chance"}});
        both({"uh25min_24h_min_overlap12h_min"}, {{"uh25min_24h_min_overlap12h_nmep_r013_t<-35", "UH < -35 chance"}, {"uh25min_24h_min_overlap12h_nmep_r013_t<-70", "UH < -70 chance"}});
        for (const char * id : {"qpf_03h_acc_pmmean", "qpf_03h_acc_mean", "qpf_03h_acc_max"}) {
            out[id] = {{"qpf_03h_acc_nmep_r013_t>001", "1 in chance"}, {"qpf_03h_acc_nmep_r013_t>003", "3 in chance"}};
        }
        out["qpf_06h_acc_mean"] = {{"qpf_06h_acc_nmep_r013_t>0p100", "0.1 in chance"}, {"qpf_06h_acc_nmep_r013_t>0p250", "0.25 in chance"}};
        return out;
    }();
    const auto addChances = [] (Product& x, const std::string& array, const std::string& title, const QColor& color) {
        const std::string key = "n" + std::to_string(x.wants.size());
        x.wants.push_back(GfsData::Want{key, array, "", "", "", ""});
        ContourSet c;
        c.key = key;
        c.interval = 20.0;   // (the levels are in the labelled unit: per cent)
        c.base = 10.0;
        c.minimum = 9.0;
        c.scale = 100.0;
        c.title = title + " (%)";
        c.color = color;
        c.uniform = true;
        c.width = 1.4;
        x.contours.push_back(c);
    };
    const auto addTemperatureLine = [] (Product& x, double level, const QColor& color) {
        if (std::none_of(x.wants.begin(), x.wants.end(), [] (const GfsData::Want& w) { return w.key == "t2"; })) {
            x.wants.push_back(GfsData::Want{"t2", "t2m_mean", "", "", "", ""});
        }
        ContourSet c;
        c.key = "t2";
        c.base = level;
        c.interval = 1000.0;   // one line: this level (and not the ones a thousand degrees away)
        c.minimum = level - 1.0;
        c.title = "2-m temperature (ensemble mean, F)";
        c.color = color;
        c.uniform = true;
        c.width = 2.0;
        x.contours.push_back(c);
    };
    const auto decorate = [&] (Product& x, const UtilitySpcRefs::Product& source) {
        const auto found = chances.find(source.id);
        if (found != chances.end()) {
            const QColor colors[] = {QColor{0, 0, 0}, QColor{170, 0, 0}};
            for (size_t i = 0; i < found->second.size(); i++) {
                addChances(x, found->second[i].first, found->second[i].second, colors[i % 2]);
            }
        }
        const auto starts = [&source] (const char * prefix) { return source.id.rfind(prefix, 0) == 0; };
        if (starts("fosberg_") || starts("rh2m_s10m_prob_") || starts("gust_") || starts("s10m_01h_max_") || starts("bliz_")) {
            addTemperatureLine(x, 32.0, QColor{"#0000c4"});
            if (!starts("bliz_")) {
                addTemperatureLine(x, 60.0, QColor{"#bb271a"});
            }
        }
    };
    for (const auto& source : UtilitySpcRefs::allProducts()) {   // every field of the store, as the SPC REFS screen had them
        Product x;
        x.source = "SPCREFS";
        x.id = source.id;
        x.label = source.label;
        x.pickerGroup = "SPC REFS: " + source.group;
        x.wants = {GfsData::Want{"a", source.id, "", "", "", source.members ? "m001" : ""}};
        decorate(x, source);
        if (source.kind == UtilitySpcRefs::Kind::Paintball) {   // bit i of the mask: member i is over the limit
            x.derive = [] (Grids& g, const Context&) {
                const auto mask = g.at("a");
                for (int m = 0; m < 10; m++) {
                    auto bit = mask;
                    for (auto& v : bit.values) {
                        v = std::isnan(v) ? v : ((static_cast<long long>(v) >> m) & 1LL) ? 1.0f : 0.0f;
                    }
                    g["Member " + std::to_string(m + 1)] = std::move(bit);
                }
            };
            const auto solid = [] (unsigned rgb) {
                QColor c{static_cast<QRgb>(rgb)};
                c.setAlpha(170);
                return Ramp{{{0.0, QColor{255, 255, 255, 0}}, {0.49, QColor{255, 255, 255, 0}}, {0.5, c}, {1.0, c}}};
            };
            x.fill = [] (const Grids& g) { return pick(g, "Member 1"); };
            x.ramp = solid(memberColors[0]);
            for (int m = 1; m < 10; m++) {
                x.overlays.push_back({"Member " + std::to_string(m + 1), solid(memberColors[m])});
            }
            x.fillTitle = source.label + ": each member over the limit in its own color";
            x.legendStep = 0;
            p.push_back(x);
            continue;
        }
        const double factor = source.factor, offset = source.offset;
        x.fill = [factor, offset] (const Grids& g) {
            auto out = pick(g, "a");
            for (auto& v : out.values) {
                v = std::isnan(v) ? v : static_cast<float>(v * factor + offset);
            }
            return out;
        };
        Ramp ramp;
        for (const auto& stop : source.stops) {
            ramp.stops.emplace_back(stop.value, QColor{stop.r, stop.g, stop.b});
        }
        if (ramp.stops.empty()) {   // no table of its own: SPC's rainbow over a usual range
            ramp.stops = {{0.0, QColor{"#3b4cc0"}}, {50.0, QColor{"#e6e61e"}}, {100.0, QColor{"#8c0a0a"}}};
        }
        ramp.banded = source.kind == UtilitySpcRefs::Kind::Banded;
        if (!ramp.banded && source.hideBelow > -1e29 && source.hideBelow > ramp.stops.front().first) {   // a smooth scale with nothing drawn below a value
            const auto at = QColor::fromRgba(ramp.at(source.hideBelow));
            Ramp cut;
            cut.stops.emplace_back(source.hideBelow - 1e-6, QColor{at.red(), at.green(), at.blue(), 0});
            cut.stops.emplace_back(source.hideBelow, at);
            for (const auto& stop : ramp.stops) {
                if (stop.first > source.hideBelow) {
                    cut.stops.push_back(stop);
                }
            }
            ramp = cut;
        }
        x.ramp = ramp;
        x.fillTitle = source.label + (source.units.empty() ? "" : " (" + source.units + ")");
        x.legendStep = 0;
        p.push_back(x);
    }
    {   // the 1-hour rain, snow, freezing rain and mix amounts of the ensemble mean, each in its own colors by the ensemble's dominant precipitation type (the viewer's own bands), with the freezing line
        Product x;
        x.source = "SPCREFS";
        x.id = "qpf_01h_ptype";
        x.label = "1-h QPF and Dominant Precipitation Type";
        x.pickerGroup = "SPC REFS: Precipitation";
        x.wants = {GfsData::Want{"a", "qpf_01h_acc_mean", "", "", "", ""}, GfsData::Want{"p", "ptype_consensus", "", "", "", ""}};
        addTemperatureLine(x, 32.0, QColor{"#0000c4"});
        x.derive = [] (Grids& g, const Context&) {   // the type of each point: 1 rain, 2 snow, 3 freezing rain, 4 mix (0 none)
            const auto kinds = {std::pair{1, "rain"}, std::pair{2, "snow"}, std::pair{3, "freezing"}, std::pair{4, "mix"}};
            for (const auto& [code, name] : kinds) {
                auto out = g.at("a");
                const auto& type = g.at("p");
                for (size_t i = 0; i < out.values.size(); i++) {
                    const float t = i < type.values.size() ? type.values[i] : std::nanf("");
                    if (std::isnan(t) || static_cast<int>(std::lround(t)) != code) {
                        out.values[i] = std::nanf("");
                    }
                }
                g[name] = std::move(out);
            }
        };
        const auto bands = [] (double a, double b, double c, double d, const char * c1, const char * c2, const char * c3, const char * c4) {
            Ramp r;
            r.banded = true;
            r.stops = {{a, QColor{c1}}, {b, QColor{c2}}, {c, QColor{c3}}, {d, QColor{c4}}};
            return r;
        };
        x.fill = [] (const Grids& g) { return pick(g, "rain"); };
        x.ramp = bands(0.01, 0.1, 0.25, 1.0, "#a0fc4e", "#73c139", "#478625", "#1e4b10");
        x.overlays = {{"snow", bands(0.01, 0.05, 0.1, 0.2, "#75fbfd", "#4ba7f8", "#2254f5", "#0000f5")},
                      {"freezing", bands(0.01, 0.05, 0.1, 0.2, "#ea3729", "#b92c1f", "#892214", "#581b08")},
                      {"mix", bands(0.01, 0.05, 0.1, 0.2, "#de9def", "#be6bcb", "#9e3caa", "#801787")}};
        x.fillTitle = "1-h QPF (in) by dominant type: green rain, blue snow, red freezing rain, purple mix";
        x.legendStep = 0;
        p.push_back(x);
    }
}

}  // namespace detail
}  // namespace GfsChart
