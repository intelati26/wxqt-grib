// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

// The National Blend of Models (NBM), and the real-time and unrestricted mesoscale analyses (RTMA, URMA) that share its way of reading the files.
#include "gfs/GfsChartInternal.h"
#include <QTimeZone>
#include "gfs/AirNowLoader.h"
#include "gfs/GfsNormals.h"
#include <regex>

namespace GfsChart {
namespace detail {

void nbmProducts(std::vector<Product>& p) {
// ---- the National Blend of Models: the plain field of each (NWS's blend of many models, 2.5 km over the contiguous United States)
const auto nbmWant = [] (const std::string& key, const char * variable, const char * level, const std::string& forecast, const std::string& detail = "") {
    return GfsData::Want{key, variable, level, forecast, detail};
};
const auto atHour = [] (int hour) { return std::to_string(hour) + " hour fcst"; };
const auto window = [] (int hour, int length, const char * kind) { return std::to_string(hour - length) + "-" + std::to_string(hour) + " hour " + kind + " fcst"; };
// wind from its speed and direction: u and v in m/s as the barbs and the speed fills want them
const auto windComponents = [] (Grids& g, const Context&) {
    auto u = g["ws"], v = g["ws"];
    for (size_t i = 0; i < u.values.size(); i++) {
        const double speedNow = g["ws"].values[i], from = g["wd"].values[i] * pi / 180.0;
        u.values[i] = static_cast<float>(-speedNow * std::sin(from));
        v.values[i] = static_cast<float>(-speedNow * std::cos(from));
    }
    g["u"] = std::move(u);
    g["v"] = std::move(v);
};
const auto nbmSurface = [&] (const char * id, const char * label, const char * variable, const char * level, const Ramp& ramp, const char * title, Quantity quantity, double step) {
    Product x;
    x.source = "NBM";
    x.id = id;
    x.label = label;
    x.needs = [=] (int hour) {
        return std::vector<GfsData::Need>{{hour, nbmWant("f", variable, level, std::to_string(hour) + " hour fcst")}, {hour, nbmWant("ws", "WIND", "10 m above ground", std::to_string(hour) + " hour fcst")},
                                          {hour, nbmWant("wd", "WDIR", "10 m above ground", std::to_string(hour) + " hour fcst")}};
    };
    x.derive = windComponents;
    x.fill = [] (const Grids& g) { return pick(g, "f"); };
    x.ramp = ramp;
    x.fillTitle = title;
    x.quantity = quantity;
    x.legendStep = step;
    x.barbU = "u";
    x.barbV = "v";
    return x;
};
p.push_back(nbmSurface("2m_temp_10m_wnd", "2 m Temperature and 10 m Wind", "TMP", "2 m above ground", temperature(), "2 m temperature", Quantity::Temperature, 5));
p.push_back(nbmSurface("2m_dewp_10m_wnd", "2 m Dewpoint and 10 m Wind", "DPT", "2 m above ground", dewpoint(), "2 m dewpoint", Quantity::Temperature, 5));
p.push_back(nbmSurface("2m_relh_10m_wnd", "2 m Relative Humidity and 10 m Wind", "RH", "2 m above ground", humidity(), "2 m relative humidity (%)", Quantity::Other, 10));
p.push_back(nbmSurface("2m_apparent_temp", "2 m Apparent Temperature and 10 m Wind", "APTMP", "2 m above ground", temperature(), "2 m apparent temperature", Quantity::Temperature, 5));
p.push_back(nbmSurface("total_cloud_cover", "Total Cloud Cover and 10 m Wind", "TCDC", "surface", cloudCover(), "Total cloud cover (%)", Quantity::Other, 10));
{   // the gust is the fill; the barbs are the sustained wind
    auto x = nbmSurface("10m_wnd_gust", "10 m Wind and Gust", "GUST", "10 m above ground", lowWind(), "10 m wind gust (kt)", Quantity::Other, 10);
    x.fill = [] (const Grids& g) { return GfsGrid::scaled(pick(g, "f"), msToKnots); };
    p.push_back(x);
}
// ---- the nice weather outlook (a work in progress): where the daytime weather of a day is the best for being outside, in five levels (Decent, Fair, Nice, Beautiful, Perfect). The idea and the
// levels are the reference outlook's; the scoring is our own and is meant to be tuned. A frame is a day: the forecast hour 24 is day 1, 48 day 2 ... 168 day 7. The apparent temperature, dewpoint,
// cloud cover, gusts and chance of rain at the hours of the local daytime (9 to 19 o'clock by the sun, worked out for each cell from its longitude) are scored and put together; a top level also
// needs every ingredient to be good (the gates), and the further the day the lower the level it can reach. The RRFS version (days 1 to 3) also has the near-surface smoke (PM2.5), which can
// cap or remove a level.
for (const std::string source : {"NBM"}) {
    const bool blend = true;
    const int lastDay = 8;
    Product x;
    x.source = source;
    x.id = "nice_weather";
    x.label = "Nice Weather Outlook (hour 24 = day 1)";
    x.periodHours = 24;
    x.mainCycleStep = 6;   // the long hours are made by the main runs
    const auto sampleHours = [blend] (int day) {   // the forecast hours that can fall in a cell's daytime of that day, whatever the run's hour: 3-hourly where the model has them, then (the blend) 6-hourly
        std::vector<int> hours;
        for (int f = 24 * (day - 1) + 3; f <= 24 * (day - 1) + 36; f += 3) {   // (to 36 hours: a late run's tomorrow reaches to the evening of the west coast)
            if (!blend || f <= 36 || f % 6 == 0) {
                hours.push_back(f);
            }
        }
        return hours;
    };
    x.needs = [=] (int hour) {
        std::vector<GfsData::Need> needs;
        const int day = std::clamp(hour / 24, 1, lastDay);
        for (const int f : sampleHours(day)) {
            const std::string at = std::to_string(f);
            if (blend) {
                needs.push_back({f, nbmWant("a" + at, "APTMP", "2 m above ground", atHour(f))});
                needs.push_back({f, nbmWant("d" + at, "DPT", "2 m above ground", atHour(f))});
                needs.push_back({f, nbmWant("c" + at, "TCDC", "surface", atHour(f))});
                needs.push_back({f, nbmWant("g" + at, "GUST", "10 m above ground", atHour(f))});
                needs.push_back({f, nbmWant("u" + at, "WIND", "10 m above ground", atHour(f))});
                needs.push_back({f, nbmWant("p" + at, "APCP", "surface", window(f, f <= 36 ? 1 : 6, "acc"), "prob >0.254")});
            }
        }
        return needs;
    };
    x.derive = [=] (Grids& g, const Context& context) {
        const int day = std::clamp(context.hour / 24, 1, lastDay);
        const auto hours = sampleHours(day);
        const auto& shape = g["a" + std::to_string(hours.front())];
        GfsGrid::Grid level = shape;
        const QDate runDate = QDate::fromString(QString::fromStdString(context.run.date), "yyyyMMdd");
        const double runHours = QDate{1970, 1, 1}.daysTo(runDate) * 24.0 + std::atoi(context.run.cycle.c_str());
        const auto ramp = [] (double v, double good, double bad) { return std::clamp((bad - v) / (bad - good), 0.0, 1.0); };   // 1 at `good`, 0 at `bad` (either way round)
        // the curves of the ingredients' scores (0 to 1): a flat top where the weather is no bother, a steepening loss, and a floor or an end. Shaped by what the reference outlook's published city scores show of
        // how people take each (their numbers, read as curves, not copied rules).
        using Curve = std::vector<std::pair<double, double>>;
        const auto along = [] (const Curve& curve, double v) {
            if (v <= curve.front().first) {
                return curve.front().second;
            }
            for (size_t i = 1; i < curve.size(); i++) {
                if (v <= curve[i].first) {
                    const double f = (v - curve[i - 1].first) / (curve[i].first - curve[i - 1].first);
                    return curve[i - 1].second + f * (curve[i].second - curve[i - 1].second);
                }
            }
            return curve.back().second;
        };
        static const Curve skyCurve{{0, 1.0}, {12, 1.0}, {16, 0.994}, {20, 0.967}, {24, 0.94}, {28, 0.913}, {32, 0.88}, {36, 0.84}, {40, 0.80}, {44, 0.76}, {48, 0.72}, {52, 0.66}, {56, 0.58}, {60, 0.50}, {64, 0.421}, {68, 0.34}, {72, 0.274}, {76, 0.22}, {80, 0.167}, {84, 0.113}, {88, 0.08}, {92, 0.053}, {96, 0.027}, {100, 0.0}};   // by the cloud cover, % (read off his scores: one curve for every city, 100 to 12% cloud, 50 at 60%, 0 at 100%)
        static const Curve rainCurve{{0, 1.0}, {4, 1.0}, {6, 0.90}, {8, 0.72}, {10, 0.55}, {13, 0.33}, {15, 0.23}, {18, 0.13}, {22, 0.05}, {28, 0.0}};                           // by the chance of rain, %
        static const Curve dewCurve{{0, 0.5}, {30, 0.5}, {40, 0.90}, {50, 1.0}, {60, 1.0}, {61, 0.939}, {62, 0.884}, {63, 0.824}, {64, 0.765}, {65, 0.708}, {66, 0.637}, {67, 0.563}, {68, 0.5}, {69, 0.423}, {70, 0.351}, {71, 0.306}, {72, 0.251}, {73, 0.194}, {74, 0.171}, {80, 0.05}, {84, 0.0}};   // by the dewpoint, F (read off his scores: nothing under 30 F is 50, 90 at 40, 100 from 50 to 60, then 6 points a degree to 50 at 68; the same at every city)
        static const Curve sustainedCurve{{0, 1.0}, {8, 1.0}, {9, 0.981}, {10, 0.959}, {11, 0.939}, {12, 0.917}, {13, 0.895}, {14, 0.875}, {15, 0.857}, {16, 0.80}, {17, 0.732}, {18, 0.684}, {19, 0.633}, {20, 0.582}, {21, 0.52}, {22, 0.454}, {23, 0.388}, {24, 0.329}, {25, 0.268}, {26, 0.20}, {27, 0.141}, {28, 0.065}, {29, 0.016}, {30, 0.0}};   // by the sustained wind, mph (read off his scores)
        static const Curve gustCurve{{0, 1.0}, {19.9, 1.0}, {20.5, 0.78}, {29.9, 0.78}, {30.6, 0.565}, {60, 0.545}};   // times, by the gust, mph: his two steps, the gust factor 0.78 from 20 mph and 0.57 from 31
        static const Curve coldCurve{{0, 0.0}, {10, 0.06}, {20, 0.17}, {23, 0.23}, {29, 0.375}, {35, 0.51}, {41, 0.68}, {46, 0.84}, {50, 0.91}, {56.5, 1.0}};   // by the apparent temperature of a cold day, F (his: 23 F is 23 of 100, 35 F 51, 41 F 68, 47 F 83, whatever is normal for the place)
        // the cold side, relative: his score against his high under the normal high (the 5 km NCEI normal is his), where the cold of the day itself does not bind (the south: 82 of 100 at 8 F under, 56 at 14, 32 at 20)
        static const Curve coldBelowCurve{{-40, 0.0}, {-26, 0.08}, {-22.5, 0.22}, {-19.5, 0.32}, {-16.5, 0.44}, {-13.5, 0.56}, {-10.5, 0.70}, {-7.5, 0.82}, {-4.5, 0.93}, {-2, 1.0}};
        static const Curve warmCurve{{-20, 0.94}, {1, 0.92}, {4, 0.88}, {7, 0.84}, {7.3, 0.56}, {10, 0.47}, {13, 0.36}, {16, 0.20}, {22, 0.0}};   // by the day's high over the normal high, F (his, in the flat states: 90 of 100 at the normal, 84 at 7 F over, then a step to 56 within a third of a degree, 47 at 10 over and 20 at 16)
        // his normal high less ours (degrees F), what his temperature scores imply at 14,000 of his cities, interpolated to a 1 degree grid (latitude 24 to 50 north, longitude 125 to 66 west): his normals run
        // 3 F over ours on the whole and differ by state (5 to 6 in the south east, 1 to 2 in the plains, -3 in Minnesota); cities held out are placed to 1.4 F by their neighbours' offsets
        static const float normalOffset[27][60] = {
            {-4.0, -4.0, -4.0, -4.2, -4.2, -4.2, -4.1, -4.2, -4.2, 0.2, -1.1, -4.5, -4.4, -4.4, -4.3, -3.7, -3.4, -2.2, -1.6, 0.7, 2.6, 2.8, 2.8, 2.8, 2.8, 2.8, 2.3, 0.3, 0.7, 2.3, 3.2, 3.3, 3.3, 3.3, 10.8, 7.9, 6.7, -1.2, -7.3, -8.0, -8.7, -9.3, -9.8, -10.0, -9.8, -8.8, -7.9, -7.8, -7.8, -7.8, -7.8, -7.8, -7.9, -7.9, -7.9, -7.9, -7.9, -7.9, -7.9, -7.9},
            {-2.5, -4.0, -4.0, -4.2, -4.2, -4.2, -4.1, -4.1, -4.2, 1.1, 0.3, -4.5, -4.3, -4.4, -4.3, -4.3, -3.4, -2.2, -1.5, -1.2, 0.8, 2.7, 2.8, 2.8, 2.8, 2.8, 2.8, 0.3, 2.0, 3.3, 3.4, 3.4, 3.4, 12.8, 10.9, 7.9, 7.7, 5.0, -6.2, -6.6, -6.8, -7.6, -9.0, -9.5, -9.3, -7.4, -7.4, -7.6, -7.7, -7.7, -7.8, -7.8, -7.8, -7.8, -7.5, -7.5, -7.5, -7.5, -7.5, -6.2},
            {2.0, -3.2, -4.0, -4.0, -4.2, -4.2, -4.2, -4.1, -4.2, 1.1, 0.8, -4.2, -4.5, -4.4, -4.3, -4.2, -3.5, -2.1, -1.5, -1.5, 0.2, 1.3, 2.7, 2.8, 2.8, 2.8, 2.8, 0.3, 5.1, 3.7, 3.5, 3.4, 0.0, 11.4, 12.6, 7.9, 7.6, 6.1, -1.5, -5.1, -6.1, -5.9, -5.1, -4.2, -5.1, -5.6, -6.7, -7.4, -7.2, -7.3, -7.4, -7.4, -7.4, -7.4, -7.4, -7.4, -6.9, -5.7, -5.3, -4.8},
            {2.0, 0.7, -4.0, -4.0, -4.2, -4.2, -4.2, -4.1, -4.2, 0.2, 2.1, -2.9, -4.5, -4.4, -4.3, -4.2, -4.0, -1.8, -1.4, -1.4, -1.0, 0.7, 1.8, 2.8, 2.6, 2.8, 2.8, 1.9, 4.9, 8.0, 0.6, -1.5, 1.0, 8.6, 12.9, 8.0, 7.3, 4.9, -1.5, -2.0, -4.7, -4.9, -4.3, -4.4, -4.1, -4.7, -5.8, -5.9, -5.9, -6.0, -6.0, -6.0, -6.0, -6.0, -6.0, -5.9, -4.8, -4.6, -4.5, -4.5},
            {2.5, 2.2, -2.0, -4.0, -4.0, -4.2, -4.2, -4.1, -4.2, 0.2, 4.8, -1.1, -4.2, -4.7, -4.3, -4.0, -4.0, -1.7, -1.2, -1.3, -1.1, -0.0, 1.3, 2.5, 2.6, 2.2, 2.2, 2.1, 3.6, 3.2, -3.3, 0.9, 0.7, 6.0, 14.3, 8.1, 4.6, 2.6, 1.5, -1.4, -3.6, -5.2, -3.4, -2.0, -4.1, -4.4, -4.1, -4.1, -4.6, -4.6, -4.6, -4.9, -4.9, -5.2, -4.5, -4.5, -4.5, -4.5, -4.5, -4.5},
            {2.5, 2.1, 1.9, -3.0, -4.0, -4.2, -4.2, -4.2, -4.2, -0.7, 4.8, 0.8, -3.5, -4.5, -4.3, -4.1, -4.0, -1.8, -1.1, -1.3, -0.5, -0.2, 0.0, 2.0, 2.8, 2.3, 1.0, 0.9, 1.5, -2.5, -1.3, 3.2, -0.7, 1.5, 12.3, 8.5, 2.0, 2.4, 2.7, -0.6, -3.4, -4.3, -2.8, -0.2, -3.4, -4.0, -4.0, -4.0, -4.0, -4.0, -4.0, -4.2, -4.3, -4.5, -4.5, -4.5, -4.5, -4.5, -4.5, -4.7},
            {2.4, 2.5, 2.1, 2.1, -3.4, -4.0, -4.2, -4.2, -4.2, -0.7, 4.7, 3.9, -2.9, -4.2, -4.6, -4.1, -4.0, -1.6, -1.4, -1.4, -1.2, -1.7, -1.4, -0.3, 1.8, 0.4, -0.4, 1.6, 1.8, 1.9, 1.0, -1.2, 0.2, -0.5, 2.3, 2.9, 4.2, 3.3, 3.0, 1.2, -3.0, -1.3, -1.4, 1.6, 0.5, -0.4, -2.6, -3.3, -3.7, -2.6, -3.2, -4.0, -4.5, -4.5, -4.5, -4.5, -4.5, -4.7, -4.6, -4.6},
            {2.3, 2.4, 2.5, 2.1, 1.0, -3.8, -4.0, -4.2, -4.1, 0.4, 4.7, 4.6, -1.8, -3.3, -4.9, -4.1, -4.0, -1.3, -0.9, -1.5, -2.6, -1.0, -0.2, 0.2, 1.7, 3.0, 2.2, 3.3, 4.2, 3.3, 2.9, 2.4, 1.8, 3.0, 2.7, 3.1, 4.4, 5.2, 4.8, 4.5, 5.1, 4.1, 3.8, 1.7, 0.8, 0.5, -0.3, -0.4, -1.4, -3.5, -3.8, -4.2, -4.5, -4.5, -4.5, -4.5, -4.7, -4.6, -4.6, -4.5},
            {2.6, 2.3, 2.4, 2.5, 2.2, -0.1, -3.5, -4.2, -4.2, -0.3, 4.7, 5.1, 1.6, -1.1, -0.2, -3.4, -3.1, -2.7, -3.3, -2.1, -3.5, 0.3, 1.4, 2.1, 3.2, 4.3, 4.7, 5.3, 4.4, 5.5, 3.4, 3.3, 4.1, 6.1, 5.5, 3.8, 4.3, 6.1, 6.6, 5.2, 7.8, 8.0, 7.0, 5.2, 0.6, 0.1, -0.3, -1.1, -3.5, -3.2, -4.1, -4.5, -4.5, -4.5, -4.5, -4.6, -4.6, -4.5, -4.5, -4.5},
            {2.0, 2.5, 2.3, 2.4, 2.6, 2.0, -0.6, 1.6, -5.4, 1.3, 4.2, 5.2, 3.9, 4.2, 3.9, 0.5, 0.1, -6.2, -3.8, -4.0, 0.2, 2.5, 0.7, 1.8, 3.1, 4.3, 5.0, 4.6, 4.0, 3.5, 3.5, 3.8, 4.7, 6.0, 6.2, 5.2, 5.5, 7.0, 4.5, 6.3, 6.7, 7.5, 8.0, 5.8, 3.1, 2.9, -0.9, -3.6, -3.2, -4.0, -4.2, -4.5, -4.5, -4.7, -4.6, -4.5, -4.5, -4.4, -4.4, -4.4},
            {0.8, 1.8, 2.1, 2.6, 2.2, 2.9, 2.0, -1.0, -1.4, -2.2, 5.5, 4.8, -0.5, 2.1, -2.8, -5.0, -5.5, -1.9, -1.4, -2.4, 1.1, 2.1, 0.7, 0.9, 1.8, 3.2, 3.6, 3.9, 3.0, 3.8, 3.7, 4.1, 5.3, 7.1, 6.1, 5.9, 5.8, 5.7, 4.7, 3.7, 6.7, 7.5, 6.3, 6.6, 5.6, 4.8, -0.7, -3.4, 0.0, -4.2, -4.4, -4.6, -4.6, -4.6, -4.4, -4.4, -4.4, -4.4, -4.4, -5.1},
            {0.8, 1.0, 0.9, 2.4, 3.1, 2.2, -0.9, 1.1, -0.9, 0.9, 8.0, 2.6, -5.1, -3.1, -4.9, -3.5, -1.7, 0.1, -0.2, -0.8, 2.5, 3.5, 1.4, 1.1, 1.1, 1.8, 1.2, 1.4, 2.0, 4.3, 4.7, 4.1, 4.8, 6.4, 7.2, 5.7, 11.1, 7.5, 13.8, 9.8, 8.2, 2.0, 4.9, 7.7, 6.4, 7.0, 5.3, 2.5, 2.6, -1.1, -4.5, -4.6, -4.5, -4.5, -4.4, -4.4, -4.4, -5.0, -5.1, -5.3},
            {0.7, 0.9, 1.1, 0.9, 2.5, 6.1, 7.5, 2.7, 8.5, 5.6, 9.7, 5.2, -2.0, -2.7, -3.1, -1.3, -1.0, 0.4, 0.3, -1.8, -1.2, 3.5, 2.4, 2.3, 2.0, 1.3, 1.4, 1.7, 2.1, 3.6, 4.8, 4.0, 3.3, 4.3, 6.1, 6.7, 5.3, 6.5, 14.5, 8.4, 3.6, 6.7, 5.5, 3.3, 5.4, 8.3, 6.6, 6.0, 4.0, -2.8, -4.6, -4.6, -4.5, -4.5, -4.5, -4.5, -4.6, -5.0, -5.0, -5.8},
            {0.5, 1.6, 1.1, 2.8, 6.7, 7.0, 8.2, 1.5, 7.2, 7.1, 7.4, 11.3, -0.1, -1.8, -3.0, -0.1, 2.6, 2.2, 0.5, -3.2, 2.1, 3.4, 2.2, 1.2, 1.0, 1.1, 1.5, 2.0, 2.1, 2.5, 2.8, 2.9, 2.9, 2.9, 3.6, 4.7, 5.4, 3.8, 4.4, 4.3, 6.1, 5.5, 5.4, 4.4, 2.5, 6.4, 8.2, 5.6, 6.0, 3.6, 0.8, -0.1, 0.5, 1.0, 5.2, 10.3, -0.2, -5.0, -5.3, -5.9},
            {-0.4, -0.3, 1.4, 5.4, 8.6, 4.4, -5.4, 1.1, 2.9, 3.4, 2.4, 1.0, -3.4, -4.6, -3.4, 1.6, 1.4, 2.6, -1.5, -1.2, 5.9, 5.0, 3.3, 1.3, 0.9, -0.0, 1.0, 1.9, 1.8, 2.0, 1.9, 2.3, 4.0, 3.2, 4.0, 4.5, 4.8, 4.9, 5.1, 4.5, 4.7, 6.9, 6.8, 6.9, 4.8, 2.9, 5.1, 8.1, 6.4, 10.0, 0.7, 1.1, 6.7, 13.9, 12.4, 0.4, -2.9, -5.0, -5.9, -5.9},
            {0.5, 1.0, 3.7, 8.9, 8.1, -2.6, 4.3, 4.7, 4.1, 2.0, -0.2, 2.4, -0.4, -0.9, 0.2, 2.9, 6.8, 7.8, 2.8, -0.4, 5.4, 4.1, 2.1, 1.1, -0.2, -0.4, 0.7, 0.9, 0.9, 1.7, 1.9, 1.5, 1.8, 1.9, 4.3, 4.2, 3.6, 2.3, 3.3, 3.2, 3.1, 3.8, 4.5, 5.9, 7.2, 3.5, 5.4, 6.1, 6.5, 15.5, 15.5, 15.5, 11.0, 5.7, -0.5, -0.4, -5.0, -5.9, -5.9, -5.8},
            {-0.4, -0.3, 3.6, 9.8, 2.9, 4.2, 4.6, 5.9, 4.6, 2.4, 1.2, 1.8, 1.7, 1.5, 0.6, 1.6, 4.5, 5.0, 2.7, -0.3, 9.9, 7.5, 2.3, 1.8, 0.6, -1.4, -1.1, -0.2, -0.4, 0.0, 1.0, 0.5, 0.2, 0.3, 1.7, 2.9, 1.4, 1.3, 1.5, 1.7, 1.3, 2.3, 3.0, 3.5, 3.9, 7.0, 3.8, 5.4, 5.4, 1.9, 7.4, 0.2, -2.4, -2.8, 3.4, -4.6, -5.8, -5.8, -5.8, -5.7},
            {1.7, 5.3, 3.7, 6.6, 3.0, 3.3, 5.1, 6.0, 5.4, 3.7, 0.5, 2.5, 2.0, 3.5, -0.5, 0.9, 2.2, 3.9, 2.2, 2.0, 5.5, 5.9, 1.2, 0.5, -0.2, -1.9, -1.4, -1.3, -1.4, -1.2, -1.2, -1.1, -0.7, -0.6, 1.2, 1.9, 0.9, 1.0, 0.9, 0.4, 1.2, 1.4, 1.6, 0.2, 1.1, 3.9, 5.6, 3.5, 3.2, 1.3, 3.3, 1.0, 1.1, 6.4, -1.1, -5.1, -5.8, -5.7, -5.6, -5.6},
            {-0.2, -0.5, 4.3, 1.5, 3.3, 4.3, 5.9, 6.5, 6.4, 5.2, 5.0, 4.3, 4.0, 1.3, 0.0, 0.6, 2.3, 2.9, 3.0, 3.2, 5.2, 3.6, 1.4, -1.5, -1.0, -0.8, -1.5, -2.1, -1.5, -1.6, -2.5, -1.0, -1.7, -0.6, 0.0, 0.5, 1.0, 1.0, 1.0, 0.9, 0.9, 1.4, 0.2, 1.8, 3.8, -1.2, -2.9, -2.7, 3.2, 1.5, 0.3, 1.1, -5.0, -4.8, -2.4, -4.6, -5.0, -5.1, -5.1, -5.1},
            {-0.5, 0.6, 4.9, 2.6, 1.1, 3.3, 6.7, 7.0, 8.1, 8.6, 7.1, 4.9, 5.3, 4.6, 3.0, 2.0, 3.0, 3.5, 4.5, 4.6, 4.2, 1.0, 0.3, -0.1, -0.6, -0.7, -0.8, -1.7, -2.0, -2.5, -3.2, -2.4, -2.2, -1.6, -0.3, -0.0, 1.0, 1.0, 1.0, 0.7, 1.4, 1.0, 0.3, 0.5, 1.7, -1.7, -2.3, -2.5, -2.5, -0.7, -4.4, -4.3, -4.5, -5.7, -4.6, -5.2, -4.8, -4.6, -4.8, -4.8},
            {-1.4, -0.8, 4.0, -0.4, -0.1, 3.1, 4.2, 5.4, 7.0, 6.5, 4.6, 4.1, 4.8, 5.0, 5.2, 4.7, 4.0, 2.9, 2.7, 2.4, 0.1, -0.9, 1.9, 0.9, 0.1, 0.0, -0.4, -1.9, -3.0, -3.2, -3.0, -2.7, -2.8, -2.6, -2.0, -0.4, 1.0, 1.0, 1.0, 0.9, 1.1, 0.5, 0.3, 0.3, -1.6, -1.7, -1.8, -2.4, -2.5, -4.4, -4.4, -4.4, -4.4, -4.9, -4.6, -4.8, -5.1, -5.1, -4.8, -4.6},
            {-2.9, -2.6, 3.5, 1.9, 2.6, 3.5, 4.2, 1.9, 0.9, 2.7, 3.8, 4.3, 6.1, 6.9, 6.5, 6.5, 6.2, 4.0, 2.4, 2.1, 0.5, 0.2, 0.4, -0.8, -1.7, -2.3, -1.5, -2.6, -3.2, -2.5, -2.7, -2.7, -2.7, -2.7, -2.7, -2.3, 1.0, 1.0, 1.0, 1.1, 0.6, 0.5, 0.3, 0.3, -1.6, -1.7, -1.7, -2.3, -2.4, -4.4, -4.4, -4.4, -4.4, -4.3, -4.6, -4.8, -5.1, -5.1, -5.1, -5.0},
            {-5.3, -5.1, 0.2, 2.0, 1.8, 3.6, 4.9, 6.0, 4.8, 5.8, 5.1, 4.5, 5.7, 6.0, 6.1, 6.3, 6.5, 5.3, 2.9, 2.4, 2.7, 1.1, -0.3, -0.8, -1.1, -1.0, -1.6, -1.8, -2.4, -2.7, -2.6, -2.6, -2.7, -2.7, -2.7, -2.7, -2.4, 1.0, 1.0, 0.7, 0.6, 0.5, 0.4, -0.2, -1.7, -1.7, -1.7, -2.3, -2.6, -4.4, -4.4, -4.4, -4.4, -4.3, -4.6, -4.8, -4.8, -5.1, -5.1, -5.1},
            {-4.6, -4.5, -0.9, 1.0, -1.6, -0.2, 1.5, 1.6, 1.6, 1.3, 2.7, 3.8, 4.7, 5.0, 4.5, 3.0, 4.9, 3.7, 1.2, 1.8, 4.6, 1.6, -0.3, -1.2, -1.1, -1.1, -1.3, -1.4, -2.0, -2.4, -2.6, -2.6, -2.6, -2.6, -2.7, -2.7, -2.6, -1.6, 1.1, 0.6, 0.5, 0.5, 0.4, -1.0, -1.7, -1.7, -1.7, -1.7, -2.8, -4.4, -4.4, -4.4, -4.4, -4.3, -4.6, -4.7, -4.8, -4.8, -5.1, -5.1},
            {-3.6, -2.9, -0.5, 2.1, -0.4, -2.8, -0.1, 0.2, 2.4, 1.4, 1.2, 3.6, 4.4, 6.0, 4.2, 3.3, 3.2, 2.0, 1.8, 2.1, 2.8, 0.9, 0.1, -0.4, -0.8, -1.0, -1.2, -1.3, -1.5, -2.3, -2.5, -2.6, -2.6, -2.6, -2.6, -2.7, -2.6, -2.6, -0.7, 0.6, 0.5, 0.5, 0.4, -1.2, -1.7, -1.7, -1.7, -1.7, -3.0, -4.4, -4.4, -4.4, -4.4, -4.2, -4.5, -4.7, -4.8, -4.8, -4.8, -5.1},
            {-0.5, 2.8, 2.5, 3.0, 2.1, -0.2, -0.1, 0.7, 2.5, 2.0, 2.1, 3.9, 4.7, 4.7, 3.8, 3.6, 3.3, 2.1, 1.9, 2.2, 2.1, 1.0, 0.7, 0.3, -0.2, -0.7, -0.9, -1.2, -1.3, -1.7, -2.3, -2.5, -2.6, -2.6, -2.6, -2.6, -2.6, -2.7, -2.6, 0.6, 0.5, 0.5, 0.4, -1.7, -1.7, -1.7, -1.7, -2.4, -3.2, -4.4, -4.4, -4.4, -4.4, -4.3, -4.5, -4.7, -4.8, -4.8, -4.8, -4.8},
            {3.1, 4.0, 3.5, 3.6, 3.5, 2.5, 0.7, 0.7, 2.1, 2.5, 2.7, 4.1, 4.6, 4.5, 4.4, 4.1, 3.9, 2.6, 1.6, 1.7, 2.1, 1.0, 0.8, 0.6, 0.3, -0.2, -0.7, -0.8, -1.0, -1.3, -1.8, -2.1, -2.5, -2.6, -2.6, -2.6, -2.6, -2.6, -2.6, -2.0, 0.5, 0.4, -0.5, -1.7, -1.7, -1.7, -1.7, -2.6, -3.5, -4.4, -4.4, -4.4, -4.4, -4.5, -4.4, -4.6, -4.7, -4.8, -4.8, -4.8}};
        const auto normalOffsetAt = [] (double lon, double lat) {
            const double fy = std::clamp(lat - 24.0, 0.0, 25.999), fx = std::clamp(lon + 125.0, 0.0, 58.999);
            const int y = static_cast<int>(fy), x = static_cast<int>(fx);
            const double ty = fy - y, tx = fx - x;
            return std::clamp((1.0 - ty) * ((1.0 - tx) * normalOffset[y][x] + tx * normalOffset[y][x + 1]) + ty * ((1.0 - tx) * normalOffset[y + 1][x] + tx * normalOffset[y + 1][x + 1]), -6.0, 8.0);
        };
        // the blend's values on his scale: what his weather is on average where the blend has the value (42,000 city days; his sources differ from the blend's: his cloud is 20 points over at 8%, his wind 6 mph over in calm,
        // the gusts flatten above 20 mph)
        static const Curve cloudToHis{{0, 3}, {2.5, 6.2}, {7.5, 20.8}, {12.5, 28}, {17.5, 33.2}, {25, 39.8}, {35, 49.1}, {45, 51.6}, {55, 64.7}, {65, 72.7}, {75, 83.2}, {85, 91.6}, {95, 95.6}, {100, 100}};
        static const Curve windToHis{{0, 6.5}, {1, 7.1}, {2.5, 8.2}, {3.5, 8.8}, {4.5, 9.9}, {5.5, 11.3}, {7, 12.4}, {9, 14.5}, {11, 16.7}, {13.5, 18.3}, {17.5, 21.9}, {25, 28}, {35, 36}};
        static const Curve gustToHis{{0, 11}, {4, 12.7}, {10, 14.5}, {14, 19.6}, {18, 25.6}, {22, 28.3}, {26, 29.8}, {30, 31.2}, {36, 35.8}, {60, 60}};
        static const Curve heatCurve{{0, 1.0}, {85, 1.0}, {86, 0.963}, {87, 0.925}, {88, 0.883}, {89, 0.841}, {90, 0.805}, {91, 0.759}, {95, 0.55}, {100, 0.2}, {105, 0.0}};   // by the day's high (feels like), F (his: full marks to 85, then 4 points a degree to 76 at 91; above that is a guess: his week had none)
        // the climate adjusted version: the normal of the place for the day (the 1991-2020 daily mean near the surface, 2.5 degrees: its high taken as 9 F above it) moves the comfortable
        // range up half way where the normal high is above its middle (an 85 F normal high: 64 + 10.5, 59 to 74 F becomes 67 to 82 F); a cold normal does not move it down
        GfsGrid::Grid normal;
        bool haveNormal = false;
        // the contiguous states have NOAA's own normal high of the day, 5 km (GfsNormals: 15 small files the first time, 2.6 MB kept): his normal, to within a degree or two. The reanalysis one
        // (2.5 degrees, its mean taken 9 F under the high, 3 F cold on the whole) stays for the rest of the world and for a place the 5 km grid does not hold
        GfsNormals::Day normalDay;
        bool haveNcei = false;
        {
            auto t = QDateTime::fromString(QString::fromStdString(context.run.id()), "yyyyMMddHH");
            t.setTimeZone(QTimeZone::utc());
            t = t.addSecs(context.hour * 3600);
            std::string why;
            haveNcei = GfsNormals::shared().prepare(GfsNormals::Kind::High, why);
            normalDay = GfsNormals::day(t.date().month(), t.date().day());
            if (context.climate) {
                haveNormal = context.climate->at(GfsClimate::surfaceTemperature(), GfsClimate::dayIndex(t.date().year(), t.date().month(), t.date().day()), normal, why);
            }
        }
        // the ingredients of each cell, which are smoothed over a radius of 50 miles before the levels are made of them (a map of the weather people can plan by, not of every ridge and valley)
        const float none = std::numeric_limits<float>::quiet_NaN();
        const size_t cells = shape.values.size();
        std::vector<float> cHt(cells, none), cT(cells, none), cR(cells, none), cD(cells, none), cC(cells, none), cW(cells, none), cA(cells, none), cS(cells, none), cG(cells, none);
        for (int row = 0; row < shape.rows; row++) {
            const double lat = shape.lat0 - row * shape.step;
            for (int column = 0; column < shape.columns; column++) {
                const size_t at = static_cast<size_t>(row) * static_cast<size_t>(shape.columns) + static_cast<size_t>(column);
                double lon = shape.lon0 + column * shape.step;
                if (lon > 180.0) {
                    lon -= 360.0;
                }
                const double zone = lon / 15.0;   // hours of the sun's time ahead of UTC
                double normalHigh = std::numeric_limits<double>::quiet_NaN();   // the normal high of the day at the place (degrees F), where the climate adjustment has it
                double middle = 64.0;   // the middle of the comfortable range, degrees F (56.5 to 71.5): the weather of the day itself, whatever is normal for the place
                if (haveNcei) {
                    const double ncei = GfsNormals::shared().at(GfsNormals::Kind::High, normalDay, lon, lat);
                    if (std::isfinite(ncei)) {
                        normalHigh = ncei;
                        middle = std::max(64.0, normalHigh - 6.0);
                    }
                }
                if (std::isnan(normalHigh) && haveNormal) {
                    double meanK = normal.sample(lon < 0.0 ? lon + 360.0 : lon, lat);
                    if (!std::isnan(meanK)) {
                        meanK = meanK > 150.0 ? meanK - 273.15 : meanK;
                        normalHigh = meanK * 1.8 + 32.0 + 9.0 + normalOffsetAt(lon, lat);   // (his normals: ours + 9 and his offset at the place)
                        middle = std::max(64.0, normalHigh - 6.0);   // (the hours' range moves with the normal: at a 70 F normal high it is the fixed range)
                    }
                }
                const double nowLocal = runHours + zone;
                const double hourNow = nowLocal - 24.0 * std::floor(nowLocal / 24.0);
                const double target = std::floor(nowLocal / 24.0) + (hourNow <= 9.5 ? 0 : 1) + (day - 1);
                double wWorst = 1.0, tScore = 0, dScore = 0, cScore = 0, wScore = 0, pMax = 0, pSum = 0, smoke = 0, smokeWeight = 0, aSum = 0, high = -1000.0, gustMax = 0;
                double count = 0;   // the weight of the hours (an hour counts fully from 10 to 18 o'clock and less towards 8 and 20, so that the cells where the hours fall differently do not make a line)
                for (const int f : hours) {
                    const double local = runHours + f + zone;
                    const double dayIndex = std::floor(local / 24.0), hourOfDay = local - 24.0 * dayIndex;
                    if (dayIndex != target || hourOfDay <= 8.0 || hourOfDay >= 20.0) {
                        continue;
                    }
                    const double weight = std::min({1.0, (hourOfDay - 8.0) / 2.0, (20.0 - hourOfDay) / 2.0});
                    const std::string name = std::to_string(f);
                    double apparent = g["a" + name].values[at], dewC = g["d" + name].values[at];
                    // the blend's inputs on the scale his scores read them (his cloud cover is 11 points above the blend's, his wind 5 mph, his gusts 4 mph; fitted over 8,000 city days: correlations 0.86 to 0.92)
                    const double cloud = along(cloudToHis, g["c" + name].values[at]), gust = along(gustToHis, g["g" + name].values[at] * 2.23694), sustained = along(windToHis, g["u" + name].values[at] * 2.23694);
                    double rain = g["p" + name].values[at];
                    {   // the fine particles (micrograms a cubic meter): the RRFS's (where it reaches) and GEOS-FP's (the dust, sea salt, black and organic carbon and sulfate), the larger where both have them
                        double fine = std::numeric_limits<double>::quiet_NaN();
                        if (const auto found = g.find("s" + name); found != g.end() && !found->second.values.empty() && !std::isnan(found->second.values[at])) {
                            fine = found->second.values[at] * 1.0e9;
                        }
                        if (const auto found = g.find("y" + name); found != g.end() && !found->second.values.empty() && !std::isnan(found->second.values[at])) {   // GEOS-FP's own PM2.5 (kg a cubic meter)
                            const double geos = found->second.values[at] * 1.0e9;
                            fine = std::isnan(fine) ? geos : std::max(fine, geos);
                        }
                        if (!std::isnan(fine)) {
                            smoke += weight * fine;
                            smokeWeight += weight;
                        }
                    }
                    apparent = apparent * 1.8 + 32.0;
                    const double dew = dewC * 1.8 + 32.0;
                    if (std::isnan(apparent) || std::isnan(dew) || std::isnan(cloud) || std::isnan(gust) || std::isnan(sustained) || std::isnan(rain)) {
                        continue;
                    }
                    const double outside = std::max(0.0, std::max(middle - 7.5 - apparent, apparent - (middle + 12.0)));   // degrees F outside the comfortable range (a warm day is fine to 76 F: his Tucson, 74 F, is Beautiful)
                    aSum += weight * apparent;
                    if (weight >= 0.5) {
                        gustMax = std::max(gustMax, gust);
                    }
                    tScore += weight * (apparent >= 95.0 ? 0.0 : apparent < middle - 7.5 ? along(coldCurve, apparent + 4.0) : ramp(outside, 0.0, 14.0));   // (dangerous heat is never nice)
                    dScore += weight * along(dewCurve, dew);   // (a little under full marks for very dry air: about 92 at a 44 F dewpoint)
                    if (weight >= 0.5) {
                        high = std::max(high, apparent);
                    }
                    cScore += weight * along(skyCurve, cloud);   // (his sky: 5% cloud 98.9, 36% cloud 70.4; the power curve through both, nothing at an overcast)
                    const double windNow = along(sustainedCurve, sustained) * along(gustCurve, gust);
                    wScore += weight * windNow;
                    if (weight >= 0.5) {
                        wWorst = std::min(wWorst, windNow);   // (the windiest hour counts as much as the day's mean: a breezy afternoon is what people feel)
                    }   // (his Topeka, 12 mph, is Fair; a 7.8 mph day with 12.7 mph gusts scores 96.7: the sustained wind counts from 8 mph, nothing at 26)
                    pMax = std::max(pMax, weight * rain);
                    pSum += weight * rain;
                    count += weight;
                }
                if (count < 0.5) {
                    continue;
                }
                const double rain = std::min(100.0, 1.5 * pSum / count + 1.5);   // (his chance of rain is 1.5 times the blend's mean hourly chance of the daytime hours: correlation 0.93)
                // the temperature: half the hours' score (the range) and half the day's high (full marks from 70 F, 90 at 66 F, 70 at 62.6 F, nothing under 56 F; a loss over 80)
                const double rawHigh = high;
                const double highShift = middle - 64.0;   // (the day's high is judged by the same range, moved with it)
                high -= highShift;
                // (his day high against the normal: Dayton, 59 F with a 57 F normal, is 92 of 100; a high 12 F over the normal scores 70, 4 F over 90: 2.4 points a degree over, half that under)
                const double highScore = !std::isnan(normalHigh) ? (rawHigh < -500.0 || rawHigh >= 95.0 ? 0.0 : std::min({rawHigh - 5.5 < 56.5 ? along(coldCurve, rawHigh - 5.5) : 1.0, along(warmCurve, rawHigh - 1.5 - normalHigh), along(coldBelowCurve, rawHigh - 1.5 - normalHigh)})) : high < -500.0 ? 0.0 : high >= 95.0 ? 0.0 : high > 80.0 ? 1.0 - 0.05 * (high - 80.0) : high >= 70.0 ? 1.0 : std::max(0.0, 1.0 - 0.0054 * (70.0 - high) * (70.0 - high));   // (our apparent high is 5.5 F over his apparent and 1.5 F over his 2 m high; his normals are in the offset map)
                cT[at] = static_cast<float>(0.25 * tScore / count + 0.75 * highScore);
                cD[at] = static_cast<float>(dScore / count);
                cC[at] = static_cast<float>(cScore / count);
                cW[at] = static_cast<float>(0.5 * wScore / count + 0.5 * wWorst);
                cR[at] = static_cast<float>(along(rainCurve, rain));
                cHt[at] = static_cast<float>(along(heatCurve, rawHigh));   // (a moderate chance pulls the score down a good bit)
                cA[at] = static_cast<float>(aSum / count);   // (the top levels are for a day that is warm in itself)
                cS[at] = static_cast<float>(smokeWeight >= 0.5 ? smoke / smokeWeight : 0.0);   // (no particles forecast, no cap)
                cG[at] = gustMax > 35.0 ? 1.0f : 0.0f;   // a gust over 35 mph at some hour: the share of the area around that has one is what caps
            }
        }
        {   // the smoothing: the mean of the cells within 50 miles (a box: the width in cells follows the latitude), where there is a value
            const double radiusKm = 80.5;
            const int rows = shape.rows, columns = shape.columns;
            const int ry = std::max(1, static_cast<int>(std::lround(radiusKm / (111.32 * shape.step))));
            for (auto * v : {&cT, &cR, &cD, &cC, &cW, &cA, &cS, &cG, &cHt}) {
                std::vector<float> across(cells, none);
                std::vector<double> sum(static_cast<size_t>(std::max(rows, columns)) + 1), number(static_cast<size_t>(std::max(rows, columns)) + 1);
                for (int row = 0; row < rows; row++) {
                    const double lat = shape.lat0 - row * shape.step;
                    const int rx = std::min(columns / 2, static_cast<int>(std::lround(ry / std::max(0.2, std::cos(lat * pi / 180.0)))));
                    for (int column = 0; column < columns; column++) {
                        const float value = (*v)[static_cast<size_t>(row) * static_cast<size_t>(columns) + static_cast<size_t>(column)];
                        sum[static_cast<size_t>(column) + 1] = sum[static_cast<size_t>(column)] + (std::isnan(value) ? 0.0 : value);
                        number[static_cast<size_t>(column) + 1] = number[static_cast<size_t>(column)] + (std::isnan(value) ? 0.0 : 1.0);
                    }
                    for (int column = 0; column < columns; column++) {
                        const int lo = std::max(0, column - rx), hi = std::min(columns - 1, column + rx);
                        const double n = number[static_cast<size_t>(hi) + 1] - number[static_cast<size_t>(lo)];
                        if (n > 0.0) {
                            across[static_cast<size_t>(row) * static_cast<size_t>(columns) + static_cast<size_t>(column)] = static_cast<float>((sum[static_cast<size_t>(hi) + 1] - sum[static_cast<size_t>(lo)]) / n);
                        }
                    }
                }
                std::vector<float> result(cells, none);
                for (int column = 0; column < columns; column++) {
                    for (int row = 0; row < rows; row++) {
                        const float value = across[static_cast<size_t>(row) * static_cast<size_t>(columns) + static_cast<size_t>(column)];
                        sum[static_cast<size_t>(row) + 1] = sum[static_cast<size_t>(row)] + (std::isnan(value) ? 0.0 : value);
                        number[static_cast<size_t>(row) + 1] = number[static_cast<size_t>(row)] + (std::isnan(value) ? 0.0 : 1.0);
                    }
                    for (int row = 0; row < rows; row++) {
                        const int lo = std::max(0, row - ry), hi = std::min(rows - 1, row + ry);
                        const double n = number[static_cast<size_t>(hi) + 1] - number[static_cast<size_t>(lo)];
                        const size_t at = static_cast<size_t>(row) * static_cast<size_t>(columns) + static_cast<size_t>(column);
                        if (n > 0.0 && !std::isnan((*v)[at])) {   // (not past the edge of the data)
                            result[at] = static_cast<float>((sum[static_cast<size_t>(hi) + 1] - sum[static_cast<size_t>(lo)]) / n);
                        }
                    }
                }
                *v = std::move(result);
            }
        }
        // AirNow's air quality (the reporting areas, the cities and their surroundings) for the day: the worst category within 50 miles caps a place (Good: nothing, but an index known to be over 35
        // is not for Perfect; Moderate: Beautiful; Unhealthy for Sensitive Groups: Fair; Unhealthy and worse: no level). Where the file has nothing for the day (the forecasts go about five days
        // out) there is no cap from it.
        std::vector<int> airCap(cells, 5);
        {
            const double midLocal = runHours - 6.3;   // the day's date by the sun's time at the middle of the country
            const double hourMid = midLocal - 24.0 * std::floor(midLocal / 24.0);
            const long dayCount = static_cast<long>(std::floor(midLocal / 24.0)) + (hourMid < 17.0 ? 0 : 1) + (day - 1);
            const auto points = AirNow::points(QDate{1970, 1, 1}.addDays(dayCount));
            const double radiusKm = 80.5;
            for (const auto& point : points) {
                const int cap = point.category >= 3 ? 0 : point.category == 2 ? 2 : point.category == 1 ? 4 : point.aqi > 35.0 ? 4 : 5;
                if (cap >= 5) {
                    continue;
                }
                const int rowCenter = static_cast<int>(std::lround((shape.lat0 - point.lat) / shape.step));
                const int spanRows = static_cast<int>(radiusKm / (111.32 * shape.step)) + 1;
                for (int row = std::max(0, rowCenter - spanRows); row <= std::min(shape.rows - 1, rowCenter + spanRows); row++) {
                    const double lat = shape.lat0 - row * shape.step;
                    const double perDegree = 111.32 * std::max(0.2, std::cos(lat * pi / 180.0));
                    double lonCell = point.lon - shape.lon0;
                    while (lonCell < 0.0) lonCell += 360.0;
                    const int columnCenter = static_cast<int>(std::lround(lonCell / shape.step));
                    const int spanColumns = static_cast<int>(radiusKm / (perDegree * shape.step)) + 1;
                    for (int column = std::max(0, columnCenter - spanColumns); column <= std::min(shape.columns - 1, columnCenter + spanColumns); column++) {
                        const double dy = (lat - point.lat) * 111.32, dx = (shape.lon0 + column * shape.step - (point.lon < 0 && shape.lon0 >= 0 ? point.lon + 360.0 : point.lon)) * perDegree;
                        if (dx * dx + dy * dy <= radiusKm * radiusKm) {
                            auto& value = airCap[static_cast<size_t>(row) * static_cast<size_t>(shape.columns) + static_cast<size_t>(column)];
                            value = std::min(value, cap);
                        }
                    }
                }
            }
        }
        // the base each level needs (his day 1 colored 10% of the area Beautiful, 17% Nice, 63% Fair and 10% Decent: Fair is the wide band, Nice and Beautiful are the narrow ones; fitted with the temperature judged against the normal)
        GfsGrid::Grid gate = level, gateValue = level;   // what sets each cell's level, and its score (for the hover)
        const double thP = 0.92, thB = 0.805, thN = 0.76, thF = 0.45, thD = 0.20;   // (his scale: Beautiful from about 82 of 100, as at Huntsville; the shares of his day 1: 10% Beautiful, 17% Nice, 60% Fair)
        for (size_t at = 0; at < cells; at++) {
            double lonAt = shape.lon0 + static_cast<double>(at % static_cast<size_t>(shape.columns)) * shape.step;
            if (lonAt > 180.0) {
                lonAt -= 360.0;
            }
            if (std::isnan(cT[at]) || !insideUnitedStates(lonAt, shape.lat0 - static_cast<double>(at / static_cast<size_t>(shape.columns)) * shape.step)) {   // (a map of the land of the United States)
                level.values[at] = none;
                gate.values[at] = none;
                gateValue.values[at] = none;
                continue;
            }
            const double tScore = cT[at], rScore = cR[at], dScore = cD[at], cScore = cC[at], wScore = cW[at], apparentMean = cA[at], smoke = cS[at], gusty = cG[at];
            // his score reads as 100 less a penalty for each ingredient, the temperature by far the heaviest (fitted to nine of his city scores: 90 points for a whole unit short on the temperature, 50 on the dewpoint,
            // 25 on the sky; rain and wind lighter, from his zone texts)
            const double heatScore = cHt[at];
            const double base = (0.35 * tScore + 0.25 * rScore + 0.06 * heatScore + 0.06 * dScore + 0.10 * cScore + 0.11 * wScore) / 0.93;
            // each ingredient allows a level up to the one its score reaches (the weakest of them rules: a level needs all of the ingredients at its mark, whatever the others are). The marks were
            // fitted to how the reference outlook's levels follow his ingredient scores (about 77% of 21,000 cities at the same level, against 69% for a cutoff on the weighted score)
            static const double marks[6][4] = {   // Decent, Fair, Nice, Beautiful (of 100): temperature, rain, heat, dewpoint, sky, wind
                {0, 18, 68, 91}, {0, 34, 89, 100}, {0, 50, 78, 100}, {17, 39, 67, 92}, {0, 25, 76, 89}, {0, 38, 75, 82}};   // (refitted on the blend's inputs on his scale, shares of the levels matched to his: 76% of his day 1 and 80% of his day 2 at the same level at 21,000 cities, 98% within one)
            const double scores[6] = {tScore * 100.0, rScore * 100.0, heatScore * 100.0, dScore * 100.0, cScore * 100.0, wScore * 100.0};
            // what sets the level, for the hover: the ingredient (0 temperature ... 5 wind) with the fewest marks reached, or the cap that took the level down last, or 10: nothing holds it back
            int value = 4;
            int weakest = 0;
            int weakestReached = 99;
            for (int ingredient = 0; ingredient < 6; ingredient++) {
                int reached = 0;
                for (int level = 0; level < 4; level++) {
                    reached += scores[ingredient] >= marks[ingredient][level] ? 1 : 0;
                }
                if (reached < weakestReached || (reached == weakestReached && scores[ingredient] < scores[weakest])) {
                    weakestReached = reached;
                    weakest = ingredient;
                }
                value = std::min(value, reached);
            }
            const int lowest = static_cast<int>(std::min_element(scores, scores + 6) - scores);   // the ingredient with the lowest score
            int gateCode = 10;
            double gateScore = std::numeric_limits<double>::quiet_NaN();
            if (value < 4) {
                gateCode = weakest;
                gateScore = scores[weakest];
            }
            if (value == 4) {
                if (base >= thP && scores[lowest] >= 96.0) {   // (Perfect: every ingredient all but flawless)
                    value = 5;
                } else {   // Beautiful, and the lowest ingredient is what keeps it from Perfect
                    gateCode = lowest;
                    gateScore = scores[lowest];
                }
            }
            const auto cap = [&] (int limit, int code) {
                if (limit < value) {
                    value = limit;
                    gateCode = code;
                    gateScore = std::numeric_limits<double>::quiet_NaN();
                }
            };
            cap(day <= 2 ? 5 : day == 3 ? 4 : 3, 6);   // the further the day, the lower the level the forecast can stand behind
            // the fine particles, the daytime mean (micrograms a cubic meter), by the EPA's air quality index: Perfect needs an index of 35 (8.4 micrograms), Beautiful about 55 (14), Nice 100 (35.4), Fair
            // 150 (55.4), and over that no level at all (Unhealthy)
            cap(smoke <= 8.4 ? 5 : smoke <= 14.0 ? 4 : smoke <= 35.4 ? 3 : smoke <= 55.4 ? 2 : 0, 7);
            cap(airCap[at], 8);
            if (wScore < 0.2 || gusty >= 0.05) {   // gusts over 35 mph in the area around (or a mean gust past about 31 mph) hold a place at Decent whatever its score
                cap(1, 9);
            }
            gate.values[at] = static_cast<float>(gateCode);
            gateValue.values[at] = static_cast<float>(gateScore);
            level.values[at] = static_cast<float>(value);
        }
        g["level"] = std::move(level);
        g["gate"] = std::move(gate);
        g["gatescore"] = std::move(gateValue);
    };
    // the smoke is the RRFS's (the blend has none): its near-surface fine particles at the same hours, for the first days it reaches
    x.extras.push_back({"RRFS", nbmWant("s", "MASSDEN", "8 m above ground", "", "aerosol=Total aerosol"), "RRFS smoke", 0, [sampleHours] (int hour) { return sampleHours(std::clamp(hour / 24, 1, 8)); }, 6});
    {   // GEOS-FP's fine particles at the same hours (the daytime of the days it has): the dust, sea salt, black carbon, organic matter and sulfate of its 3 hour surface averages
        const auto geosHours = [sampleHours] (int hour) { return sampleHours(std::clamp(hour / 24, 1, 8)); };
        x.extras.push_back({"GEOS-FP", GfsData::Want{"y", "PM25", "aerosol", "", ""}, "GEOS-FP particles", 0, geosHours, 0});
    }
    x.fill = [] (const Grids& g) { return pick(g, "level"); };
    x.hoverValues = {{"gate", "Limited by", {"Temperature", "Chance of rain", "Heat", "Dewpoint (humidity)", "Cloud cover (sky)", "Wind", "How far off the day is (a later day is held to a lower level)", "Smoke (fine particles)", "Air quality (AirNow)",
                                            "Gusts over 35 mph nearby", "Nothing: every ingredient allows the level"}},
                     {"gatescore", "Its score (of 100)", {}}};
    x.ramp = Ramp{{{1.0, QColor{"#d6e7a3"}}, {2.0, QColor{"#9fd486"}}, {3.0, QColor{"#4fb868"}}, {4.0, QColor{"#1a8c9a"}}, {5.0, QColor{"#f5b800"}}}, true};   // each level starts at its whole number; the scale ends at 5
    x.fillTitle = "Nice weather in the daytime: 1 Decent, 2 Fair, 3 Nice, 4 Beautiful, Perfect 5/5";
    x.legendStep = 1;
    p.push_back(x);
}
// ---- the RTMA and URMA surface analyses (2.5 km, the hour of the analysis only): the blend's surface charts made from the analysis's own fields
for (const char * source : {"RTMA", "URMA", "RTMA-AK", "URMA-AK", "RTMA-HI", "URMA-HI", "RTMA-PR", "URMA-PR", "RTMA-GU"}) {
    const auto analysisChart = [&, source] (const char * id, const char * label, const char * variable, const char * level, const Ramp& ramp, const char * title, Quantity quantity, double step, bool barbs) {
        Product x;
        x.source = source;
        x.id = id;
        x.label = label;
        x.needs = [=] (int hour) {
            std::vector<GfsData::Need> needs{{hour, nbmWant("f", variable, level, "anl")}};
            if (barbs) {
                needs.push_back({hour, nbmWant("ws", "WIND", "10 m above ground", "anl")});
                needs.push_back({hour, nbmWant("wd", "WDIR", "10 m above ground", "anl")});
            }
            return needs;
        };
        if (barbs) {
            x.derive = windComponents;
            x.barbU = "u";
            x.barbV = "v";
        }
        x.fill = [] (const Grids& g) { return pick(g, "f"); };
        x.ramp = ramp;
        x.fillTitle = title;
        x.quantity = quantity;
        x.legendStep = step;
        return x;
    };
    p.push_back(analysisChart("2m_temp_10m_wnd", "2 m Temperature and 10 m Wind", "TMP", "2 m above ground", bands4F(temperature(), -32, 112), "2 m temperature", Quantity::Temperature, 5, true));
    p.push_back(analysisChart("2m_dewp_10m_wnd", "2 m Dewpoint and 10 m Wind", "DPT", "2 m above ground", bands4F(dewpoint(), -40, 84), "2 m dewpoint", Quantity::Temperature, 5, true));
    p.push_back(analysisChart("total_cloud_cover", "Total Cloud Cover and 10 m Wind", "TCDC", "entire atmosphere (considered as a single layer)", cloudCover(), "Total cloud cover (%)", Quantity::Other, 10, true));
    {   // the gust is the fill; the barbs are the sustained wind
        auto x = analysisChart("10m_wnd_gust", "10 m Wind and Gust", "GUST", "10 m above ground", lowWind(), "10 m wind gust (kt)", Quantity::Other, 10, true);
        x.fill = [] (const Grids& g) { return GfsGrid::scaled(pick(g, "f"), msToKnots); };
        p.push_back(x);
    }
    {
        auto x = analysisChart("visibility", "2-meter Visibility", "VIS", "surface", visibilityRamp(), "Visibility (miles)", Quantity::Other, 0, false);
        x.fill = [] (const Grids& g) { return GfsGrid::scaled(pick(g, "f"), 1.0 / 1609.344); };
        x.palette = "vis";
        p.push_back(x);
    }
}
{   // the temperature extreme of the 12 hours that end at this hour (the blend has them only for the periods it makes: 12 hour minimum and maximum)
    for (const auto& [id, label, variable, kind] : {std::tuple{"2m_min_temp", "2 m Minimum Temperature (12 hours ending)", "TMIN", "min"}, {"2m_max_temp", "2 m Maximum Temperature (12 hours ending)", "TMAX", "max"}}) {
        Product x;
        x.source = "NBM";
        x.id = id;
        x.label = label;
        x.needs = [=] (int hour) { return std::vector<GfsData::Need>{{hour, nbmWant("f", variable, "2 m above ground", window(hour, 12, kind))}}; };
        x.fill = [] (const Grids& g) { return pick(g, "f"); };
        x.ramp = temperature();
        x.fillTitle = "2 m temperature extreme";
        x.quantity = Quantity::Temperature;
        x.legendStep = 5;
        p.push_back(x);
    }
}
// precipitation and snowfall of the 1, 6 and 12 hours that end at the hour; the whole total is the sum of the 6 hour pieces
const auto nbmAccum = [&] (const char * id, const char * label, const char * variable, int length, const Ramp& ramp, Quantity quantity, double scale) {
    Product x;
    x.source = "NBM";
    x.id = id;
    x.label = label;
    x.needs = [=] (int hour) {
        std::vector<GfsData::Need> needs;
        if (length > 0) {
            needs.push_back({hour, nbmWant("a0", variable, "surface", window(hour, length, "acc"))});
        } else {   // the sum of the 6 hour pieces from the start to this hour
            int n = 0;
            for (int end = hour; end > 0; end -= 6) {
                needs.push_back({end, nbmWant("a" + std::to_string(n++), variable, "surface", window(end, std::min(6, end), "acc"))});
            }
        }
        return needs;
    };
    if (length != 1) {   // the 1 hour amounts of the same run added up: for the hours the 6 and 12 hour amounts are not made, as long as the hourly files go (the first 36 hours)
        x.fallbackNeeds = [=] (int hour) {
            std::vector<GfsData::Need> needs;
            const int count = length > 0 ? length : hour;
            if (hour > 36 || hour - count < 0) {
                return needs;
            }
            for (int i = 0; i < count; i++) {
                needs.push_back({hour - i, nbmWant("a" + std::to_string(i), variable, "surface", window(hour - i, 1, "acc"))});
            }
            return needs;
        };
    }
    x.derive = [scale] (Grids& g, const Context&) {
        auto sum = g["a0"];
        for (const auto& [key, grid] : g) {
            if (key != "a0" && key.size() >= 2 && key[0] == 'a' && std::isdigit(static_cast<unsigned char>(key[1]))) {
                for (size_t i = 0; i < sum.values.size(); i++) {
                    sum.values[i] += grid.values[i];
                }
            }
        }
        for (auto& v : sum.values) {
            v = static_cast<float>(std::max(0.0f, v) * scale);
        }
        g["sum"] = std::move(sum);
    };
    x.fill = [] (const Grids& g) { return pick(g, "sum"); };
    x.ramp = ramp;
    x.quantity = quantity;
    x.fillTitleFor = [length, label] (int hour) {
        return length > 0 ? std::string{label} + ", the " + std::to_string(length) + " hours ending at hour " + std::to_string(hour) : std::string{label} + ", from the start to hour " + std::to_string(hour);
    };
    x.legendStep = 0.0;
    return x;
};
p.push_back(nbmAccum("precip_p01", "Total Precipitation", "APCP", 1, precipitation(), Quantity::Millimeters, 1.0));
p.push_back(nbmAccum("precip_p06", "Total Precipitation", "APCP", 6, precipitation(), Quantity::Millimeters, 1.0));
p.push_back(nbmAccum("precip_p12", "Total Precipitation", "APCP", 12, precipitation(), Quantity::Millimeters, 1.0));
p.push_back(nbmAccum("precip_ptot", "Accumulated Precipitation", "APCP", 0, precipitation(), Quantity::Millimeters, 1.0));
p.push_back(nbmAccum("snow_p06", "Snowfall", "ASNOW", 6, snowfall(), Quantity::Centimeters, 100.0));   // the grid is in meters
p.push_back(nbmAccum("snow_p12", "Snowfall", "ASNOW", 12, snowfall(), Quantity::Centimeters, 100.0));
p.push_back(nbmAccum("snow_ptot", "Accumulated Snowfall", "ASNOW", 0, snowfall(), Quantity::Centimeters, 100.0));
{   // the chance of thunder in the 6 hours that end at this hour, and the CAPE
    Product x;
    x.source = "NBM";
    x.id = "tstm_prob";
    x.label = "Thunderstorm Probability (6 hours ending)";
    x.needs = [=] (int hour) { return std::vector<GfsData::Need>{{hour, nbmWant("f", "TSTM", "surface", window(hour, 6, "acc"), "probability forecast")}}; };
    x.fill = [] (const Grids& g) { return pick(g, "f"); };
    x.ramp = probability();
        x.palette = "prob";
    x.fillTitle = "Chance of a thunderstorm (%)";
    x.legendStep = 10;
    p.push_back(x);
    Product y;
    y.source = "NBM";
    y.id = "cape";
    y.label = "Surface-Based CAPE";
    y.needs = [=] (int hour) { return std::vector<GfsData::Need>{{hour, nbmWant("f", "CAPE", "surface", atHour(hour))}}; };
    y.fill = [] (const Grids& g) { return pick(g, "f"); };
    y.ramp = capeRamp();
    y.fillTitle = "Surface-based CAPE (J/kg)";
    y.legendStep = 500;
    p.push_back(y);
}

// ---- the rest of the blend's maps: chances of precipitation and of each type, snow (hourly, the percentiles, the chance of reaching an amount), the snow to liquid ratio, thunderstorm
// coverage and chance, visibility, ceiling and echo top, and the chances of low visibility and low ceilings
{
    const auto nbmChart = [&] (const char * id, const char * label) {
        Product x;
        x.source = "NBM";
        x.id = id;
        x.label = label;
        return x;
    };
    const auto chance = [&] (const char * id, const char * label, const char * variable, const char * level, std::function<std::string(int)> forecast, const char * detail, const char * title) {
        auto x = nbmChart(id, label);
        const std::string var = variable, lev = level, det = detail;
        x.needs = [=] (int hour) { return std::vector<GfsData::Need>{{hour, nbmWant("f", var.c_str(), lev.c_str(), forecast(hour), det)}}; };
        x.fill = [] (const Grids& g) { return pick(g, "f"); };
        x.ramp = probability();
        x.palette = "prob";
        x.fillTitle = title;
        x.legendStep = 10;
        return x;
    };
    const auto accWindow = [window] (int length) { return [window, length] (int hour) { return window(hour, std::min(length, hour), "acc"); }; };
    p.push_back(chance("1hour_precip_chance", "1-hour Chance of Precipitation", "APCP", "surface", accWindow(1), "prob >0.254", "Chance of 0.01 in or more of precipitation in the hour (%)"));
    p.push_back(chance("6hour_precip_chance", "6-hour Chance of Precipitation", "APCP", "surface", accWindow(6), "prob >0.254", "Chance of 0.01 in or more of precipitation in 6 hours (%)"));
    p.push_back(chance("12hour_precip_chance", "12-hour Chance of Precipitation", "APCP", "surface", accWindow(12), "prob >0.254", "Chance of 0.01 in or more of precipitation in 12 hours (%)"));
    const auto now = [atHour] (int hour) { return atHour(hour); };
    p.push_back(chance("prob_rain", "Probability of Rain", "PTYPE", "surface", now, "prob >=1 <2", "Chance of rain (%)"));
    p.push_back(chance("prob_snow", "Probability of Snow", "PTYPE", "surface", now, "prob >=8 <9", "Chance of snow (%)"));
    p.push_back(chance("prob_sleet", "Probability of Sleet", "PTYPE", "surface", now, "prob >=5 <7", "Chance of sleet (%)"));
    p.push_back(chance("prob_freezing_rain", "Probability of Freezing Rain", "PTYPE", "surface", now, "prob >=3 <4", "Chance of freezing rain (%)"));
    for (const auto& [id, label, inches, meters] : {std::tuple{"prob_1h_snow_0.1in", "Probability of 1-hour 0.1 inch of snow", "0.1 in", "0.00254"}, {"prob_1h_snow_0.5in", "Probability of 1-hour 0.5 inch of snow", "0.5 in", "0.0127"},
                                                    {"prob_1h_snow_1in", "Probability of 1-hour 1 inch of snow", "1 in", "0.0254"}, {"prob_1h_snow_1.5in", "Probability of 1-hour 1.5 inch of snow", "1.5 in", "0.0381"},
                                                    {"prob_1h_snow_2in", "Probability of 1-hour 2 inches of snow", "2 in", "0.0508"}, {"prob_1h_snow_3in", "Probability of 1-hour 3 inches of snow", "3 in", "0.0762"},
                                                    {"prob_1h_snow_4in", "Probability of 1-hour 4 inches of snow", "4 in", "0.1016"}}) {
        p.push_back(chance(id, label, "ASNOW", "surface", accWindow(1), (std::string{"prob >"} + meters).c_str(), (std::string{"Chance of "} + inches + " or more of snow in the hour (%)").c_str()));
    }
    p.push_back(nbmAccum("1hour_accu_snow", "1-hour accumulated snow", "ASNOW", 1, snowfall(), Quantity::Centimeters, 100.0));
    for (const int level : {10, 50, 90}) {   // the amounts that 10, 50 and 90 per cent of the blend's members stay under
        auto x = nbmChart((std::to_string(level) + "th_percentile_1hr_snow").c_str(), (std::to_string(level) + "th Percentile Hourly Snowfall").c_str());
        const auto detail = std::to_string(level) + "% level";
        x.needs = [=] (int hour) { return std::vector<GfsData::Need>{{hour, nbmWant("f", "ASNOW", "surface", std::to_string(hour - 1) + "-" + std::to_string(hour) + " hour acc@*", detail)}}; };
        x.fill = [] (const Grids& g) { return GfsGrid::scaled(pick(g, "f"), 100.0); };
        x.ramp = snowfall();
        x.quantity = Quantity::Centimeters;
        x.fillTitle = std::to_string(level) + "th percentile of the snow in the hour";
        x.legendStep = 0;
        p.push_back(x);
    }
    {
        auto x = nbmChart("snow_liquid_ratio", "Snow Liquid Ratio");
        x.needs = [=] (int hour) { return std::vector<GfsData::Need>{{hour, nbmWant("f", "SNOWLR", "surface", atHour(hour))}}; };
        x.fill = [] (const Grids& g) { return pick(g, "f"); };
        x.ramp = Ramp{{{0, QColor{"#b5368f"}}, {5, QColor{"#d9435f"}}, {8, QColor{"#ee7a47"}}, {10, QColor{"#f6e04a"}}, {13, QColor{"#a9d98a"}}, {16, QColor{"#55b6a8"}}, {20, QColor{"#2b7fc0"}}, {30, QColor{"#5b43a8"}}}};
        x.fillTitle = "Inches of snow for an inch of water (x to 1)";
        x.legendStep = 5;
        p.push_back(x);
    }
    {   // coverage: the 1 hour chance of thunder in the bands the forecasters use (isolated 10-20 %, scattered 30-50 %, numerous from 60 %)
        auto x = chance("tstm_coverage", "Thunderstorm Coverage", "TSTM", "surface", accWindow(1), "probability forecast", "Thunderstorm coverage in the hour: isolated, scattered, numerous");
        x.ramp = Ramp{{{0, QColor{255, 255, 255, 0}}, {9.9, QColor{255, 255, 255, 0}}, {10, QColor{"#ffe97a"}}, {29.9, QColor{"#ffe97a"}}, {30, QColor{"#f09a2e"}}, {59.9, QColor{"#f09a2e"}}, {60, QColor{"#c4262c"}}, {100, QColor{"#c4262c"}}}};
        x.legendStep = 0;
        p.push_back(x);
    }
    p.push_back(chance("prob_tstm", "Thunderstorm Probability", "TSTM", "surface", accWindow(1), "probability forecast", "Chance of a thunderstorm in the hour (%)"));
    {
        auto x = nbmChart("visibility", "2-meter Visibility");
        x.needs = [=] (int hour) { return std::vector<GfsData::Need>{{hour, nbmWant("f", "VIS", "surface", atHour(hour))}}; };
        x.fill = [] (const Grids& g) { return GfsGrid::scaled(pick(g, "f"), 1.0 / 1609.344); };
        x.ramp = visibilityRamp();
        x.palette = "vis";
        x.fillTitle = "Visibility (miles)";
        x.legendStep = 0;
        p.push_back(x);
    }
    {
        auto x = nbmChart("ceiling", "Ceiling");
        x.needs = [=] (int hour) { return std::vector<GfsData::Need>{{hour, nbmWant("f", "CEIL", "cloud ceiling", atHour(hour))}}; };
        x.fill = [] (const Grids& g) { return GfsGrid::scaled(pick(g, "f"), 3.28084); };
        x.ramp = ceilingRamp();
        x.palette = "ceiling";
        x.fillTitle = "Ceiling (feet)";
        x.legendStep = 0;
        p.push_back(x);
    }
    {
        auto x = nbmChart("echo_top", "Echo Top Height");
        x.needs = [=] (int hour) { return std::vector<GfsData::Need>{{hour, nbmWant("f", "RETOP", "cloud top", atHour(hour))}}; };
        x.fill = [] (const Grids& g) {   // where there is no echo the blend writes a huge number, not a missing value
            auto out = GfsGrid::scaled(pick(g, "f"), 3.28084 / 1000.0);
            for (auto& v : out.values) {
                if (v > 80.0f || v < 0.0f) {
                    v = std::nanf("");
                }
            }
            return out;
        };
        x.ramp = echoTopRamp();
        x.palette = "echo_top";
        x.fillTitle = "Echo top (thousands of feet)";
        x.legendStep = 10;
        p.push_back(x);
    }
    {   // the number of hours with precipitation in the last 12: the hourly amounts counted where there is at least 0.01 inch (the blend has the hourly amounts to hour 36)
        auto x = nbmChart("precip_duration", "Precipitation duration");
        x.needs = [=] (int hour) {
            std::vector<GfsData::Need> out;
            for (int k = std::max(hour - 11, 1); k <= hour; k++) {
                out.push_back({k, nbmWant("d" + std::to_string(k), "APCP", "surface", window(k, 1, "acc"))});
            }
            return out;
        };
        x.derive = [] (Grids& g, const Context&) {
            GfsGrid::Grid count;
            for (const auto& [key, grid] : g) {
                if (key.size() >= 2 && key[0] == 'd' && std::isdigit(static_cast<unsigned char>(key[1]))) {
                    if (count.empty()) {
                        count = grid;
                        std::fill(count.values.begin(), count.values.end(), 0.0f);
                    }
                    for (size_t i = 0; i < count.values.size(); i++) {
                        count.values[i] = std::isnan(grid.values[i]) || std::isnan(count.values[i]) ? std::nanf("") : count.values[i] + (grid.values[i] >= 0.254f ? 1.0f : 0.0f);
                    }
                }
            }
            g["count"] = std::move(count);
        };
        x.fill = [] (const Grids& g) { return pick(g, "count"); };
        if (const auto * bands = magPalette("duration")) {
            x.ramp = *bands;
        }
        x.palette = "duration";
        x.fillTitle = "Hours with precipitation in the last 12 (0.01 in or more in the hour)";
        x.legendStep = 0;
        p.push_back(x);
    }
    for (const auto& [id, label, miles, meters] : {std::tuple{"prob_vis_5mi", "Probability of Visibility 5 miles or less", "5 miles", "8046.73"}, {"prob_vis_3mi", "Probability of Visibility less than 3 miles", "3 miles", "4828.03"},
                                                   {"prob_vis_2mi", "Probability of Visibility less than 2 miles", "2 miles", "3218.69"}, {"prob_vis_1mi", "Probability of Visibility less than 1 mile", "1 mile", "1609.34"}}) {
        p.push_back(chance(id, label, "VIS", "surface", now, (std::string{"prob <"} + meters).c_str(), (std::string{"Chance of visibility under "} + miles + " (%)").c_str()));
    }
    for (const auto& [id, label, feet, meters] : {std::tuple{"prob_ceil_1000ft", "Probability of Ceiling Less than 1000 feet", "1000 feet", "304.8"}, {"prob_ceil_2000ft", "Probability of Ceiling Less than 2000 feet", "2000 feet", "609.6"},
                                                  {"prob_ceil_3000ft", "Probability of Ceiling 3000 feet or less", "3000 feet", "914.5"}, {"prob_ceil_6500ft", "Probability of Ceiling 6500 feet or less", "6500 feet", "2011.68"}}) {
        p.push_back(chance(id, label, "CEIL", "cloud ceiling", now, (std::string{"prob <"} + meters).c_str(), (std::string{"Chance of a ceiling under "} + feet + " (%)").c_str()));
    }
}

}

}  // namespace detail
}  // namespace GfsChart
