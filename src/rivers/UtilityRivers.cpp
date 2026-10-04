// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "rivers/UtilityRivers.h"
#include <algorithm>
#include <cmath>
#include <ctime>
#include <mutex>
#include <numbers>
#include <QByteArray>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include "util/UtilityIO.h"

namespace {
    const string mapLayer{"https://mapservices.weather.noaa.gov/eventdriven/rest/services/water/riv_gauges/MapServer/0/query"};
    const string nwps{"https://api.water.noaa.gov/nwps/v1/"};

    // the services mark a missing number with -999 / -9999, or give an empty string
    double number(const QJsonValue& value) {
        double v = UtilityRivers::none;
        if (value.isDouble()) {
            v = value.toDouble();
        } else if (value.isString()) {
            bool ok = false;
            const double parsed = value.toString().toDouble(&ok);
            v = ok ? parsed : UtilityRivers::none;
        }
        return UtilityRivers::has(v) && v > -900.0 ? v : UtilityRivers::none;
    }

    long long epoch(const QString& iso) {
        auto time = QDateTime::fromString(iso, Qt::ISODate);
        time.setTimeSpec(Qt::UTC);
        return time.isValid() ? time.toSecsSinceEpoch() : 0;
    }

    QJsonObject fetchObject(const string& url) {
        const auto bytes = UtilityIO::downloadAsByteArray(url);
        return bytes.isEmpty() ? QJsonObject{} : QJsonDocument::fromJson(bytes).object();
    }

    // the observed or forecast block of .../stageflow: the stage and the flow in cfs (the service says which unit it used)
    void readStageFlow(const QJsonObject& block, UtilityRivers::Series& stage, UtilityRivers::Series& flow, string& issued) {
        const auto data = block.value("data").toArray();
        if (data.isEmpty()) {
            return;
        }
        issued = block.value("issuedTime").toString().toStdString();
        stage.name = block.value("primaryName").toString().toStdString();
        stage.unit = block.value("primaryUnits").toString().toStdString();
        flow.name = block.value("secondaryName").toString().toStdString();
        flow.unit = "cfs";
        const auto secondaryUnit = block.value("secondaryUnits").toString().toLower();
        const double toCfs = secondaryUnit == "kcfs" ? 1000.0 : 1.0;
        for (const auto& item : data) {
            const auto o = item.toObject();
            const auto time = epoch(o.value("validTime").toString());
            const double primary = number(o.value("primary"));
            const double secondary = number(o.value("secondary"));
            if (time == 0) {
                continue;
            }
            if (UtilityRivers::has(primary)) {
                stage.points.push_back({time, primary});
            }
            if (UtilityRivers::has(secondary)) {
                flow.points.push_back({time, secondary * toCfs});
            }
        }
    }

    void readModel(const QJsonObject& block, UtilityRivers::Series& out, string& reference, const char * name) {
        out.name = name;
        out.unit = "cfs";
        const auto seriesObject = block.value("series").toObject();   // {referenceTime, units, data: [...]}
        const auto series = seriesObject.value("data").toArray();
        reference = seriesObject.value("referenceTime").toString().toStdString();
        for (const auto& item : series) {
            const auto o = item.toObject();
            const auto time = epoch(o.value("validTime").toString());
            const double flow = number(o.value("flow"));
            if (time != 0 && UtilityRivers::has(flow)) {
                out.points.push_back({time, flow});
            }
        }
    }

    vector<UtilityRivers::Crest> readCrests(const QJsonArray& list) {
        vector<UtilityRivers::Crest> crests;
        for (const auto& item : list) {
            const auto o = item.toObject();
            crests.push_back({o.value("occurredTime").toString().toStdString(), number(o.value("stage")), number(o.value("flow"))});
        }
        return crests;
    }

    string fixed(double value) {
        char text[32];
        std::snprintf(text, sizeof(text), "%g", std::round(value * 100.0) / 100.0);
        return text;
    }
}

bool UtilityRivers::loadGauges(vector<Gauge>& out, string& error) {
    static std::mutex lock;
    static vector<Gauge> cache;
    static std::time_t cachedAt = 0;
    {
        std::lock_guard<std::mutex> guard{lock};
        if (!cache.empty() && std::time(nullptr) - cachedAt < 600) {
            out = cache;
            return true;
        }
    }
    vector<Gauge> all;
    const string fields{"gaugelid,status,location,waterbody,state,obstime,wfo,action,units,flood,moderate,major,observed,latitude,longitude"};
    for (int offset = 0; offset < 40000; offset += 10000) {
        const auto url = mapLayer + "?where=1%3D1&outFields=" + fields + "&returnGeometry=false&orderByFields=objectid&resultOffset=" +
            std::to_string(offset) + "&resultRecordCount=10000&f=json";
        const auto root = fetchObject(url);
        const auto features = root.value("features").toArray();
        if (features.isEmpty() && offset == 0) {
            error = "The river gauge service did not answer.";
            return false;
        }
        for (const auto& item : features) {
            const auto a = item.toObject().value("attributes").toObject();
            Gauge g;
            g.lid = a.value("gaugelid").toString().toStdString();
            g.name = a.value("location").toString().toStdString();
            g.waterbody = a.value("waterbody").toString().toStdString();
            g.state = a.value("state").toString().toStdString();
            g.wfo = a.value("wfo").toString().toStdString();
            g.status = a.value("status").toString().toStdString();
            g.units = a.value("units").toString().toStdString();
            g.obsTime = a.value("obstime").toString().toStdString();
            g.observed = number(a.value("observed"));
            g.action = number(a.value("action"));
            g.minor = number(a.value("flood"));
            g.moderate = number(a.value("moderate"));
            g.major = number(a.value("major"));
            g.lat = a.value("latitude").toDouble();
            g.lon = a.value("longitude").toDouble();
            if (g.lid.empty() || (g.lat == 0.0 && g.lon == 0.0)) {
                continue;
            }
            g.mercator = 180.0 / std::numbers::pi * std::log(std::tan(std::numbers::pi / 4.0 + g.lat * std::numbers::pi / 360.0));
            all.push_back(std::move(g));
        }
        if (!root.value("exceededTransferLimit").toBool()) {
            break;
        }
    }
    {
        std::lock_guard<std::mutex> guard{lock};
        cache = all;
        cachedAt = std::time(nullptr);
    }
    out = std::move(all);
    return true;
}

bool UtilityRivers::loadDetail(const string& lid, Detail& d, string& error) {
    const auto gauge = fetchObject(nwps + "gauges/" + lid);
    if (gauge.isEmpty() || gauge.value("lid").toString().isEmpty()) {
        error = "NWPS has no gauge " + lid + ", or did not answer.";
        return false;
    }
    d.lid = lid;
    d.name = gauge.value("name").toString().toStdString();
    d.usgsId = gauge.value("usgsId").toString().toStdString();
    d.reachId = gauge.value("reachId").toString().toStdString();
    d.rfc = gauge.value("rfc").toObject().value("name").toString().toStdString();
    d.wfo = gauge.value("wfo").toObject().value("name").toString().toStdString();
    d.state = gauge.value("state").toObject().value("name").toString().toStdString();
    d.county = gauge.value("county").toString().toStdString();
    d.upstream = gauge.value("upstreamLid").toString().toStdString();
    d.downstream = gauge.value("downstreamLid").toString().toStdString();
    d.observedCategory = gauge.value("ObservedFloodCategory").toString().toStdString();
    d.forecastCategory = gauge.value("ForecastFloodCategory").toString().toStdString();
    d.forecastNote = gauge.value("forecastReliability").toString().toStdString();
    d.lat = gauge.value("latitude").toDouble();
    d.lon = gauge.value("longitude").toDouble();
    const auto flood = gauge.value("flood").toObject();
    d.stageUnit = flood.value("stageUnits").toString("ft").toStdString();
    const auto categories = flood.value("categories").toObject();
    d.action = number(categories.value("action").toObject().value("stage"));
    d.minor = number(categories.value("minor").toObject().value("stage"));
    d.moderate = number(categories.value("moderate").toObject().value("stage"));
    d.major = number(categories.value("major").toObject().value("stage"));
    for (const auto& item : flood.value("impacts").toArray()) {
        const auto o = item.toObject();
        d.impacts.push_back({number(o.value("stage")), o.value("statement").toString().trimmed().toStdString()});
    }
    std::sort(d.impacts.begin(), d.impacts.end(), [] (const Impact& a, const Impact& b) { return a.stage > b.stage; });
    const auto crests = flood.value("crests").toObject();
    d.crestsByStage = readCrests(crests.value("historic").toArray());
    d.crestsRecent = readCrests(crests.value("recent").toArray());
    const auto lro = flood.value("lro").toObject();
    d.outlookInterval = lro.value("interval").toString().toStdString();
    d.outlookMinor = lro.value("minorCS").toString().toStdString();
    d.outlookModerate = lro.value("moderateCS").toString().toStdString();
    d.outlookMajor = lro.value("majorCS").toString().toStdString();
    d.outlookProduced = lro.value("producedTime").toString().toStdString();
    const auto images = gauge.value("images").toObject();
    const auto weekly = images.value("probability").toObject().value("weekint").toObject();
    d.probabilityStageUrl = weekly.value("stage").toString().toStdString();
    d.probabilityFlowUrl = weekly.value("flow").toString().toStdString();
    d.hydrographUrl = images.value("hydrograph").toObject().value("default").toString().toStdString();

    // the stage and flow, observed and forecast
    const auto stageflow = fetchObject(nwps + "gauges/" + lid + "/stageflow");
    readStageFlow(stageflow.value("observed").toObject(), d.observedStage, d.observedFlow, d.observedIssued);
    readStageFlow(stageflow.value("forecast").toObject(), d.forecastStage, d.forecastFlow, d.forecastIssued);

    // the National Water Model, for the river reach the gauge is on
    if (!d.reachId.empty()) {
        const auto reach = fetchObject(nwps + "reaches/" + d.reachId + "/streamflow");
        readModel(reach.value("analysisAssimilation").toObject(), d.modelAnalysis, d.modelReferenceAnalysis, "National Water Model, analysis");
        readModel(reach.value("shortRange").toObject(), d.modelShort, d.modelReferenceShort, "National Water Model, short range");
        string ignored;
        readModel(reach.value("mediumRange").toObject(), d.modelMedium, ignored, "National Water Model, medium range");
    }
    // the rating curve (stage <-> flow), to put the model's flow on the stage scale
    const auto ratings = fetchObject(nwps + "gauges/" + lid + "/ratings");
    for (const auto& item : ratings.value("data").toArray()) {
        const auto o = item.toObject();
        const double stage = number(o.value("stage"));
        const double flow = number(o.value("flow"));
        if (has(stage) && has(flow)) {
            d.rating.push_back({stage, flow});
        }
    }
    std::sort(d.rating.begin(), d.rating.end(), [] (const RatingPoint& a, const RatingPoint& b) { return a.stage < b.stage; });
    return true;
}

double UtilityRivers::stageFromFlow(const vector<RatingPoint>& rating, double flow) {
    if (rating.size() < 2 || !has(flow)) {
        return none;
    }
    for (size_t i = 1; i < rating.size(); i += 1) {
        const auto& a = rating[i - 1];
        const auto& b = rating[i];
        if (flow >= a.flow && flow <= b.flow) {
            return b.flow == a.flow ? a.stage : a.stage + (b.stage - a.stage) * (flow - a.flow) / (b.flow - a.flow);
        }
    }
    return none;
}

double UtilityRivers::flowFromStage(const vector<RatingPoint>& rating, double stage) {
    if (rating.size() < 2 || !has(stage)) {
        return none;
    }
    for (size_t i = 1; i < rating.size(); i += 1) {
        const auto& a = rating[i - 1];
        const auto& b = rating[i];
        if (stage >= a.stage && stage <= b.stage) {
            return b.stage == a.stage ? a.flow : a.flow + (b.flow - a.flow) * (stage - a.stage) / (b.stage - a.stage);
        }
    }
    return none;
}

string UtilityRivers::statusLabel(const string& status) {
    if (status == "major") {
        return "Major flooding";
    }
    if (status == "moderate") {
        return "Moderate flooding";
    }
    if (status == "minor") {
        return "Minor flooding";
    }
    if (status == "action") {
        return "Action stage";
    }
    if (status == "no_flooding") {
        return "No flooding";
    }
    if (status == "low_threshold") {
        return "Below the low-water level";
    }
    if (status == "obs_not_current") {
        return "Observation not current";
    }
    if (status == "out_of_service") {
        return "Out of service";
    }
    if (status == "not_defined") {
        return "No flood stages defined";
    }
    if (status == "fcst_not_current") {
        return "No current forecast";
    }
    return status;
}

string UtilityRivers::thresholdText(const Detail& d) {
    string text;
    const auto add = [&text] (const char * label, double value) {
        if (has(value)) {
            text += (text.empty() ? "" : ", ") + string{label} + " " + fixed(value);
        }
    };
    add("action", d.action);
    add("minor", d.minor);
    add("moderate", d.moderate);
    add("major", d.major);
    return text.empty() ? string{"no flood stages defined"} : text + " " + d.stageUnit;
}
