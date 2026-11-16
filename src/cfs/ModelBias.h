// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef MODELBIAS_H
#define MODELBIAS_H

#include <map>
#include <string>
#include <vector>
#include <QByteArray>

// How wrong each model has been at a place, and in which direction (docs/plans.md, "Model comparison at a place": the second step, bias against what happened). A forecast of 1 to 7 days ahead is
// what a model said that many days before; the past runs come from Open-Meteo's previous runs service (`<variable>_previous_dayN`), and "what happened" is ERA5, the reanalysis (the archive service,
// about five days behind the present: the last days have no truth and are left out). The numbers are plain functions here, tested without the network.
namespace ModelBias {
    // one model's forecasts of one variable: `lead[k][t]` is the value for time t made k days ahead (lead 0 is the newest run's own first hours), NaN where there is none
    struct Forecasts {
        std::vector<double> times;
        int utcOffset{0};
        std::map<std::string, std::vector<std::vector<float>>> byModel;   // model -> 8 series, lead 0 to 7
    };
    struct Truth {
        std::vector<double> times;
        std::vector<float> values;
        int utcOffset{0};
    };
    // What a forecast did against the truth. `bias` is forecast minus truth, averaged (positive: the forecast was too high), `mae` the average size of the miss, `n` the hours (or days, for
    // an added-up variable) it is made of; `dayStart` / `dayError` the error of each of the place's own days (unix seconds of its midnight, and the day's mean error, or for rain and snow the
    // forecast total minus the real one).
    struct Stat {
        float bias{0}, mae{0};
        int n{0};
        std::vector<double> dayStart;
        std::vector<float> dayError;
    };
    // `forecast` is on `times`; `truth` on its own times. A day needs 20 hours with both (an added-up variable needs all 24, and is a day's total).
    Stat evaluate(const std::vector<double>& times, int utcOffset, const std::vector<float>& forecast, const Truth& truth, bool dailySum);
    bool parseForecasts(const QByteArray& json, const std::string& variable, const std::vector<std::string>& models, Forecasts& out, std::string& why);
    bool parseTruth(const QByteArray& json, const std::string& variable, Truth& out, std::string& why);
    std::string forecastUrl(double lat, double lon, const std::string& variable, const std::vector<std::string>& models, int pastDays);
    std::string truthUrl(double lat, double lon, const std::string& variable, const std::string& from, const std::string& to);
    // The models that keep past runs: id, what it is.
    struct Model { std::string id, label; };
    const std::vector<Model>& models();
    // Both models' forecasts and the truth for a place over the last `days` days (7 to 60). Blocking; kept 20 minutes.
    bool fetch(double lat, double lon, const std::string& variable, const std::string& modelA, const std::string& modelB, int days, Forecasts& forecasts, Truth& truth, std::string& why);
}

#endif  // MODELBIAS_H
