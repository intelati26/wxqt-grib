// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

// The GEFS: the mean and the spread of its members, the chances and the members' own charts.
#include "gfs/GfsChartInternal.h"
#include <QTimeZone>

namespace GfsChart {
namespace detail {

void gefsProducts(std::vector<Product>& p) {
    const auto spr = [] (const char * key, const char * variable, const char * level) { return GfsData::Want{key, variable, level, "", "", "spr"}; };
    const auto meanHeights = [] (double interval) { auto c = heights(interval); c.title = "Mean height (dam)"; return c; };
    const auto spread = [&] (const char * id, const char * label, std::vector<GfsData::Want> wants, std::function<GfsGrid::Grid(const Grids&)> fill, const char * title, double top, double step) {
        Product x;
        x.source = "GEFS";
        x.id = id;
        x.label = label;
        x.wants = std::move(wants);
        x.fill = std::move(fill);
        x.ramp = spreadRamp(top);
        x.fillTitle = title;
        x.legendStep = step;
        return x;
    };
    for (const auto& [level, top, interval] : {std::tuple{"250 mb", 120.0, 12.0}, {"500 mb", 60.0, 6.0}, {"700 mb", 40.0, 3.0}, {"850 mb", 30.0, 3.0}}) {
        const std::string n = std::string{level}.substr(0, std::string{level}.size() - 3);
        auto z = spread(("spread_" + n + "_ht").c_str(), (n + "mb Height Spread and Mean Height").c_str(), {spr("s", "HGT", level), want("z", "HGT", level)},
                        [] (const Grids& g) { return pick(g, "s"); }, ("Spread of the " + n + " mb height (m)").c_str(), top, top / 6);
        z.contours = {meanHeights(interval)};
        p.push_back(z);
        auto w = spread(("spread_" + n + "_wnd").c_str(), (n + "mb Wind Spread, Mean Wind and Height").c_str(),
                        {spr("su", "UGRD", level), spr("sv", "VGRD", level), want("u", "UGRD", level), want("v", "VGRD", level), want("z", "HGT", level)},
                        speedOf("su", "sv"), ("Spread of the " + n + " mb wind (kt)").c_str(), n == "250" ? 30.0 : 20.0, 5);
        w.contours = {meanHeights(interval)};
        w.barbU = "u";
        w.barbV = "v";
        p.push_back(w);
        if (n != "250") {
            auto t = spread(("spread_" + n + "_temp").c_str(), (n + "mb Temperature Spread and Mean Height").c_str(), {spr("s", "TMP", level), want("z", "HGT", level)},
                            [] (const Grids& g) { return pick(g, "s"); }, ("Spread of the " + n + " mb temperature (C)").c_str(), 5.0, 1);
            t.contours = {meanHeights(interval)};
            p.push_back(t);
        }
    }
    {
        auto x = spread("spread_mslp", "Sea Level Pressure Spread and Mean", {spr("s", "PRMSL", "mean sea level"), want("p", "PRMSL", "mean sea level")},
                        [] (const Grids& g) { return GfsGrid::scaled(pick(g, "s"), 0.01); }, "Spread of the sea level pressure (mb)", 6.0, 1);
        x.contours = {pressure()};
        p.push_back(x);
    }
    {
        auto x = spread("spread_2m_temp", "2m Temperature Spread, Mean Pressure and Wind", {spr("s", "TMP", "2 m above ground"), want("p", "PRMSL", "mean sea level"), want("u", "UGRD", "10 m above ground"),
                        want("v", "VGRD", "10 m above ground")}, [] (const Grids& g) { return pick(g, "s"); }, "Spread of the 2 m temperature (C)", 6.0, 1);
        x.contours = {pressure()};
        x.barbU = "u";
        x.barbV = "v";
        p.push_back(x);
    }
    {
        auto x = spread("spread_10m_wnd", "10m Wind Spread and Mean Wind", {spr("su", "UGRD", "10 m above ground"), spr("sv", "VGRD", "10 m above ground"), want("u", "UGRD", "10 m above ground"),
                        want("v", "VGRD", "10 m above ground"), want("p", "PRMSL", "mean sea level")}, speedOf("su", "sv"), "Spread of the 10 m wind (kt)", 10.0, 2);
        x.contours = {pressure()};
        x.barbU = "u";
        x.barbV = "v";
        p.push_back(x);
    }
    {
        auto x = spread("spread_pwat", "Precipitable Water Spread", {spr("s", "PWAT", "entire atmosphere (considered as a single layer)"), want("p", "PRMSL", "mean sea level")},
                        [] (const Grids& g) { return pick(g, "s"); }, "Spread of the precipitable water (mm)", 12.0, 2);
        x.contours = {pressure()};
        p.push_back(x);
    }
    {
        auto x = spread("spread_cape", "Surface-Based CAPE Spread", {spr("s", "CAPE", "surface"), want("c", "CAPE", "surface")}, [] (const Grids& g) { return pick(g, "s"); },
                        "Spread of the CAPE (J/kg)", 1500.0, 250);
        p.push_back(x);
    }

    // ---- the maps that count the members: the share of the 31 (the control and the 30) that pass a limit, and the spread of a change. Each is 31 fetches of one field (the cache keeps them).
    static const std::vector<std::string> memberNames = [] {
        std::vector<std::string> names{"c00"};
        for (int i = 1; i <= 30; i++) {
            names.push_back(std::string{"p"} + (i < 10 ? "0" : "") + std::to_string(i));
        }
        return names;
    }();
    // the same record from every member: the grids are named "<prefix><member>"
    const auto memberNeeds = [] (const char * prefix, const char * variable, const char * level, int hour, const std::string& forecast) {
        std::vector<GfsData::Need> out;
        for (const auto& name : memberNames) {
            out.push_back({hour, GfsData::Want{std::string{prefix} + name, variable, level, forecast, "", name}});
        }
        return out;
    };
    // The percentage of the members that pass a test at each point (1 passes, 0 does not, -1 no data), into `out`; the members' grids are dropped then, 31 of them are a lot to carry.
    const auto share = [] (Grids& g, const std::function<int(const Grids&, const std::string&, size_t)>& test, const char * out, std::initializer_list<const char *> prefixes) {
        auto result = g.at(std::string{*prefixes.begin()} + memberNames.front());
        for (size_t i = 0; i < result.values.size(); i++) {
            int n = 0, passed = 0;
            for (const auto& name : memberNames) {
                const int r = test(g, name, i);
                if (r >= 0) {
                    n++;
                    passed += r;
                }
            }
            result.values[i] = n > 0 ? 100.0f * static_cast<float>(passed) / static_cast<float>(n) : std::nanf("");
        }
        g[out] = std::move(result);
        for (const auto * prefix : prefixes) {
            for (const auto& name : memberNames) {
                g.erase(std::string{prefix} + name);
            }
        }
    };
    const auto shareChart = [&] (const char * id, const std::string& label, const std::string& title, std::function<std::vector<GfsData::Need>(int)> needs, std::function<void(Grids&)> derive) {
        Product x;
        x.source = "GEFS";
        x.id = id;
        x.label = label;
        x.needs = std::move(needs);
        x.derive = [derive] (Grids& g, const Context&) { derive(g); };
        x.fill = [] (const Grids& g) { return pick(g, "share"); };
        x.ramp = probability();
        x.palette = "prob";
        x.fillTitle = title;
        x.legendStep = 10;
        return x;
    };
    const auto sixHourAmount = [] (int hour) { return std::to_string(hour - 6) + "-" + std::to_string(hour) + " hour acc fcst"; };
    const auto sixHourAverage = [] (int hour) { return std::to_string(hour - 6) + "-" + std::to_string(hour) + " hour ave fcst"; };
    // ---- made on request: the same maps with a limit and a period of the user's choosing ("rain over 2 in in 36 hours"). The model screen's "Build a chart" lists the templates; the chart's
    // id carries the choice ("gen~rain_over~2~36"), so a view that is saved or restored draws it again, and product() makes it when it is asked for.
    {
        auto& made = generators();
        const auto makeRain = [memberNeeds, share, sixHourAmount] (bool over) { return [memberNeeds, share, sixHourAmount, over] (const std::vector<double>& v) {   // inches, hours
            const double inches = v.at(0);
            const int window = std::max(6, static_cast<int>(std::lround(v.at(1) / 6.0)) * 6);
            Product x;
            x.source = "GEFS";
            x.id = generatedId(over ? "rain_over" : "rain_under", {inches, static_cast<double>(window)});
            x.label = std::string{"Chance of "} + (over ? "more" : "less") + " than " + trimmed(inches) + " in of rain in " + std::to_string(window) + " hours";
            x.needs = [memberNeeds, sixHourAmount, window] (int hour) {
                std::vector<GfsData::Need> out;
                if (hour < 6 || hour % 6 != 0) {   // the members' precipitation is in pieces of 6 hours
                    return out;
                }
                for (int end = hour, k = 0; end >= 6 && end > hour - window; end -= 6, k++) {
                    for (auto& need : memberNeeds(("a" + std::to_string(k) + "_").c_str(), "APCP", "surface", end, sixHourAmount(end))) {
                        out.push_back(std::move(need));
                    }
                }
                return out;
            };
            x.derive = [share, inches, window, over] (Grids& g, const Context& context) {
                const int pieces = std::min(window, context.hour) / 6;
                for (const auto& name : memberNames) {   // each member's total over the period
                    auto total = g.at("a0_" + name);
                    for (int k = 1; k < pieces; k++) {
                        const auto& more = g.at("a" + std::to_string(k) + "_" + name);
                        for (size_t i = 0; i < total.values.size(); i++) {
                            total.values[i] += more.values[i];
                        }
                    }
                    g["t" + name] = std::move(total);
                }
                for (auto it = g.begin(); it != g.end();) {   // the pieces are not needed again: 31 members of 10 of them are a lot to carry
                    it = it->first.size() > 1 && it->first[0] == 'a' && std::isdigit(static_cast<unsigned char>(it->first[1])) ? g.erase(it) : std::next(it);
                }
                const double mm = inches * 25.4;
                share(g, [mm, over] (const Grids& grids, const std::string& name, size_t i) {
                    const float a = grids.at("t" + name).values[i];
                    return std::isnan(a) ? -1 : (over ? a > mm : a < mm) ? 1 : 0;
                }, "share", {"t"});
            };
            x.fill = [] (const Grids& g) { return pick(g, "share"); };
            x.ramp = probability();
            x.palette = "prob";
            x.fillTitleFor = [inches, window, over] (int hour) {
                const int used = std::min(window, hour);
                return std::string{"Chance of "} + (over ? "more" : "less") + " than " + trimmed(inches) + " in of precipitation in the " + std::to_string(used) + " hours ending at hour " + std::to_string(hour) + " (% of the 31 members)";
            };
            x.legendStep = 10;
            return x;
        }; };
        made["rain_over"] = makeRain(true);
        made["rain_under"] = makeRain(false);
        // The precipitation of a type: the share of the members with a 6 hour amount over a limit that fell mostly as rain, snow, ice pellets or freezing rain
        made["ptype"] = [memberNeeds, share, sixHourAmount] (const std::vector<double>& v) {   // type (1 rain, 2 snow, 3 ice pellets, 4 freezing rain), inches
            const int type = std::clamp(static_cast<int>(std::lround(v.at(0))), 1, 4);
            const double inches = v.at(1);
            static const char * names[] = {"", "rain", "snow", "ice pellets", "freezing rain"};
            static const char * variables[] = {"", "CRAIN", "CSNOW", "CICEP", "CFRZR"};
            Product x;
            x.source = "GEFS";
            x.id = generatedId("ptype", {static_cast<double>(type), inches});
            x.label = std::string{"Chance of "} + names[type] + " (6 hours, over " + trimmed(inches) + " in)";
            x.needs = [memberNeeds, sixHourAmount, type] (int hour) {
                std::vector<GfsData::Need> out;
                if (hour < 6 || hour % 6 != 0) {
                    return out;
                }
                const auto average = std::to_string(hour - 6) + "-" + std::to_string(hour) + " hour ave fcst";
                for (auto& need : memberNeeds("a", "APCP", "surface", hour, sixHourAmount(hour))) {
                    out.push_back(std::move(need));
                }
                for (auto& need : memberNeeds("k", variables[type], "surface", hour, average)) {
                    out.push_back(std::move(need));
                }
                return out;
            };
            x.derive = [share, inches] (Grids& g, const Context&) {
                const float mm = static_cast<float>(inches * 25.4);
                share(g, [mm] (const Grids& grids, const std::string& name, size_t i) {
                    const float a = grids.at("a" + name).values[i], k = grids.at("k" + name).values[i];
                    return std::isnan(a) || std::isnan(k) ? -1 : (a >= mm && k >= 0.5f) ? 1 : 0;   // the type held most of the 6 hours
                }, "share", {"a", "k"});
            };
            x.fill = [] (const Grids& g) { return pick(g, "share"); };
            x.ramp = probability();
            x.palette = "prob";
            x.fillTitle = std::string{"Chance of "} + names[type] + " in the 6 hours, with at least " + trimmed(inches) + " in of precipitation (% of the 31 members)";
            x.legendStep = 10;
            return x;
        };
        // The chance of an anomaly of so many standard deviations: each member's value less the day's mean, over the day's standard deviation (both from the 1991-2020 reanalysis; the
        // standard deviation is worked out on first use, in the background)
        made["stdanom"] = [memberNeeds, share] (const std::vector<double>& v) {   // what (1-5), direction (1 under, 2 over), deviations
            const int what = std::clamp(static_cast<int>(std::lround(v.at(0))), 1, 5);
            const bool over = std::lround(v.at(1)) == 2;
            const double sigmas = v.at(2);
            static const char * names[] = {"", "a 500 mb height", "a 700 mb height", "a 700 mb temperature", "an 850 mb temperature", "precipitable water"};
            static const char * variables[] = {"", "HGT", "HGT", "TMP", "TMP", "PWAT"};
            static const char * levels[] = {"", "500 mb", "700 mb", "700 mb", "850 mb", "entire atmosphere (considered as a single layer)"};
            Product x;
            x.source = "GEFS";
            x.id = generatedId("stdanom", {static_cast<double>(what), over ? 2.0 : 1.0, sigmas});
            const std::string words = std::string{"Chance of "} + names[what] + " more than " + trimmed(sigmas) + " standard deviation" + (sigmas == 1.0 ? "" : "s") + " " + (over ? "above" : "below") + " normal";
            x.label = words;
            x.needs = [memberNeeds, what] (int hour) {
                if (hour < 3) {
                    return std::vector<GfsData::Need>{};
                }
                auto out = memberNeeds("v", variables[what], levels[what], hour, std::to_string(hour) + " hour fcst");
                if (what == 2 || what == 3 || what == 4) {   // where the surface is above the level, the level is underground: the control run's surface pressure marks it
                    out.push_back({hour, GfsData::Want{"ps", "PRES", "surface", std::to_string(hour) + " hour fcst", "", "c00"}});
                }
                return out;
            };
            x.derive = [share, what, over, sigmas] (Grids& g, const Context& context) {
                if (!context.climate) {
                    throw std::runtime_error{"no climatology"};
                }
                auto t = QDateTime::fromString(QString::fromStdString(context.run.id()), "yyyyMMddHH");
                t.setTimeZone(QTimeZone::utc());
                t = t.addSecs(context.hour * 3600LL);
                const int day = GfsClimate::dayIndex(t.date().year(), t.date().month(), t.date().day());
                const auto field = what == 1 ? GfsClimate::height(500) : what == 2 ? GfsClimate::height(700) : what == 3 ? GfsClimate::temperature(700) : what == 4 ? GfsClimate::temperature(850) : GfsClimate::precipitableWater();
                GfsGrid::Grid normal, deviation;
                std::string error;
                if (!context.climate->at(field, day, normal, error) || !context.climate->deviation(field, day, deviation, error)) {
                    throw std::runtime_error{error};
                }
                if (what == 3 || what == 4) {
                    normal = GfsGrid::scaled(normal, 1.0, -273.15);   // the reanalysis' kelvins to the degrees C of the forecast fields
                }
                const auto& first = g.at("v" + memberNames.front());
                auto zero = first;
                std::fill(zero.values.begin(), zero.values.end(), 0.0f);
                const auto spreadHere = GfsGrid::scaled(GfsGrid::anomaly(zero, deviation), -1.0);   // the standard deviation on the members' grid
                const float level = what == 2 ? 70000.0f : what == 3 ? 70000.0f : what == 4 ? 85000.0f : 0.0f;
                for (const auto& name : memberNames) {
                    auto z = GfsGrid::anomaly(g.at("v" + name), normal);
                    for (size_t i = 0; i < z.values.size(); i++) {
                        const float sd = spreadHere.values[i];
                        const bool under = level > 0.0f && g.at("ps").values[i] < level;
                        z.values[i] = (sd > 1e-6f && !under) ? z.values[i] / sd : std::nanf("");
                    }
                    g["v" + name] = std::move(z);
                }
                g.erase("ps");
                share(g, [over, sigmas] (const Grids& grids, const std::string& name, size_t i) {
                    const float a = grids.at("v" + name).values[i];
                    return std::isnan(a) ? -1 : (over ? a > sigmas : a < -sigmas) ? 1 : 0;
                }, "share", {"v"});
            };
            x.fill = [] (const Grids& g) { return pick(g, "share"); };
            x.ramp = probability();
            x.palette = "prob";
            x.fillTitle = words + " (% of the 31 members)";
            x.legendStep = 10;
            return x;
        };
        // WPC's "heavy rain potential": the share of the members that have, together at a point, precipitable water over so many standard deviations above normal, CAPE over a limit and a
        // 6 hour amount over a limit ("PROB PW > 2 SIGMA + MUCAPE > 500 + QPF > 0.10 IN"). CAPE is that of the most unstable parcel of the lowest 255 mb (the second 0.5 degree file).
        made["heavy_rain"] = [memberNeeds, share, sixHourAmount] (const std::vector<double>& v) {   // deviations, CAPE, inches
            const double sigmas = v.at(0), cape = v.at(1), inches = v.at(2);
            Product x;
            x.source = "GEFS";
            x.id = generatedId("heavy_rain", {sigmas, cape, inches});
            const std::string words = "Chance of precipitable water over " + trimmed(sigmas) + " standard deviation" + (sigmas == 1.0 ? "" : "s") + " above normal, MUCAPE over " + trimmed(cape) +
                " and over " + trimmed(inches) + " in of rain in 6 hours";
            x.label = "Heavy rain potential: PW > " + trimmed(sigmas) + " sigma, MUCAPE > " + trimmed(cape) + ", rain > " + trimmed(inches) + " in";
            x.needs = [memberNeeds, sixHourAmount] (int hour) {
                std::vector<GfsData::Need> out;
                if (hour < 6 || hour % 6 != 0) {
                    return out;
                }
                for (auto& need : memberNeeds("w", "PWAT", "entire atmosphere (considered as a single layer)", hour, std::to_string(hour) + " hour fcst")) {
                    out.push_back(std::move(need));
                }
                for (auto& need : memberNeeds("e", "CAPE", "255-0 mb above ground", hour, std::to_string(hour) + " hour fcst")) {
                    out.push_back(std::move(need));
                }
                for (auto& need : memberNeeds("a", "APCP", "surface", hour, sixHourAmount(hour))) {
                    out.push_back(std::move(need));
                }
                return out;
            };
            x.derive = [share, sigmas, cape, inches] (Grids& g, const Context& context) {
                if (!context.climate) {
                    throw std::runtime_error{"no climatology"};
                }
                auto t = QDateTime::fromString(QString::fromStdString(context.run.id()), "yyyyMMddHH");
                t.setTimeZone(QTimeZone::utc());
                t = t.addSecs(context.hour * 3600LL);
                const int day = GfsClimate::dayIndex(t.date().year(), t.date().month(), t.date().day());
                GfsGrid::Grid normal, deviation;
                std::string error;
                if (!context.climate->at(GfsClimate::precipitableWater(), day, normal, error) || !context.climate->deviation(GfsClimate::precipitableWater(), day, deviation, error)) {
                    throw std::runtime_error{error};
                }
                auto zero = g.at("w" + memberNames.front());
                std::fill(zero.values.begin(), zero.values.end(), 0.0f);
                const auto spreadHere = GfsGrid::scaled(GfsGrid::anomaly(zero, deviation), -1.0);
                for (const auto& name : memberNames) {   // the member's precipitable water in standard deviations
                    auto z = GfsGrid::anomaly(g.at("w" + name), normal);
                    for (size_t i = 0; i < z.values.size(); i++) {
                        z.values[i] = spreadHere.values[i] > 1e-6f ? z.values[i] / spreadHere.values[i] : std::nanf("");
                    }
                    g["w" + name] = std::move(z);
                }
                const float mm = static_cast<float>(inches * 25.4);
                share(g, [sigmas, cape, mm] (const Grids& grids, const std::string& name, size_t i) {
                    const float w = grids.at("w" + name).values[i], e = grids.at("e" + name).values[i], a = grids.at("a" + name).values[i];
                    return std::isnan(w) || std::isnan(e) || std::isnan(a) ? -1 : (w > sigmas && e > cape && a > mm) ? 1 : 0;
                }, "share", {"w", "e", "a"});
            };
            x.fill = [] (const Grids& g) { return pick(g, "share"); };
            x.ramp = probability();
            x.palette = "prob";
            x.fillTitle = words + " (% of the 31 members)";
            x.legendStep = 10;
            return x;
        };
        // ---- ingredients together: the share of the 31 members that meet every condition chosen at a point (up to three: CAPE over a limit, shear over a limit, rain over a limit ...)
        made["ingredients"] = [memberNeeds, sixHourAmount] (const std::vector<double>& v) {
            const auto conditions = ingredientConditions(true, "GEFS");
            std::vector<IngredientCondition> chosen;
            for (const double index : v) {
                if (index >= 0 && static_cast<size_t>(index) < conditions.size()) {
                    chosen.push_back(conditions[static_cast<size_t>(index)]);
                }
            }
            if (chosen.empty()) {
                throw std::runtime_error{"no condition chosen"};
            }
            Product x;
            x.source = "GEFS";
            x.id = generatedId("ingredients", v);
            std::string words;
            for (const auto& c : chosen) {
                words += (words.empty() ? "" : " and ") + conditionText(c);
            }
            x.label = "Chance of " + ingredientNames(chosen);   // (the limits are in the line under the title)
            const bool sixHour = std::any_of(chosen.begin(), chosen.end(), [] (const IngredientCondition& c) { return ingredients()[c.ingredient].reqs.front().kind == 1; });
            x.needs = [memberNeeds, sixHourAmount, chosen, sixHour] (int hour) {
                std::vector<GfsData::Need> out;
                if (hour < 3 || (sixHour && (hour < 6 || hour % 6 != 0))) {
                    return out;
                }
                for (size_t slot = 0; slot < chosen.size(); slot++) {
                    for (const auto& req : ingredients()[chosen[slot].ingredient].reqs) {
                        const auto prefix = "s" + std::to_string(slot) + "_" + req.key;
                        for (auto& need : memberNeeds(prefix.c_str(), req.variable, req.level, hour, req.kind == 1 ? sixHourAmount(hour) : std::to_string(hour) + " hour fcst")) {
                            out.push_back(std::move(need));
                        }
                    }
                }
                return out;
            };
            x.derive = [chosen] (Grids& g, const Context&) {
                auto result = g.at("s0_" + std::string{ingredients()[chosen[0].ingredient].reqs.front().key} + memberNames.front());
                std::vector<float> passed(result.values.size(), 0.0f), counted(result.values.size(), 0.0f);
                for (const auto& name : memberNames) {
                    std::vector<char> all(result.values.size(), 1), known(result.values.size(), 1);
                    for (size_t slot = 0; slot < chosen.size(); slot++) {
                        const auto prefix = "s" + std::to_string(slot) + "_";
                        const auto value = ingredients()[chosen[slot].ingredient].value([&g, &prefix, &name] (const char * key) -> const GfsGrid::Grid& { return g.at(prefix + key + name); });
                        for (size_t i = 0; i < all.size(); i++) {
                            if (std::isnan(value.values[i])) {
                                known[i] = 0;
                            } else if (!(value.values[i] > chosen[slot].limit)) {
                                all[i] = 0;
                            }
                        }
                    }
                    for (size_t i = 0; i < all.size(); i++) {
                        if (known[i]) {
                            counted[i] += 1.0f;
                            passed[i] += all[i] ? 1.0f : 0.0f;
                        }
                    }
                }
                for (size_t i = 0; i < result.values.size(); i++) {
                    result.values[i] = counted[i] > 0.0f ? 100.0f * passed[i] / counted[i] : std::nanf("");
                }
                for (auto it = g.begin(); it != g.end();) {   // the members' grids are dropped: 31 of them are a lot to carry
                    it = it->first.compare(0, 1, "s") == 0 && it->first.find('_') != std::string::npos ? g.erase(it) : std::next(it);
                }
                g["share"] = std::move(result);
            };
            x.fill = [] (const Grids& g) { return pick(g, "share"); };
            x.ramp = probability();
            x.palette = "prob";
            x.fillTitle = "Chance of " + words + ", together (% of the 31 members)";
            x.legendStep = 10;
            return x;
        };
        // ---- ingredients met on one run: where each condition holds, and how many of them hold at a point; each condition's own line (its limit) is drawn over the picture, so the one that
        // limits the pattern can be seen
        for (const auto& sourceName : ingredientSources()) {
            made["ingr_" + sourceName] = [sourceName] (const std::vector<double>& v) {
                const auto conditions = ingredientConditions(false, sourceName);
                std::vector<IngredientCondition> chosen;
                for (const double index : v) {
                    if (index >= 0 && static_cast<size_t>(index) < conditions.size()) {
                        chosen.push_back(conditions[static_cast<size_t>(index)]);
                    }
                }
                if (chosen.empty()) {
                    throw std::runtime_error{"no condition chosen"};
                }
                Product x;
                x.source = sourceName;
                x.id = generatedId("ingr_" + sourceName, v);
                std::string words;
                for (const auto& c : chosen) {
                    words += (words.empty() ? "" : ", ") + conditionText(c);
                }
                x.label = "Ingredients met: " + ingredientNames(chosen);   // (the limits are in the line under the title)
                for (size_t slot = 0; slot < chosen.size(); slot++) {
                    for (const auto& req : ingredients()[chosen[slot].ingredient].reqs) {
                        x.wants.push_back(GfsData::Want{"s" + std::to_string(slot) + "_" + req.key, req.variable, req.level, "", ""});
                    }
                }
                x.derive = [chosen] (Grids& g, const Context&) {
                    std::vector<GfsGrid::Grid> values;
                    for (size_t slot = 0; slot < chosen.size(); slot++) {
                        const auto prefix = "s" + std::to_string(slot) + "_";
                        values.push_back(ingredients()[chosen[slot].ingredient].value([&g, &prefix] (const char * key) -> const GfsGrid::Grid& { return g.at(prefix + key); }));
                    }
                    auto count = values.front();
                    for (size_t i = 0; i < count.values.size(); i++) {
                        int met = 0;
                        bool known = false;
                        for (size_t slot = 0; slot < chosen.size(); slot++) {
                            const float value = values[slot].values[i];
                            if (!std::isnan(value)) {
                                known = true;
                                met += value > chosen[slot].limit ? 1 : 0;
                            }
                        }
                        count.values[i] = known ? static_cast<float>(met) : std::nanf("");
                    }
                    for (size_t slot = 0; slot < values.size(); slot++) {
                        g["v" + std::to_string(slot)] = std::move(values[slot]);
                    }
                    g["count"] = std::move(count);
                };
                x.fill = [] (const Grids& g) { return pick(g, "count"); };
                // one condition met: grey; all of them: red (and amber between, when there are three)
                Ramp ramp;
                ramp.banded = true;
                if (chosen.size() == 1) {
                    ramp.stops = {{1.0, QColor{"#d02a1e"}}};
                } else if (chosen.size() == 2) {
                    ramp.stops = {{1.0, QColor{"#c9ced6"}}, {2.0, QColor{"#d02a1e"}}};
                } else {
                    ramp.stops = {{1.0, QColor{"#c9ced6"}}, {2.0, QColor{"#f2b134"}}, {3.0, QColor{"#d02a1e"}}};
                }
                x.ramp = ramp;
                x.fillTitle = "Conditions met at the point (all of them: red): " + words;
                x.legendStep = 0;
                static const QColor lineColors[] = {QColor{20, 70, 190}, QColor{20, 130, 60}, QColor{130, 40, 160}};
                for (size_t slot = 0; slot < chosen.size(); slot++) {
                    ContourSet c;
                    c.key = "v" + std::to_string(slot);
                    c.base = chosen[slot].limit;
                    c.interval = 1.0e9;   // one line, at the limit
                    c.minimum = chosen[slot].limit - 1.0e-6;
                    c.title = conditionText(chosen[slot]);
                    c.color = lineColors[slot % 3];
                    c.uniform = true;
                    c.width = 1.6;
                    c.dashed = slot > 0;
                    c.labelled = false;
                    x.contours.push_back(c);
                }
                return x;
            };
        }
        // ---- the maps NAEFS has, from GEFS's 31 members alone (NAEFS adds the Canadian members and corrects the bias; neither is open): the 10th, 50th and 90th percentile of the members'
        // 10 m wind and 2 m temperature, and the extreme forecast index
        {
            const auto windNeeds = [memberNeeds] (int hour) {
                if (hour < 3) {
                    return std::vector<GfsData::Need>{};
                }
                auto out = memberNeeds("u", "UGRD", "10 m above ground", hour, std::to_string(hour) + " hour fcst");
                for (auto& need : memberNeeds("w", "VGRD", "10 m above ground", hour, std::to_string(hour) + " hour fcst")) {
                    out.push_back(std::move(need));
                }
                return out;
            };
            const auto speedOfMembers = [] (Grids& g) {   // "v<member>": the 10 m wind speed (m/s)
                for (const auto& name : memberNames) {
                    auto u = std::move(g.at("u" + name));
                    const auto& w = g.at("w" + name);
                    for (size_t i = 0; i < u.values.size(); i++) {
                        u.values[i] = std::hypot(u.values[i], w.values[i]);
                    }
                    g["v" + name] = std::move(u);
                    g.erase("u" + name);
                    g.erase("w" + name);
                }
            };
            const auto temperatureNeeds = [memberNeeds] (int hour) { return hour < 3 ? std::vector<GfsData::Need>{} : memberNeeds("v", "TMP", "2 m above ground", hour, std::to_string(hour) + " hour fcst"); };
            const auto pressureNeeds = [memberNeeds] (int hour) { return hour < 3 ? std::vector<GfsData::Need>{} : memberNeeds("v", "PRMSL", "mean sea level", hour, std::to_string(hour) + " hour fcst"); };
            for (const int percent : {10, 50, 90}) {
                for (const bool wind : {true, false}) {
                    Product x;
                    x.source = "GEFS";
                    x.id = std::string{"pct"} + std::to_string(percent) + (wind ? "_10m_wnd" : "_2m_temp");
                    x.label = std::to_string(percent) + (percent == 10 ? "th" : percent == 50 ? "th" : "th") + " percentile " + (wind ? "10m Winds" : "2m Temperature");
                    x.needs = wind ? std::function<std::vector<GfsData::Need>(int)>{windNeeds} : std::function<std::vector<GfsData::Need>(int)>{temperatureNeeds};
                    x.derive = [wind, percent, speedOfMembers] (Grids& g, const Context&) {
                        if (wind) {
                            speedOfMembers(g);
                        }
                        auto out = g.at("v" + memberNames.front());
                        std::vector<float> values;
                        for (size_t i = 0; i < out.values.size(); i++) {
                            values.clear();
                            for (const auto& name : memberNames) {
                                const float v = g.at("v" + name).values[i];
                                if (!std::isnan(v)) {
                                    values.push_back(v);
                                }
                            }
                            if (values.empty()) {
                                out.values[i] = std::nanf("");
                                continue;
                            }
                            std::sort(values.begin(), values.end());
                            const double rank = percent / 100.0 * static_cast<double>(values.size() - 1);
                            const auto low = static_cast<size_t>(std::floor(rank)), high = static_cast<size_t>(std::ceil(rank));
                            out.values[i] = static_cast<float>(values[low] + (values[high] - values[low]) * (rank - static_cast<double>(low)));
                        }
                        g["pct"] = wind ? GfsGrid::scaled(out, 1.943844) : out;   // kt
                        for (const auto& name : memberNames) {
                            g.erase("v" + name);
                        }
                    };
                    x.fill = [] (const Grids& g) { return pick(g, "pct"); };
                    x.ramp = wind ? lowWind() : temperature();
                    x.quantity = wind ? Quantity::Other : Quantity::Temperature;
                    x.fillTitle = std::to_string(percent) + "th percentile of the 31 members: " + (wind ? "10 m wind speed (kt)" : "2 m temperature");
                    x.legendStep = wind ? 10 : 5;
                    p.push_back(x);
                }
            }
            // the members' 2 m temperature in one picture: the median as the shading, the 10th and 90th percentile as numbers at cities (and evenly elsewhere) - the range the
            // ensemble gives at each place. One sort per point makes all three.
            {
                Product x;
                x.source = "GEFS";
                x.id = "pct50_2m_temp_range";
                x.label = "Median 2m Temperature with 10th / 90th percentiles";
                x.needs = temperatureNeeds;
                x.derive = [] (Grids& g, const Context&) {
                    auto p10 = g.at("v" + memberNames.front());
                    auto p50 = p10;
                    auto p90 = p10;
                    std::vector<float> values;
                    const auto at = [&values] (int percent) {
                        const double rank = percent / 100.0 * static_cast<double>(values.size() - 1);
                        const auto low = static_cast<size_t>(std::floor(rank)), high = static_cast<size_t>(std::ceil(rank));
                        return static_cast<float>(values[low] + (values[high] - values[low]) * (rank - static_cast<double>(low)));
                    };
                    for (size_t i = 0; i < p50.values.size(); i++) {
                        values.clear();
                        for (const auto& name : memberNames) {
                            const float v = g.at("v" + name).values[i];
                            if (!std::isnan(v)) {
                                values.push_back(v);
                            }
                        }
                        if (values.empty()) {
                            p10.values[i] = p50.values[i] = p90.values[i] = std::nanf("");
                            continue;
                        }
                        std::sort(values.begin(), values.end());
                        p10.values[i] = at(10);
                        p50.values[i] = at(50);
                        p90.values[i] = at(90);
                    }
                    for (const auto& name : memberNames) {
                        g.erase("v" + name);
                    }
                    g["p10"] = std::move(p10);
                    g["p50"] = std::move(p50);
                    g["p90"] = std::move(p90);
                };
                x.fill = [] (const Grids& g) { return pick(g, "p50"); };
                x.ramp = temperature();
                x.quantity = Quantity::Temperature;
                x.fillTitle = "Median of the 31 members: 2 m temperature. Numbers: 10th percentile (blue), 90th percentile (red)";
                x.legendStep = 5;
                x.pointValues = {{"p10", QColor{30, 80, 200}, "10th percentile 2 m temperature"},
                                 {"p90", QColor{200, 40, 30}, "90th percentile 2 m temperature"}};
                p.push_back(x);
            }
            // the extreme forecast index: where the members' values stand in the climate of the day (the 1991-2020 daily mean and spread of the reanalysis, taken as a normal distribution): +1 when every
            // member is above everything the climate has, -1 below. (ECMWF's index, made with a model climate; this one is a stand-in made with the reanalysis')
            struct Efi {
                const char * id;
                const char * label;
                const char * what;
                int kind;   // 0 temperature, 1 wind, 2 pressure
            };
            for (const auto& e : {Efi{"efi_2m_temp", "Extreme Forecast Index 2m Temperature", "2 m temperature", 0}, Efi{"efi_10m_wnd", "Extreme Forecast Index 10m Winds", "10 m wind speed", 1},
                                  Efi{"efi_mslp", "Extreme Forecast Index mslp", "sea level pressure", 2}}) {
                Product x;
                x.source = "GEFS";
                x.id = e.id;
                x.label = e.label;
                const int kind = e.kind;
                x.needs = kind == 0 ? std::function<std::vector<GfsData::Need>(int)>{temperatureNeeds} : kind == 1 ? std::function<std::vector<GfsData::Need>(int)>{windNeeds} : std::function<std::vector<GfsData::Need>(int)>{pressureNeeds};
                x.derive = [kind, speedOfMembers] (Grids& g, const Context& context) {
                    if (!context.climate) {
                        throw std::runtime_error{"no climatology"};
                    }
                    if (kind == 1) {
                        speedOfMembers(g);
                    }
                    auto t = QDateTime::fromString(QString::fromStdString(context.run.id()), "yyyyMMddHH");
                    t.setTimeZone(QTimeZone::utc());
                    t = t.addSecs(context.hour * 3600LL);
                    const int day = GfsClimate::dayIndex(t.date().year(), t.date().month(), t.date().day());
                    GfsGrid::Grid mean, deviation;
                    std::string error;
                    const auto stat = kind == 0 ? GfsClimate::nearSurfaceTemperature() : kind == 1 ? GfsClimate::nearSurfaceWind() : GfsClimate::seaLevelPressureStat();
                    if (!context.climate->statistics(stat, day, mean, deviation, error)) {
                        throw std::runtime_error{error};
                    }
                    if (kind == 0) {
                        mean = GfsGrid::scaled(mean, 1.0, -273.15);   // kelvins -> the degrees C of the forecast
                    }
                    auto out = g.at("v" + memberNames.front());
                    auto zero = out;
                    std::fill(zero.values.begin(), zero.values.end(), 0.0f);
                    const auto mu = GfsGrid::scaled(GfsGrid::anomaly(zero, mean), -1.0);       // the climate's mean and spread on the members' grid
                    const auto sigma = GfsGrid::scaled(GfsGrid::anomaly(zero, deviation), -1.0);
                    constexpr double pi = 3.14159265358979323846;
                    std::vector<double> ps;
                    for (size_t i = 0; i < out.values.size(); i++) {
                        ps.clear();
                        const double sd = sigma.values[i];
                        for (const auto& name : memberNames) {
                            const float v = g.at("v" + name).values[i];
                            if (!std::isnan(v) && sd > 1e-6) {
                                ps.push_back(0.5 * std::erfc(-(v - mu.values[i]) / sd / std::sqrt(2.0)));   // the member's place in the climate (0 to 1)
                            }
                        }
                        if (ps.empty()) {
                            out.values[i] = std::nanf("");
                            continue;
                        }
                        std::sort(ps.begin(), ps.end());
                        const double n = static_cast<double>(ps.size());
                        // EFI = 2/pi * integral over p of (p - F(p)) / sqrt(p (1 - p)), F the members' share at or under the climate's p: exact for the steps of F
                        const auto up = [] (double p) { return std::asin(std::sqrt(std::clamp(p, 0.0, 1.0))); };
                        const auto lin = [&up] (double p) { return up(p) - std::sqrt(std::clamp(p * (1.0 - p), 0.0, 1.0)); };   // the integral of p / sqrt(p (1 - p))
                        double total = 0.0, from = 0.0;
                        for (size_t j = 0; j <= ps.size(); j++) {
                            const double to = j < ps.size() ? ps[j] : 1.0;
                            total += (lin(to) - lin(from)) - (static_cast<double>(j) / n) * 2.0 * (up(to) - up(from));
                            from = to;
                        }
                        out.values[i] = static_cast<float>(2.0 / pi * total);
                    }
                    g["efi"] = std::move(out);
                    for (const auto& name : memberNames) {
                        g.erase("v" + name);
                    }
                };
                x.fill = [] (const Grids& g) { return pick(g, "efi"); };
                x.ramp = Ramp{{{-1.0, QColor{"#2b4fb0"}}, {-0.8, QColor{"#2b6cb0"}}, {-0.5, QColor{"#7fb6e0"}}, {-0.2, QColor{"#dbeaf5"}}, {0.0, QColor{245, 245, 245, 0}}, {0.2, QColor{"#fbe1c6"}}, {0.5, QColor{"#f0a060"}}, {0.8, QColor{"#d0402a"}}, {1.0, QColor{"#8b1a1a"}}}};
                x.fillTitle = std::string{"Extreme forecast index of the "} + e.what + " (-1 well below to +1 well above the day's climate)";
                x.legendStep = 0.2;
                p.push_back(x);
            }
        }
        // The chance that a member's value passes a limit: the template says what to fetch (`needs`, the members' grids named "<prefix><member>"), how to make one value of them (`collapse`, into
        // "v<member>": a wind speed from its components, a heat index) and how the limit given in the user's unit is in the file's (`toFile`).
        struct Limit {
            const char * id;
            const char * what;     // "a 2 m temperature"
            const char * unit;     // "F"
            bool above;
            std::function<std::vector<GfsData::Need>(int hour)> needs;
            std::function<void(Grids&)> collapse;
            std::function<float(double)> toFile;
        };
        const auto instant = [memberNeeds] (const char * prefix, const char * variable, const char * level) {
            return [=] (int hour) { return hour < 3 ? std::vector<GfsData::Need>{} : memberNeeds(prefix, variable, level, hour, std::to_string(hour) + " hour fcst"); };
        };
        const auto sixHour = [memberNeeds] (const char * prefix, const char * variable, const char * level, const char * kind) {   // the 6 hours ending at the hour: their largest, smallest or average
            return [=] (int hour) {
                return hour < 6 || hour % 6 != 0 ? std::vector<GfsData::Need>{} :
                    memberNeeds(prefix, variable, level, hour, std::to_string(hour - 6) + "-" + std::to_string(hour) + " hour " + kind + " fcst");
            };
        };
        const auto rename = [] (const char * from) {   // the one grid of each member, under the name the comparison reads
            return [from] (Grids& g) {
                for (const auto& name : memberNames) {
                    g["v" + name] = std::move(g.at(std::string{from} + name));
                    g.erase(std::string{from} + name);
                }
            };
        };
        const auto fahrenheit = [] (double f) { return static_cast<float>((f - 32.0) * 5.0 / 9.0); };   // the grids are in degrees C (GDAL turns the GRIB temperatures from K)
        const auto scaled = [] (double factor) { return [factor] (double v) { return static_cast<float>(v * factor); }; };
        const auto together = [memberNeeds] (std::vector<std::function<std::vector<GfsData::Need>(int)>> parts) {
            return [parts] (int hour) {
                std::vector<GfsData::Need> out;
                for (const auto& part : parts) {
                    for (auto& need : part(hour)) {
                        out.push_back(std::move(need));
                    }
                }
                return out;
            };
        };
        const auto speed = [] (Grids& g) {   // 10 m wind speed (m/s) from the components
            for (const auto& name : memberNames) {
                auto u = std::move(g.at("u" + name));
                const auto& w = g.at("w" + name);
                for (size_t i = 0; i < u.values.size(); i++) {
                    u.values[i] = std::hypot(u.values[i], w.values[i]);
                }
                g["v" + name] = std::move(u);
                g.erase("u" + name);
                g.erase("w" + name);
            }
        };
        const auto heat = [] (Grids& g) {   // the heat index (K) from the 2 m temperature and humidity: the National Weather Service's regression, with its adjustments
            for (const auto& name : memberNames) {
                auto t = std::move(g.at("t" + name));
                const auto& rh = g.at("h" + name);
                for (size_t i = 0; i < t.values.size(); i++) {
                    const double f = t.values[i] * 9.0 / 5.0 + 32.0, r = rh.values[i];
                    double hi = f;
                    if (f >= 80.0) {
                        hi = -42.379 + 2.04901523 * f + 10.14333127 * r - 0.22475541 * f * r - 0.00683783 * f * f - 0.05481717 * r * r + 0.00122874 * f * f * r + 0.00085282 * f * r * r - 0.00000199 * f * f * r * r;
                        if (r < 13.0 && f <= 112.0) {
                            hi -= (13.0 - r) / 4.0 * std::sqrt((17.0 - std::abs(f - 95.0)) / 17.0);
                        } else if (r > 85.0 && f <= 87.0) {
                            hi += (r - 85.0) / 10.0 * (87.0 - f) / 5.0;
                        }
                    }
                    t.values[i] = static_cast<float>((hi - 32.0) * 5.0 / 9.0);
                }
                g["v" + name] = std::move(t);
                g.erase("t" + name);
                g.erase("h" + name);
            }
        };
        const std::vector<Limit> limits{
            {"temp_under", "a 2 m temperature", "F", false, instant("v", "TMP", "2 m above ground"), nullptr, fahrenheit},
            {"temp_over", "a 2 m temperature", "F", true, instant("v", "TMP", "2 m above ground"), nullptr, fahrenheit},
            {"tmin_under", "a 6 hour minimum temperature", "F", false, sixHour("v", "TMIN", "2 m above ground", "min"), nullptr, fahrenheit},
            {"tmax_over", "a 6 hour maximum temperature", "F", true, sixHour("v", "TMAX", "2 m above ground", "max"), nullptr, fahrenheit},
            {"tmax_under", "a 6 hour maximum temperature", "F", false, sixHour("v", "TMAX", "2 m above ground", "max"), nullptr, fahrenheit},
            {"dewpoint_over", "a 2 m dewpoint", "F", true, instant("v", "DPT", "2 m above ground"), nullptr, fahrenheit},
            {"heat_over", "a heat index", "F", true, together({instant("t", "TMP", "2 m above ground"), instant("h", "RH", "2 m above ground")}), heat, fahrenheit},
            {"rh_under", "a 2 m relative humidity", "%", false, instant("v", "RH", "2 m above ground"), nullptr, scaled(1.0)},
            {"wind_over", "a 10 m wind", "kt", true, together({instant("u", "UGRD", "10 m above ground"), instant("w", "VGRD", "10 m above ground")}), speed, scaled(0.514444)},
            {"gust_over", "wind gusts", "kt", true, instant("v", "GUST", "surface"), nullptr, scaled(0.514444)},
            {"mslp_under", "a sea level pressure", "mb", false, instant("v", "PRMSL", "mean sea level"), nullptr, scaled(100.0)},
            {"mslp_over", "a sea level pressure", "mb", true, instant("v", "PRMSL", "mean sea level"), nullptr, scaled(100.0)},
            {"pw_over", "precipitable water", "in", true, instant("v", "PWAT", "entire atmosphere (considered as a single layer)"), nullptr, scaled(25.4)},
            {"cloud_under", "a 6 hour mean cloud cover", "%", false, sixHour("v", "TCDC", "entire atmosphere", "ave"), nullptr, scaled(1.0)},
            {"cloud_over", "a 6 hour mean cloud cover", "%", true, sixHour("v", "TCDC", "entire atmosphere", "ave"), nullptr, scaled(1.0)},
            {"cape_over", "surface-based CAPE", "J/kg", true, instant("v", "CAPE", "surface"), nullptr, scaled(1.0)},
            {"mucape_over", "most unstable CAPE", "J/kg", true, instant("v", "CAPE", "255-0 mb above ground"), nullptr, scaled(1.0)},
            {"z500_over", "a 500 mb height", "m", true, instant("v", "HGT", "500 mb"), nullptr, scaled(1.0)},
        };
        for (const auto& limit : limits) {
            made[limit.id] = [limit, share] (const std::vector<double>& v) {
                const double value = v.at(0);
                Product x;
                x.source = "GEFS";
                x.id = generatedId(limit.id, {value});
                const std::string words = std::string{"Chance of "} + limit.what + (limit.above ? " over " : " under ") + trimmed(value) + " " + limit.unit;
                x.label = words;
                x.needs = limit.needs;
                x.derive = [limit, share, value] (Grids& g, const Context&) {
                    if (limit.collapse) {
                        limit.collapse(g);
                    }
                    const float edge = limit.toFile(value);
                    const bool above = limit.above;
                    share(g, [edge, above] (const Grids& grids, const std::string& name, size_t i) {
                        const float a = grids.at("v" + name).values[i];
                        return std::isnan(a) ? -1 : (above ? a > edge : a < edge) ? 1 : 0;
                    }, "share", {"v"});
                };
                x.fill = [] (const Grids& g) { return pick(g, "share"); };
                x.ramp = probability();
                x.palette = "prob";
                x.fillTitle = words + " (% of the 31 members)";
                x.legendStep = 10;
                return x;
            };
        }
    }
    for (const auto& [id, inches, text] : {std::tuple{"prob_precip_0.25in", 0.25, "0.25"}, {"prob_precip_0.5in", 0.5, "0.50"}, {"prob_precip_1in", 1.0, "1.00"}}) {
        const double mm = inches * 25.4;
        p.push_back(shareChart(id, std::string{"Probability of 6-hour Precipitation over "} + text + " in", std::string{"Chance of more than "} + text + " in of precipitation in 6 hours (% of the 31 members)",
                               [memberNeeds, sixHourAmount] (int hour) { return hour < 6 ? std::vector<GfsData::Need>{} : memberNeeds("a", "APCP", "surface", hour, sixHourAmount(hour)); },
                               [share, mm] (Grids& g) {
                                   share(g, [mm] (const Grids& grids, const std::string& name, size_t i) {
                                       const float a = grids.at("a" + name).values[i];
                                       return std::isnan(a) ? -1 : a > mm ? 1 : 0;
                                   }, "share", {"a"});
                               }));
    }
    p.push_back(shareChart("prob_ice_0.25in", "Probability of Ice Pellets over 0.25 in", "Chance of 0.25 in or more of precipitation falling as ice pellets in 6 hours (% of the 31 members)",
                           [memberNeeds, sixHourAmount, sixHourAverage] (int hour) {
                               if (hour < 6) {
                                   return std::vector<GfsData::Need>{};
                               }
                               auto out = memberNeeds("a", "APCP", "surface", hour, sixHourAmount(hour));
                               for (auto& need : memberNeeds("i", "CICEP", "surface", hour, sixHourAverage(hour))) {   // the share of the 6 hours with ice pellets
                                   out.push_back(std::move(need));
                               }
                               return out;
                           },
                           [share] (Grids& g) {   // a member counts when it makes the amount and most of that time is ice pellets (an approximation: the amount of the ice pellets alone is not in the files)
                               share(g, [] (const Grids& grids, const std::string& name, size_t i) {
                                   const float a = grids.at("a" + name).values[i], ice = grids.at("i" + name).values[i];
                                   return std::isnan(a) || std::isnan(ice) ? -1 : (a >= 6.35f && ice >= 0.5f) ? 1 : 0;
                               }, "share", {"a", "i"});
                           }));
    for (const int joules : {250, 500, 2000, 4000}) {
        p.push_back(shareChart(("prob_cape_" + std::to_string(joules)).c_str(), "Probability of CAPE over " + std::to_string(joules), "Chance of surface-based CAPE over " + std::to_string(joules) + " J/kg (% of the 31 members)",
                               [memberNeeds] (int hour) { return memberNeeds("c", "CAPE", "surface", hour, std::to_string(hour) + " hour fcst"); },
                               [share, joules] (Grids& g) {
                                   share(g, [joules] (const Grids& grids, const std::string& name, size_t i) {
                                       const float c = grids.at("c" + name).values[i];
                                       return std::isnan(c) ? -1 : c > static_cast<float>(joules) ? 1 : 0;
                                   }, "share", {"c"});
                               }));
    }
    {   // the precipitation type that most of the members' 6 hours had (the shares of the time with rain, snow, freezing rain and ice pellets are in the mean files): 1 rain, 2 snow, 3 freezing rain, 4 ice pellets
        Product x;
        x.source = "GEFS";
        x.id = "dom_precip_type";
        x.label = "Dominant Precipitation Type";
        x.needs = [sixHourAmount, sixHourAverage] (int hour) {
            if (hour < 6) {
                return std::vector<GfsData::Need>{};
            }
            return std::vector<GfsData::Need>{{hour, {"a", "APCP", "surface", sixHourAmount(hour), "", ""}}, {hour, {"r", "CRAIN", "surface", sixHourAverage(hour), "", ""}}, {hour, {"s", "CSNOW", "surface", sixHourAverage(hour), "", ""}},
                                              {hour, {"f", "CFRZR", "surface", sixHourAverage(hour), "", ""}}, {hour, {"i", "CICEP", "surface", sixHourAverage(hour), "", ""}}};
        };
        x.derive = [] (Grids& g, const Context&) {
            auto type = g["a"];
            for (size_t i = 0; i < type.values.size(); i++) {
                if (std::isnan(g["a"].values[i]) || g["a"].values[i] < 0.254f) {   // under 0.01 inch: no precipitation to name
                    type.values[i] = std::nanf("");
                    continue;
                }
                const float shares[4] = {g["r"].values[i], g["s"].values[i], g["f"].values[i], g["i"].values[i]};
                int best = 0;
                for (int k = 1; k < 4; k++) {
                    best = shares[k] > shares[best] ? k : best;
                }
                type.values[i] = static_cast<float>(best + 1);
            }
            g["type"] = std::move(type);
        };
        x.fill = [] (const Grids& g) { return pick(g, "type"); };
        x.ramp = Ramp{{{1, QColor{0, 200, 0}}, {2, QColor{30, 100, 255}}, {3, QColor{230, 20, 20}}, {4, QColor{150, 40, 200}}}, true};
        x.fillTitle = "Dominant precipitation type: 1 rain (green), 2 snow (blue), 3 freezing rain (red), 4 ice pellets (purple)";
        x.legendStep = 0;
        p.push_back(x);
    }
    {   // the change of the snow depth over the 24 hours before: the mean of the members, and how much the members differ in it
        const auto depthNeeds = [memberNeeds] (int hour, bool members) {
            const int start = std::max(hour - 24, 0);
            std::vector<GfsData::Need> out;
            if (members) {
                out = memberNeeds("n", "SNOD", "surface", hour, std::to_string(hour) + " hour fcst");
                for (auto& need : memberNeeds("o", "SNOD", "surface", start, std::to_string(start) + " hour fcst")) {
                    out.push_back(std::move(need));
                }
            } else {
                out = {{hour, {"n", "SNOD", "surface", std::to_string(hour) + " hour fcst", "", ""}}, {start, {"o", "SNOD", "surface", std::to_string(start) + " hour fcst", "", ""}}};
            }
            return out;
        };
        Product mean;
        mean.source = "GEFS";
        mean.id = "snodpth_chng_mean";
        mean.label = "Mean Snow Depth Change from the previous 24 hours";
        mean.needs = [depthNeeds] (int hour) { return depthNeeds(hour, false); };
        mean.fill = [] (const Grids& g) { return GfsGrid::scaled(GfsGrid::difference(pick(g, "n"), pick(g, "o")), 100.0); };   // meters -> centimeters
        mean.ramp = snowChange();
        mean.palette = "snowdepth";
        mean.quantity = Quantity::Centimeters;
        mean.fillTitle = "Mean change of the snow depth in the previous 24 hours";
        mean.legendStep = 0;
        p.push_back(mean);
        Product spreadChart = mean;
        spreadChart.id = "snodpth_chng_sprd";
        spreadChart.label = "Spread of the Snow Depth Change from the previous 24 hours";
        spreadChart.needs = [depthNeeds] (int hour) { return depthNeeds(hour, true); };
        spreadChart.derive = [] (Grids& g, const Context&) {   // the spread (standard deviation) among the members of each one's change
            auto result = g.at("n" + memberNames.front());
            for (size_t i = 0; i < result.values.size(); i++) {
                double sum = 0.0, square = 0.0;
                int n = 0;
                for (const auto& name : memberNames) {
                    const float change = g.at("n" + name).values[i] - g.at("o" + name).values[i];
                    if (!std::isnan(change)) {
                        sum += change;
                        square += static_cast<double>(change) * change;
                        n++;
                    }
                }
                result.values[i] = n > 1 ? static_cast<float>(100.0 * std::sqrt(std::max(0.0, square / n - (sum / n) * (sum / n)))) : std::nanf("");   // centimeters
            }
            g["spread"] = std::move(result);
            for (const auto& name : memberNames) {
                g.erase("n" + name);
                g.erase("o" + name);
            }
        };
        spreadChart.fill = [] (const Grids& g) { return pick(g, "spread"); };
        spreadChart.ramp = spreadRamp(10.0);
        spreadChart.fillTitle = "Spread among the members of the snow depth change";
        p.push_back(spreadChart);
    }

    // ---- the spaghetti maps: one contour (or two) of a field from each of the 31 members, each member a color of its own, and the ensemble mean as the heavy black line
    const auto spaghetti = [&] (const std::string& id, const std::string& label, const char * variable, const char * level, double scale, std::vector<double> levels, const std::string& what) {
        Product x;
        x.source = "GEFS";
        x.id = id;
        x.label = label;
        x.linesOnly = true;
        x.needs = [memberNeeds, variable, level] (int hour) {
            if (hour < 3) {
                return std::vector<GfsData::Need>{};   // the members have no forecast record at hour 0
            }
            const auto when = std::to_string(hour) + " hour fcst";
            auto out = memberNeeds("z", variable, level, hour, when);
            out.push_back({hour, GfsData::Want{"zm", variable, level, when, "", ""}});
            return out;
        };
        x.fillTitle = what + ": one thin line for each of the 31 members, the heavy black line the ensemble mean";
        x.derive = [] (Grids& g, const Context&) {   // members taken out (the Layers box): the mean of the rest, not the model's own mean of all 31
            std::vector<const GfsGrid::Grid *> kept;
            for (const auto& name : memberNames) {
                const auto found = g.find("z" + name);
                if (found != g.end() && !found->second.values.empty()) {
                    kept.push_back(&found->second);
                }
            }
            if (kept.empty() || kept.size() == memberNames.size() || !g.count("zm")) {
                return;
            }
            auto mean = *kept.front();
            for (size_t i = 0; i < mean.values.size(); i++) {
                double sum = 0.0;
                int n = 0;
                for (const auto * m : kept) {
                    if (std::isfinite(m->values[i])) {
                        sum += m->values[i];
                        n++;
                    }
                }
                mean.values[i] = n > 0 ? static_cast<float>(sum / n) : std::nanf("");
            }
            g["zm"] = std::move(mean);
        };
        for (size_t m = 0; m < memberNames.size(); m++) {
            x.layers.push_back({"z" + memberNames[m], "GEFS " + memberNames[m]});
        }
        for (const double value : levels) {
            for (size_t m = 0; m < memberNames.size(); m++) {
                ContourSet c;
                c.key = "z" + memberNames[m];
                c.scale = scale;
                c.base = value;
                c.interval = 100000.0;   // one level: the value
                c.color = QColor::fromHsv(static_cast<int>(m * 360 / memberNames.size()), 230, 190);
                c.width = 1.0;
                c.labelled = false;
                c.uniform = true;
                x.contours.push_back(c);
            }
            ContourSet mean;
            mean.key = "zm";
            mean.scale = scale;
            mean.base = value;
            mean.interval = 100000.0;
            mean.color = QColor{10, 10, 10};
            mean.width = 2.6;
            mean.uniform = true;
            x.contours.push_back(mean);
        }
        return x;
    };
    for (const int value : {1176, 1188, 1200, 1212, 1224, 1230}) {
        p.push_back(spaghetti("200_" + std::to_string(value) + "_ht", "200mb " + std::to_string(value) + " Height Contours", "HGT", "200 mb", 0.1, {static_cast<double>(value)}, "200 mb height of " + std::to_string(value) + " dam"));
    }
    for (const auto& [low, high] : {std::pair{510, 552}, {516, 558}, {522, 564}, {528, 570}, {534, 576}, {540, 582}}) {
        p.push_back(spaghetti("500_" + std::to_string(low) + "_" + std::to_string(high) + "_ht", "500mb " + std::to_string(low) + "/" + std::to_string(high) + " Height Contours", "HGT", "500 mb", 0.1,
                              {static_cast<double>(low), static_cast<double>(high)}, "500 mb heights of " + std::to_string(low) + " and " + std::to_string(high) + " dam"));
    }
    for (const auto& [low, high] : {std::pair{984, 1024}, {996, 1036}, {1000, 1040}, {1004, 1044}, {1008, 1048}, {1012, 1052}}) {
        p.push_back(spaghetti("mslp_" + std::to_string(low) + "_" + std::to_string(high) + "_iso", "MSLP " + std::to_string(low) + "/" + std::to_string(high) + " Isobar Contours", "PRMSL", "mean sea level", 0.01,
                              {static_cast<double>(low), static_cast<double>(high)}, "sea level pressure of " + std::to_string(low) + " and " + std::to_string(high) + " mb"));
    }

    // ---- the maps of the model guidance site that put the mean and the spread of the 30 members on one chart: the mean as lines and barbs with the spread as the fill, or the mean as the
    // fill with the spread as lines
    const auto windSpread = [] (Grids& g) {   // the spread of the wind speed from the spread of its two components
        auto out = g["su"];
        for (size_t i = 0; i < out.values.size(); i++) {
            out.values[i] = static_cast<float>(std::hypot(g["su"].values[i], g["sv"].values[i]));
        }
        g["ws"] = std::move(out);
    };
    const auto meanContours = [] (const char * key, double interval, const char * title, QColor color) {
        ContourSet c;
        c.key = key;
        c.interval = interval;
        c.title = title;
        c.color = color;
        return c;
    };
    for (const auto& [lvl, top] : {std::pair{"250", 20.0}, {"500", 16.0}, {"700", 12.0}, {"850", 12.0}, {"925", 12.0}}) {   // winds: the mean as barbs, the spread of the speed as the fill (the scale reaches the 10 m/s and more a storm makes)
        const std::string level = std::string{lvl} + " mb";
        Product x;
        x.source = "GEFS";
        x.id = std::string{lvl} + "_wnd";
        x.label = "Mean " + std::string{lvl} + "mb Winds and Spread";
        x.wants = {want("u", "UGRD", level.c_str()), want("v", "VGRD", level.c_str()), spr("su", "UGRD", level.c_str()), spr("sv", "VGRD", level.c_str())};
        x.derive = [windSpread] (Grids& g, const Context&) { windSpread(g); };
        x.fill = [] (const Grids& g) { return pick(g, "ws"); };
        x.ramp = spreadRamp(top);
        x.palette = "spread_wind";
        x.fillTitle = "Spread of the " + std::string{lvl} + " mb wind speed (m/s)";
        x.legendStep = 2;
        x.barbU = "u";
        x.barbV = "v";
        p.push_back(x);
    }
    for (const auto& [lvl, top] : {std::pair{"250", 5.0}, {"500", 3.0}, {"700", 3.0}, {"850", 4.0}}) {   // temperatures: the mean as lines, the spread as the fill
        const std::string level = std::string{lvl} + " mb";
        Product x;
        x.source = "GEFS";
        x.id = std::string{lvl} + "_temp";
        x.label = "Mean " + std::string{lvl} + "mb Temperature and Spread";
        x.wants = {want("t", "TMP", level.c_str()), spr("s", "TMP", level.c_str())};
        x.fill = [] (const Grids& g) { return pick(g, "s"); };
        x.ramp = spreadRamp(top);
        x.fillTitle = "Spread of the " + std::string{lvl} + " mb temperature (K)";
        x.legendStep = 1;
        x.contours = {meanContours("t", 5, "Mean temperature (C)", QColor{30, 30, 30})};
        p.push_back(x);
    }
    for (const auto& [lvl, interval, top] : {std::tuple{"500", 6.0, 60.0}, {"700", 3.0, 40.0}, {"850", 3.0, 30.0}}) {   // height (mean solid, spread the fill) with the mean vorticity dashed
        const std::string level = std::string{lvl} + " mb";
        Product x;
        x.source = "GEFS";
        x.id = std::string{lvl} + "_vort_ht";
        x.label = "Mean " + std::string{lvl} + "mb Height, Vorticity and Spread";
        x.wants = {want("z", "HGT", level.c_str()), spr("s", "HGT", level.c_str()), want("u", "UGRD", level.c_str()), want("v", "VGRD", level.c_str())};
        x.derive = [] (Grids& g, const Context&) { g["vo"] = GfsGrid::scaled(GfsGrid::vorticity(GfsGrid::smoothed(g["u"], 1), GfsGrid::smoothed(g["v"], 1)), 1e5); };
        x.fill = [] (const Grids& g) { return pick(g, "s"); };
        x.ramp = spreadRamp(top);
        x.fillTitle = "Spread of the " + std::string{lvl} + " mb height (m)";
        x.legendStep = top / 6;
        auto vort = meanContours("vo", 4, "Mean vorticity (1e-5 /s)", QColor{20, 90, 150});
        vort.dashed = true;
        vort.minimum = 4;   // the cyclonic side, as the model guidance site draws it
        x.contours = {meanHeights(interval), vort};
        p.push_back(x);
    }
    {   // sea level pressure: the mean as lines, the spread as the fill
        Product x;
        x.source = "GEFS";
        x.id = "mslp";
        x.label = "Mean Sea Level Pressure and Spread";
        x.wants = {want("p", "PRMSL", "mean sea level"), spr("s", "PRMSL", "mean sea level")};
        x.fill = [] (const Grids& g) { return GfsGrid::scaled(pick(g, "s"), 0.01); };
        x.ramp = spreadRamp(6.0);
        x.palette = "spread_mslp";
        x.fillTitle = "Spread of the sea level pressure (mb)";
        x.legendStep = 1;
        x.contours = {pressure()};
        p.push_back(x);
    }
    {
        Product x;
        x.source = "GEFS";
        x.id = "10m_wnd";
        x.label = "Mean 10m Winds and Spread";
        x.wants = {want("u", "UGRD", "10 m above ground"), want("v", "VGRD", "10 m above ground"), spr("su", "UGRD", "10 m above ground"), spr("sv", "VGRD", "10 m above ground")};
        x.derive = [windSpread] (Grids& g, const Context&) { windSpread(g); };
        x.fill = [] (const Grids& g) { return pick(g, "ws"); };
        x.ramp = spreadRamp(12.0);
        x.palette = "spread_wind";
        x.fillTitle = "Spread of the 10 m wind speed (m/s)";
        x.legendStep = 2;
        x.barbU = "u";
        x.barbV = "v";
        p.push_back(x);
    }
    {
        Product x;
        x.source = "GEFS";
        x.id = "2m_temp";
        x.label = "Mean 2m Temperature and Spread";
        x.wants = {want("t", "TMP", "2 m above ground"), spr("s", "TMP", "2 m above ground")};
        x.fill = [] (const Grids& g) { return pick(g, "s"); };
        x.ramp = spreadRamp(5.0);
        x.fillTitle = "Spread of the 2 m temperature (K)";
        x.legendStep = 1;
        x.contours = {meanContours("t", 5, "Mean temperature (C)", QColor{30, 30, 30})};
        p.push_back(x);
    }
    {   // CAPE: the mean the fill, the spread lines
        Product x;
        x.source = "GEFS";
        x.id = "cape";
        x.label = "Mean CAPE and Spread";
        x.wants = {want("c", "CAPE", "surface"), spr("s", "CAPE", "surface")};
        x.fill = [] (const Grids& g) { return pick(g, "c"); };
        x.ramp = capeRamp();
        x.fillTitle = "Mean CAPE (J/kg)";
        x.legendStep = 500;
        {
            auto c = meanContours("s", 250, "Spread of the CAPE (J/kg)", QColor{30, 30, 30});
            c.minimum = 250;
            x.contours = {c};
        }
        p.push_back(x);
    }
    for (const int length : {6, 24}) {   // the precipitation of 6 and 24 hours: the mean the fill, the spread lines. The 24 hour mean is its four 6 hour means; its spread is estimated from theirs
        Product x;
        x.source = "GEFS";
        x.id = length == 6 ? "precip_p06" : "precip_p24";
        x.label = length == 6 ? "Mean 6-hour Precipitation and Spread" : "Mean 24-hour Precipitation and Spread";
        const int pieces = length / 6;
        x.needs = [pieces] (int hour) {
            std::vector<GfsData::Need> out;
            for (int k = 0; k < pieces; k++) {
                const int end = hour - 6 * k;
                if (end < 6) {
                    break;
                }
                const auto when = std::to_string(end - 6) + "-" + std::to_string(end) + " hour acc fcst";
                out.push_back({end, GfsData::Want{"a" + std::to_string(k), "APCP", "surface", when, "", ""}});
                out.push_back({end, GfsData::Want{"s" + std::to_string(k), "APCP", "surface", when, "", "spr"}});
            }
            return out;
        };
        x.derive = [pieces] (Grids& g, const Context&) {
            auto mean = g["a0"], sprd = g["s0"];
            for (size_t i = 0; i < mean.values.size(); i++) {
                double m = 0.0, q = 0.0;
                for (int k = 0; k < pieces && g.count("a" + std::to_string(k)); k++) {
                    m += g["a" + std::to_string(k)].values[i];
                    q += static_cast<double>(g["s" + std::to_string(k)].values[i]) * g["s" + std::to_string(k)].values[i];
                }
                mean.values[i] = static_cast<float>(m);
                sprd.values[i] = static_cast<float>(std::sqrt(q));
            }
            g["mean"] = mean;
            g["spread"] = sprd;
        };
        x.fill = [] (const Grids& g) { return pick(g, "mean"); };
        x.ramp = precipitation();
    x.palette = "precip";
        x.quantity = Quantity::Millimeters;
        x.fillTitle = std::string{"Mean precipitation, "} + std::to_string(length) + " hours";
        x.legendStep = 0;
        ContourSet c;
        c.key = "spread";
        c.scale = 1.0 / 25.4;   // millimeters -> inches
        c.interval = 0.1;
        c.minimum = 0.1;   // lines of real spread, not a 0 line round every dry area
        c.title = "Spread (in)";
        c.color = QColor{30, 30, 30};
        x.contours = {c};
        p.push_back(x);
    }
}

}  // namespace detail
}  // namespace GfsChart
