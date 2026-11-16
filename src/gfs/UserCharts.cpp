// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "gfs/UserCharts.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <QColor>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <regex>
#include "gfs/GfsChartInternal.h"
#include "gfs/GfsModels.h"

namespace {
    using GfsChart::Product;
    using GfsChart::Ramp;
    using GfsChart::Grids;
    constexpr int maxBytes = 128 * 1024;

    std::vector<std::string> problemList;

    const std::map<std::string, std::function<Ramp()>>& ramps() {
        using namespace GfsChart::detail;
        static const std::map<std::string, std::function<Ramp()>> table{
            {"temperature", [] { return temperature(); }}, {"wind", [] { return windSpeed(); }}, {"low-wind", [] { return lowWind(); }}, {"humidity", [] { return humidity(); }},
            {"precipitation", [] { return precipitation(); }}, {"precipitable-water", [] { return precipitableWater(); }}, {"reflectivity", [] { return reflectivity(); }},
            {"dewpoint", [] { return dewpoint(); }}, {"sea-level-pressure", [] { return seaLevelPressure(); }}, {"cape", [] { return capeRamp(); }}, {"cloud-cover", [] { return cloudCover(); }},
            {"probability", [] { return probability(); }}, {"vorticity", [] { return vorticityRamp(); }}, {"divergence", [] { return divergenceRamp(); }}, {"theta-e", [] { return thetaERamp(); }},
            {"stratosphere-temperature", [] { return stratosphereTemperature(); }}, {"sea-surface-temperature", [] { return seaSurfaceTemperature(); }}, {"wave-height", [] { return waveHeight(); }},
            {"snowfall", [] { return snowfall(); }}, {"shear", [] { return shearRamp(); }}, {"helicity", [] { return helicityRamp(); }}, {"updraft-helicity", [] { return updraftHelicity(); }},
            {"height-anomaly", [] { return heightAnomaly(); }}, {"temperature-departure", [] { return temperatureDeparture(); }}, {"visibility", [] { return visibilityRamp(); }},
            {"satellite-ir", [] { return satelliteIr(); }}, {"water-vapor", [] { return waterVapor(); }}, {"uv-index", [] { return uvIndex(); }}, {"current-speed", [] { return currentSpeed(); }}};
        return table;
    }

    // "VARIABLE@level" or "VARIABLE@level|detail"
    bool field(const QString& text, const std::string& key, GfsData::Want& want, std::string& why) {
        const auto at = text.indexOf('@');
        if (at <= 0 || at + 1 >= text.size()) {
            why = "a field is VARIABLE@level, not \"" + text.toStdString() + "\"";
            return false;
        }
        want = GfsData::Want{key, text.left(at).trimmed().toStdString(), text.mid(at + 1).section('|', 0, 0).trimmed().toStdString(), "", text.section('|', 1).trimmed().toStdString(), ""};
        const auto bad = [] (const std::string& s) { return s.empty() || s.size() > 60 || std::any_of(s.begin(), s.end(), [] (unsigned char c) { return c < 0x20 || c == '"' || c == '\\' || c == '<' || c == '>' || c == '`'; }); };
        if (bad(want.variable) || bad(want.level) || (!want.detail.empty() && bad(want.detail))) {
            why = "the field \"" + text.toStdString() + "\" has a character that is not allowed";
            return false;
        }
        return true;
    }

    std::function<GfsGrid::Grid(const Grids&)> afterScale(std::function<GfsGrid::Grid(const Grids&)> base, double scale, double offset) {
        if (scale == 1.0 && offset == 0.0) {
            return base;
        }
        return [base, scale, offset] (const Grids& g) { return GfsGrid::scaled(base(g), scale, offset); };
    }

    bool chart(const QJsonObject& o, Product& out, std::string& why) {
        out = Product{};
        out.id = o.value("id").toString().toStdString();
        out.source = o.value("model").toString().toStdString();
        const auto plain = [] (const std::string& s, size_t most) { return !s.empty() && s.size() <= most && std::all_of(s.begin(), s.end(), [] (unsigned char c) { return std::isalnum(c) || c == '_' || c == '-'; }); };
        if (!plain(out.id, 60)) {
            why = "an id is letters, digits, _ and - (60 at most)";
            return false;
        }
        if (GfsModels::find(out.source) == nullptr) {
            why = "the model \"" + out.source + "\" is not one of the program's";
            return false;
        }
        out.label = o.value("label").toString(QString::fromStdString(out.id)).left(120).toStdString();
        out.pickerGroup = o.value("group").toString("Your charts").left(60).toStdString();
        std::set<std::string> keys;
        const auto add = [&] (const QString& text, const std::string& key, std::string& fail) {
            GfsData::Want want;
            if (!field(text, key, want, fail)) {
                return false;
            }
            if (keys.insert(key).second) {
                out.wants.push_back(want);
            }
            return true;
        };
        const auto fill = o.value("fill").toObject();
        if (fill.isEmpty() && o.value("contours").toArray().isEmpty() && !o.contains("barbs")) {
            why = "a chart has a fill, lines or barbs";
            return false;
        }
        if (!fill.isEmpty()) {
            const double scale = fill.value("scale").toDouble(1.0), offset = fill.value("offset").toDouble(0.0);
            std::function<GfsGrid::Grid(const Grids&)> base;
            if (fill.contains("field")) {
                if (!add(fill.value("field").toString(), "f0", why)) {
                    return false;
                }
                base = [] (const Grids& g) { return GfsChart::detail::pick(g, "f0"); };
            } else {
                const auto kind = fill.contains("speed") ? "speed" : fill.contains("diff") ? "diff" : fill.contains("mean") ? "mean" : "";
                const auto pair = fill.value(kind).toArray();
                if (std::string{kind}.empty() || pair.size() != 2) {
                    why = "a fill is a \"field\", or \"speed\", \"diff\" or \"mean\" of two fields";
                    return false;
                }
                if (!add(pair[0].toString(), "f0", why) || !add(pair[1].toString(), "f1", why)) {
                    return false;
                }
                const std::string k{kind};
                base = [k] (const Grids& g) {
                    const auto& a = GfsChart::detail::pick(g, "f0"), & b = GfsChart::detail::pick(g, "f1");
                    if (k == "speed") {
                        return GfsGrid::speed(a, b);
                    }
                    if (k == "diff") {
                        return GfsGrid::difference(a, b);
                    }
                    return GfsGrid::scaled(GfsGrid::difference(a, GfsGrid::scaled(b, -1.0)), 0.5);
                };
            }
            out.fill = afterScale(base, scale, offset);
            const auto rampName = fill.value("ramp").toString("temperature").toStdString();
            const auto found = ramps().find(rampName);
            if (found == ramps().end()) {
                why = "the ramp \"" + rampName + "\" is not one of the program's (see docs/chart-files.md)";
                return false;
            }
            out.ramp = found->second();
            out.fillTitle = fill.value("title").toString("").left(80).toStdString();
            const auto quantity = fill.value("quantity").toString("other").toStdString();
            out.quantity = quantity == "temperature" ? GfsChart::Quantity::Temperature : quantity == "temperature-difference" ? GfsChart::Quantity::TemperatureDifference : quantity == "millimeters" ? GfsChart::Quantity::Millimeters
                         : quantity == "centimeters" ? GfsChart::Quantity::Centimeters : quantity == "meters" ? GfsChart::Quantity::Meters : GfsChart::Quantity::Other;
            out.legendStep = fill.value("legend_step").toDouble(0.0);
        } else {
            out.linesOnly = true;
        }
        int index = 0;
        for (const auto& value : o.value("contours").toArray()) {
            const auto c = value.toObject();
            const auto key = "c" + std::to_string(index++);
            if (!add(c.value("field").toString(), key, why)) {
                return false;
            }
            GfsChart::ContourSet set;
            set.key = key;
            set.scale = c.value("scale").toDouble(1.0);
            set.interval = c.value("interval").toDouble(0.0);
            set.base = c.value("base").toDouble(0.0);
            if (!(set.interval > 0.0) || !std::isfinite(set.interval) || !std::isfinite(set.scale) || set.scale == 0.0) {
                why = "a contour has an \"interval\" above 0 and a \"scale\" that is not 0";
                return false;
            }
            set.title = c.value("title").toString().left(80).toStdString();
            const QColor color{c.value("color").toString("#202020")};
            set.color = color.isValid() ? color : QColor{32, 32, 32};
            set.dashed = c.value("dashed").toBool(false);
            set.highsAndLows = c.value("highs_lows").toBool(false);
            set.width = std::clamp(c.value("width").toDouble(1.0), 0.5, 4.0);
            out.contours.push_back(set);
        }
        if (o.contains("barbs")) {
            const auto b = o.value("barbs").toObject();
            if (!add(b.value("u").toString(), "bu", why) || !add(b.value("v").toString(), "bv", why)) {
                return false;
            }
            out.barbU = "bu";
            out.barbV = "bv";
        }
        if (out.wants.size() > 12) {
            why = "a chart reads twelve fields at most";
            return false;
        }
        return true;
    }
}

bool UserCharts::parse(const QByteArray& json, std::vector<Product>& out, std::string& why) {
    if (json.isEmpty() || json.size() > maxBytes) {
        why = json.isEmpty() ? "the file is empty" : "the file is over 128 KB";
        return false;
    }
    QJsonParseError error;
    const auto doc = QJsonDocument::fromJson(json, &error);
    if (doc.isNull() || !doc.isObject()) {
        why = "not JSON: " + error.errorString().toStdString();
        return false;
    }
    if (doc.object().value("version").toInt() != 1) {
        why = "version is not 1";
        return false;
    }
    const auto charts = doc.object().value("charts").toArray();
    if (charts.isEmpty() || charts.size() > 50) {
        why = "one to fifty charts";
        return false;
    }
    std::vector<Product> made;
    std::set<std::string> seen;
    for (const auto& value : charts) {
        Product p;
        std::string reason;
        if (!chart(value.toObject(), p, reason)) {
            why = (p.id.empty() ? "a chart" : "the chart " + p.id) + ": " + reason;
            return false;
        }
        if (!seen.insert(p.source + "/" + p.id).second) {
            why = "the chart " + p.id + " is in the file twice";
            return false;
        }
        made.push_back(std::move(p));
    }
    for (auto& p : made) {
        out.push_back(std::move(p));
    }
    return true;
}

void UserCharts::load(const std::string& folder, std::vector<Product>& out, std::vector<std::string>& problems) {
    if (folder.empty()) {
        return;
    }
    const QDir dir{QString::fromStdString(folder)};
    for (const auto& info : dir.entryInfoList({"*.chart.json"}, QDir::Files, QDir::Name)) {
        QFile file{info.absoluteFilePath()};
        if (!file.open(QIODevice::ReadOnly)) {
            problems.push_back(info.fileName().toStdString() + ": could not be read");
            continue;
        }
        std::string why;
        if (!parse(file.read(maxBytes + 1), out, why)) {
            problems.push_back(info.fileName().toStdString() + ": " + why);
        }
    }
}

const std::vector<std::string>& UserCharts::problems() {
    return problemList;
}

std::vector<std::string> UserCharts::rampNames() {
    std::vector<std::string> names;
    for (const auto& [name, make] : ramps()) {
        names.push_back(name);
    }
    return names;
}

std::string UserCharts::folder() {
    return GfsModels::userFolderPath();
}

void userChartsRecordProblems(const std::vector<std::string>& problems) {   // (called by GfsChart::products())
    problemList = problems;
}

UserCharts::Starter UserCharts::starter(const std::string& model, const std::string& variable, const std::string& level, const std::string& detail) {
    struct Rule {
        const char * pattern, * ramp, * quantity, * title;
        double scale, step;
    };
    // the variable (NOAA's name) -> the fill's defaults; the scale turns the grid's unit into the ramp's (as in tools/make_chart.py)
    static const Rule rules[] = {
        {"^(TMP|TMAX|TMIN|APTMP)$", "temperature", "temperature", "Temperature", 1, 5},
        {"^DPT$", "dewpoint", "temperature", "Dewpoint", 1, 5},
        {"^(RH|MINRH|MAXRH)$", "humidity", "", "Relative humidity (%)", 1, 10},
        {"^PWAT$", "precipitable-water", "millimeters", "Precipitable water", 1, 10},
        {"^(APCP|ACPCP|NCPCP)$", "precipitation", "millimeters", "Precipitation", 1, 0},
        {"^CAPE$", "cape", "", "CAPE (J/kg)", 1, 1000},
        {"^(TCDC|LCDC|MCDC|HCDC)$", "cloud-cover", "", "Cloud cover (%)", 1, 10},
        {"^(REFC|REFD|MAXREF)$", "reflectivity", "", "Reflectivity (dBZ)", 1, 10},
        {"^(GUST|WIND)$", "wind", "", "Wind (kt)", 1.94384, 20},
        {"^(PRMSL|MSLET|MSLMA|PRES)$", "sea-level-pressure", "", "Pressure (mb)", 0.01, 10},
        {"^HLCY$", "helicity", "", "Storm-relative helicity (m2/s2)", 1, 0},
        {"^(MNUPHL|MXUPHL|UPHL)$", "updraft-helicity", "", "Updraft helicity (m2/s2)", 1, 0},
        {"^(ABSV|RELV)$", "vorticity", "", "Vorticity (1e-5 /s)", 100000, 10},
        {"^(WEASD|SNOD)$", "snowfall", "", "Snow (cm)", 100, 0},
        {"^SOILW$", "humidity", "", "Soil moisture (%)", 100, 0},
        {"^TSOIL$", "temperature", "temperature", "Soil temperature", 1, 5},
        {"^(HTSGW|WVHGT|SWELL)$", "wave-height", "", "Wave height (m)", 1, 0},
        {"^WTMP$", "sea-surface-temperature", "temperature", "Sea surface temperature", 1, 0},
        {"^VIS$", "visibility", "", "Visibility (mi)", 0.000621371, 0},
    };
    Starter out;
    const auto field = variable + "@" + level + (detail.empty() ? "" : "|" + detail);
    QJsonObject fill;
    fill["field"] = QString::fromStdString(field);
    const Rule * found = nullptr;
    for (const auto& r : rules) {
        if (std::regex_match(variable, std::regex{r.pattern})) {
            found = &r;
            break;
        }
    }
    if (found != nullptr) {
        fill["ramp"] = found->ramp;
        fill["title"] = found->title;
        if (found->quantity[0] != '\0') {
            fill["quantity"] = found->quantity;
        }
        if (found->scale != 1.0) {
            fill["scale"] = found->scale;
        }
        if (found->step > 0) {
            fill["legend_step"] = found->step;
        }
    } else {
        fill["ramp"] = "probability";
        fill["title"] = QString::fromStdString(variable + " " + level);
        out.note = "no ramp is known for " + variable + ": choose one (the names are in chart-files.md) and check the field's units";
    }
    std::string id = variable + "_" + level;
    for (auto& c : id) {
        c = std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-' ? static_cast<char>(std::tolower(static_cast<unsigned char>(c))) : '_';
    }
    while (!id.empty() && id.back() == '_') {
        id.pop_back();
    }
    std::string squeezed;
    for (const char c : id) {
        if (!(c == '_' && !squeezed.empty() && squeezed.back() == '_')) {
            squeezed += c;
        }
    }
    out.id = squeezed.empty() ? "mychart" : squeezed;
    QJsonObject chart;
    chart["id"] = QString::fromStdString(out.id);
    chart["model"] = QString::fromStdString(model);
    chart["label"] = QString::fromStdString(variable + "@" + level);
    chart["group"] = "Your charts";
    chart["fill"] = fill;
    QJsonObject root;
    root["version"] = 1;
    root["charts"] = QJsonArray{chart};
    out.json = QJsonDocument{root}.toJson(QJsonDocument::Indented).toStdString();
    return out;
}

bool UserCharts::writeStarter(const Starter& chart, std::string& pathOut, std::string& why) {
    const auto dir = folder();
    if (dir.empty()) {
        why = "the folder of your own models is not set";
        return false;
    }
    QDir{}.mkpath(QString::fromStdString(dir));
    std::string name = chart.id;
    for (int n = 2; QFile::exists(QString::fromStdString(dir + "/" + name + ".chart.json")) && n < 100; n++) {
        name = chart.id + "_" + std::to_string(n);
    }
    // the chart's id inside the file follows the file's name, so two starters of one field are two charts
    auto text = chart.json;
    if (name != chart.id) {
        const auto at = text.find("\"id\": \"" + chart.id + "\"");
        if (at != std::string::npos) {
            text.replace(at, chart.id.size() + 8, "\"id\": \"" + name + "\"");
        }
    }
    QFile f{QString::fromStdString(dir + "/" + name + ".chart.json")};
    if (!f.open(QIODevice::WriteOnly)) {
        why = "could not write " + f.fileName().toStdString();
        return false;
    }
    f.write(QByteArray::fromStdString(text));
    pathOut = f.fileName().toStdString();
    return true;
}
