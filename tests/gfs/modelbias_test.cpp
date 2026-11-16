// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

// How wrong a model has been (cfs/ModelBias): bias and size of the miss from made-up forecasts and truth, the days that count, the parse of the services' answers.
#include <cmath>
#include <cstdio>
#include <QTimeZone>
#include <QDate>
#include <QDateTime>
#include "cfs/ModelBias.h"

namespace B = ModelBias;
static int failures = 0;
#define CHECK(x) do { if (!(x)) { std::printf("FAILED line %d: %s\n", __LINE__, #x); failures++; } } while (0)
static bool near(double a, double b, double eps = 1e-4) { return std::fabs(a - b) < eps; }

int main() {
    // three days of hours; the truth is 10; the forecast is 12 on the first day, 9 on the second, 10 on the third
    std::vector<double> times;
    std::vector<float> forecast;
    B::Truth truth;
    for (int i = 0; i < 72; i++) {
        times.push_back(86400.0 * 100 + i * 3600.0);
        forecast.push_back(i < 24 ? 12.0f : i < 48 ? 9.0f : 10.0f);
        truth.times.push_back(times.back());
        truth.values.push_back(10.0f);
    }
    auto s = B::evaluate(times, 0, forecast, truth, false);
    CHECK(s.n == 72 && near(s.bias, (24 * 2 - 24 * 1) / 72.0) && near(s.mae, (24 * 2 + 24 * 1) / 72.0));
    CHECK(s.dayStart.size() == 3 && near(s.dayError[0], 2) && near(s.dayError[1], -1) && near(s.dayError[2], 0) && near(s.dayStart[1], 86400.0 * 101));
    // a day with the truth missing for most of it is left out of the days (and out of the bias too: only the hours with both count)
    for (int i = 24; i < 46; i++) {
        truth.values[static_cast<size_t>(i)] = std::nanf("");
    }
    s = B::evaluate(times, 0, forecast, truth, false);
    CHECK(s.dayStart.size() == 2 && s.n == 72 - 22 && near(s.dayError[0], 2) && near(s.dayError[1], 0));
    // the place's own days: six hours behind UTC, the days begin at 06 UTC, so 72 hours from 00 UTC hold two whole days (06 to 06) and two parts
    truth.values.assign(72, 10.0f);
    s = B::evaluate(times, -6 * 3600, forecast, truth, false);
    CHECK(s.dayStart.size() == 2 && near(s.dayStart[0], 86400.0 * 100 + 6 * 3600));
    // an added-up variable: a day's total of forecast minus truth (1 mm an hour against 0.5), only whole days
    std::vector<float> rain(72, 1.0f);
    B::Truth rainTruth = truth;
    rainTruth.values.assign(72, 0.5f);
    const auto r = B::evaluate(times, 0, rain, rainTruth, true);
    CHECK(r.n == 3 && r.dayStart.size() == 3 && near(r.dayError[0], 12) && near(r.bias, 12) && near(r.mae, 12));
    const auto partial = B::evaluate(times, -6 * 3600, rain, rainTruth, true);   // (its own days: two whole ones)
    CHECK(partial.n == 2);

    // the answers: two models, the previous days as `<variable>_previous_dayN_<model>`, null for a lead the model does not keep
    const QByteArray good = R"({"utc_offset_seconds":-21600,"hourly":{"time":[100,3700],
        "temperature_2m_ecmwf_ifs025":[1.5,2.5],"temperature_2m_previous_day1_ecmwf_ifs025":[1,2],"temperature_2m_previous_day7_ecmwf_ifs025":[null,null],
        "temperature_2m_gfs_global":[1,1],"temperature_2m_previous_day3_gfs_global":[3,null]}})";
    B::Forecasts f;
    std::string why;
    CHECK(B::parseForecasts(good, "temperature_2m", {"ecmwf_ifs025", "gfs_global"}, f, why) && f.utcOffset == -21600 && f.times.size() == 2 && f.byModel.size() == 2);
    CHECK(near(f.byModel["ecmwf_ifs025"][0][0], 1.5) && near(f.byModel["ecmwf_ifs025"][1][1], 2) && std::isnan(f.byModel["ecmwf_ifs025"][7][0]) && std::isnan(f.byModel["ecmwf_ifs025"][3][0]));
    CHECK(near(f.byModel["gfs_global"][3][0], 3) && std::isnan(f.byModel["gfs_global"][3][1]));
    CHECK(!B::parseForecasts(good, "temperature_2m", {"ecmwf_ifs025", "nothing"}, f, why) && why.find("nothing") != std::string::npos);
    CHECK(!B::parseForecasts(R"({"error":true,"reason":"Cannot initialize"})", "temperature_2m", {"a"}, f, why) && why.find("Cannot") != std::string::npos && !B::parseForecasts("x", "t", {"a"}, f, why));
    B::Truth t;
    CHECK(B::parseTruth(R"({"utc_offset_seconds":3600,"hourly":{"time":[100,3700],"temperature_2m":[5,null]}})", "temperature_2m", t, why) && t.values.size() == 2 && near(t.values[0], 5) && std::isnan(t.values[1]) && t.utcOffset == 3600);
    CHECK(!B::parseTruth(R"({"hourly":{"time":[1]}})", "temperature_2m", t, why) && !B::parseTruth("x", "t", t, why));
    CHECK(B::forecastUrl(39.1, -94.6, "temperature_2m", {"ecmwf_ifs025", "gfs_global"}, 30).find("temperature_2m_previous_day7&models=ecmwf_ifs025,gfs_global&past_days=30&forecast_days=1") != std::string::npos);
    CHECK(B::truthUrl(39.1, -94.6, "temperature_2m", "2026-10-01", "2026-11-01").find("start_date=2026-10-01&end_date=2026-11-01&hourly=temperature_2m") != std::string::npos);
    // a station: the nearest of the stations of a StnMeta answer, and its days in deg C and mm
    B::Station st;
    const QByteArray meta = R"({"meta":[{"ll":[-94.73,39.30],"name":"KANSAS CITY INTL AP","uid":11479},{"ll":[-94.59,39.13],"name":"KANSAS CITY DOWNTOWN AP","uid":11461},{"ll":[-95.19,39.05],"name":"KU-FIELD STATION","uid":93860},{"name":"no position","uid":1}]})";
    CHECK(B::parseStationMeta(meta, 39.12, -94.60, st, why) && st.uid == "11461" && st.name == "KANSAS CITY DOWNTOWN AP" && st.km < 3.0);
    CHECK(!B::parseStationMeta(R"({"meta":[]})", 39, -94, st, why) && !B::parseStationMeta("x", 39, -94, st, why));
    std::map<std::string, float> obs;
    const QByteArray data = R"({"data":[["2026-11-08","66","40","0.00"],["2026-11-09","72","M","T"],["2026-11-10","70","43","0.10"]]})";
    CHECK(B::parseStationDaily(data, 1, obs, why) && obs.size() == 3 && near(obs["2026-11-08"], (66 - 32) / 1.8, 1e-3) && near(obs["2026-11-09"], (72 - 32) / 1.8, 1e-3));
    CHECK(B::parseStationDaily(data, 2, obs, why) && obs.size() == 2 && obs.count("2026-11-09") == 0);   // (a missing day is left out)
    CHECK(B::parseStationDaily(data, 3, obs, why) && obs.size() == 3 && near(obs["2026-11-09"], 0.0) && near(obs["2026-11-10"], 2.54, 1e-3));
    // the forecast reduced to the place's days: the hours of 2026-11-08, -09 and -10
    std::vector<double> dt;
    std::vector<float> df;
    const double day0 = static_cast<double>(QDateTime{QDate{2026, 11, 8}, QTime{0, 0}, QTimeZone::utc()}.toSecsSinceEpoch());   // 2026-11-08 00:00 UTC
    for (int i = 0; i < 72; i++) {
        dt.push_back(day0 + i * 3600.0);
        df.push_back(static_cast<float>(i % 24));   // 0 to 23 every day: the high 23, the low 0, the rain total 276
    }
    std::map<std::string, float> highs{{"2026-11-08", 20.0f}, {"2026-11-09", 25.0f}};
    auto hs = B::evaluateDaily(dt, 0, df, highs, B::Judge::StationHigh);
    CHECK(hs.n == 2 && near(hs.dayError[0], 3) && near(hs.dayError[1], -2) && near(hs.bias, 0.5) && near(hs.mae, 2.5) && near(hs.dayStart[0], day0));
    std::map<std::string, float> lows{{"2026-11-09", 2.0f}};
    auto ls = B::evaluateDaily(dt, 0, df, lows, B::Judge::StationLow);
    CHECK(ls.n == 1 && near(ls.dayError[0], -2));
    std::map<std::string, float> rainDays{{"2026-11-08", 270.0f}};
    auto rs = B::evaluateDaily(dt, 0, df, rainDays, B::Judge::StationRain);
    CHECK(rs.n == 1 && near(rs.dayError[0], 6));
    const auto shifted = B::evaluateDaily(dt, -6 * 3600, df, highs, B::Judge::StationHigh);   // the place's days begin at 06 UTC: two whole days (8th and 9th), the third has 18 hours
    CHECK(shifted.n == 2 && near(shifted.dayStart[0], day0 + 6 * 3600));
    const auto daily = B::dailyOf(dt, 0, df, B::Aggregate::High);
    CHECK(daily.size() == 3 && near(daily.at(day0), 23) && near(B::dailyOf(dt, 0, df, B::Aggregate::Low).at(day0), 0) && near(B::dailyOf(dt, 0, df, B::Aggregate::Mean).at(day0), 11.5) && near(B::dailyOf(dt, 0, df, B::Aggregate::Sum).at(day0), 276));
    CHECK(B::dailyOf(dt, -6 * 3600, df, B::Aggregate::High).size() == 2 && B::dailyOf(dt, -6 * 3600, df, B::Aggregate::Sum).size() == 2);
    CHECK(B::models().size() >= 6 && B::models().front().id == "ecmwf_ifs025");
    std::printf(failures == 0 ? "all model bias tests passed\n" : "%d failures\n", failures);
    return failures == 0 ? 0 : 1;
}
