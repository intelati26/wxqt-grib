// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef UTILITYRIVERS_H
#define UTILITYRIVERS_H

#include <limits>
#include <string>
#include <vector>

using std::string;
using std::vector;

// River gauges from NOAA / NWS: the National Water Prediction Service (NWPS) for the official stages, flood categories, impacts and
// forecasts, and the National Water Model's flow forecast for the gauge's river reach. All public, no key.
//   map layer:  mapservices.weather.noaa.gov/eventdriven/rest/services/water/riv_gauges/MapServer/0   (every gauge, with its category)
//   a gauge:    api.water.noaa.gov/nwps/v1/gauges/{id}, .../stageflow, .../ratings, and reaches/{reachId}/streamflow
namespace UtilityRivers {
    constexpr double none = std::numeric_limits<double>::quiet_NaN();
    inline bool has(double value) { return value == value; }

    struct Gauge {
        string lid;
        string name;
        string waterbody;
        string state;
        string wfo;
        string status;        // no_flooding, action, minor, moderate, major, low_threshold, obs_not_current, out_of_service, not_defined
        string units;
        string obsTime;       // as the service gives it (UTC)
        double observed{none};
        double action{none};
        double minor{none};
        double moderate{none};
        double major{none};
        double lat{0.0};
        double lon{0.0};
        double mercator{0.0}; // the Mercator y of the latitude, kept so a map does not recompute it
    };
    // every gauge (about 13,000), newest from the service; cached for ten minutes
    bool loadGauges(vector<Gauge>&, string& error);

    struct Point {
        long long time;   // seconds since 1970, UTC
        double value;
    };
    struct Series {
        string name;
        string unit;
        vector<Point> points;
    };
    struct Crest {
        string time;
        double stage;
        double flow;
    };
    struct Impact {
        double stage;
        string statement;
    };
    struct RatingPoint {
        double stage;   // ft
        double flow;    // cfs
    };

    struct Detail {
        string lid;
        string name;
        string usgsId;
        string reachId;
        string rfc;
        string wfo;
        string state;
        string county;
        string upstream;
        string downstream;
        string observedCategory;
        string forecastCategory;
        string forecastNote;      // why there is no forecast (the service's own sentence)
        string stageUnit{"ft"};
        double lat{0.0};
        double lon{0.0};
        double action{none};
        double minor{none};
        double moderate{none};
        double major{none};
        vector<Impact> impacts;                 // highest stage first
        vector<Crest> crestsByStage;            // the record crests, highest first
        vector<Crest> crestsRecent;
        string outlookInterval;                 // the long-range flood-risk outlook: "OND" ...
        string outlookMinor;
        string outlookModerate;
        string outlookMajor;
        string outlookProduced;
        string probabilityStageUrl;
        string probabilityFlowUrl;
        string hydrographUrl;
        Series observedStage;
        Series observedFlow;                    // cfs
        Series forecastStage;
        Series forecastFlow;                    // cfs
        string observedIssued;
        string forecastIssued;
        Series modelAnalysis;                   // National Water Model flow, cfs: the last two days
        Series modelShort;                      // National Water Model flow, cfs: the next hours
        Series modelMedium;
        string modelReferenceAnalysis;
        string modelReferenceShort;
        vector<RatingPoint> rating;             // stage <-> flow
    };
    // blocking (use off the UI thread); false with an error text when the gauge itself cannot be read. The model and the rating are optional.
    bool loadDetail(const string& lid, Detail&, string& error);

    // linear between the rating curve's points; none outside the curve (no extrapolation)
    double stageFromFlow(const vector<RatingPoint>&, double flowCfs);
    double flowFromStage(const vector<RatingPoint>&, double stage);

    string statusLabel(const string& status);   // "Minor flooding", "Action stage" ...
    // the stage thresholds as one line: "action 15, minor 18, moderate 20, major 22 ft"
    string thresholdText(const Detail&);
}

#endif  // UTILITYRIVERS_H
