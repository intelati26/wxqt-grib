// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "space/UtilitySpace.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <QByteArray>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include "obs/UtilityMetarCache.h"

namespace {
    long timeOf(const QJsonObject& o, const char * key) {
        return UtilityMetarCache::parseTime(o.value(key).toString().toStdString());
    }

    double number(const QJsonObject& o, const char * key) {
        const auto v = o.value(key);
        return v.isDouble() ? v.toDouble() : UtilitySpace::missing;
    }

    int scaleOf(const QJsonValue& v) {
        const auto text = v.toString();
        bool ok = false;
        const int n = text.toInt(&ok);
        return ok ? n : -1;
    }
}

vector<UtilitySpace::ScaleDay> UtilitySpace::parseScales(const string& json) {
    vector<ScaleDay> days;
    const auto root = QJsonDocument::fromJson(QByteArray::fromStdString(json)).object();
    for (const char * key : {"0", "1", "2", "3"}) {
        if (!root.contains(key)) {
            continue;
        }
        const auto day = root.value(key).toObject();
        ScaleDay d;
        d.date = day.value("DateStamp").toString().toStdString();
        const auto r = day.value("R").toObject();
        const auto s = day.value("S").toObject();
        const auto g = day.value("G").toObject();
        d.r = scaleOf(r.value("Scale"));
        d.s = scaleOf(s.value("Scale"));
        d.g = scaleOf(g.value("Scale"));
        d.rMinor = scaleOf(r.value("MinorProb"));
        d.rMajor = scaleOf(r.value("MajorProb"));
        d.sProb = scaleOf(s.value("Prob"));
        days.push_back(std::move(d));
    }
    return days;
}

vector<UtilitySpace::Point> UtilitySpace::parseKp(const string& json) {
    vector<Point> points;
    for (const auto& value : QJsonDocument::fromJson(QByteArray::fromStdString(json)).array()) {
        const auto o = value.toObject();
        Point p;
        p.seconds = timeOf(o, "time_tag");
        p.value = o.contains("kp") ? number(o, "kp") : number(o, "Kp");
        if (p.seconds <= 0 || !has(p.value)) {
            continue;
        }
        const auto kind = o.value("observed").toString();
        p.kind = kind == "observed" || !o.contains("observed") ? 0 : kind == "estimated" ? 1 : 2;
        points.push_back(p);
    }
    return points;
}

vector<UtilitySpace::Point> UtilitySpace::parseXray(const string& json, int stepMinutes) {
    vector<Point> points;
    long last = 0;
    for (const auto& value : QJsonDocument::fromJson(QByteArray::fromStdString(json)).array()) {
        const auto o = value.toObject();
        if (o.value("energy").toString() != "0.1-0.8nm") {
            continue;
        }
        Point p;
        p.seconds = timeOf(o, "time_tag");
        p.value = number(o, "flux");
        if (p.seconds <= 0 || !has(p.value) || p.value <= 0.0) {
            continue;
        }
        if (last != 0 && p.seconds - last < stepMinutes * 60L) {
            // within a step: keep the larger, so a flare's peak is not thinned away
            if (p.value > points.back().value) {
                points.back() = p;
            }
            continue;
        }
        last = p.seconds;
        points.push_back(p);
    }
    std::sort(points.begin(), points.end(), [] (const Point& a, const Point& b) { return a.seconds < b.seconds; });
    return points;
}

vector<UtilitySpace::Point> UtilitySpace::parseWind(const string& json, int stepMinutes) {
    // every record of every source (the "active" flag lags: the newest records can come from a source that is not marked active), by time, thinned to one a step
    vector<Point> all;
    for (const auto& value : QJsonDocument::fromJson(QByteArray::fromStdString(json)).array()) {
        const auto o = value.toObject();
        Point p;
        p.seconds = timeOf(o, "time_tag");
        p.value = number(o, "proton_speed");
        p.second = number(o, "proton_density");
        if (p.seconds > 0 && has(p.value)) {
            all.push_back(p);
        }
    }
    std::stable_sort(all.begin(), all.end(), [] (const Point& a, const Point& b) { return a.seconds < b.seconds; });
    vector<Point> points;
    for (const auto& p : all) {
        if (points.empty() || p.seconds - points.back().seconds >= stepMinutes * 60L) {
            points.push_back(p);
        }
    }
    return points;
}

vector<UtilitySpace::Point> UtilitySpace::parseMag(const string& json, int stepMinutes) {
    // every record of every source (the "active" flag lags: the newest records can come from a source that is not marked active), by time, thinned to one a step
    vector<Point> all;
    for (const auto& value : QJsonDocument::fromJson(QByteArray::fromStdString(json)).array()) {
        const auto o = value.toObject();
        Point p;
        p.seconds = timeOf(o, "time_tag");
        p.value = number(o, "bt");
        p.second = number(o, "bz_gsm");
        if (p.seconds > 0 && has(p.value)) {
            all.push_back(p);
        }
    }
    std::stable_sort(all.begin(), all.end(), [] (const Point& a, const Point& b) { return a.seconds < b.seconds; });
    vector<Point> points;
    for (const auto& p : all) {
        if (points.empty() || p.seconds - points.back().seconds >= stepMinutes * 60L) {
            points.push_back(p);
        }
    }
    return points;
}

UtilitySpace::Flare UtilitySpace::parseFlare(const string& json) {
    Flare f;
    const auto array = QJsonDocument::fromJson(QByteArray::fromStdString(json)).array();
    if (array.isEmpty()) {
        return f;
    }
    const auto o = array.first().toObject();
    f.current = o.value("current_class").toString().toStdString();
    f.maxClass = o.value("max_class").toString().toStdString();
    f.maxTime = o.value("max_time").toString().toStdString();
    f.beginTime = o.value("begin_time").toString().toStdString();
    f.endTime = o.value("end_time").toString().toStdString();
    return f;
}

vector<string> UtilitySpace::parseAlerts(const string& json, size_t count) {
    vector<string> lines;
    for (const auto& value : QJsonDocument::fromJson(QByteArray::fromStdString(json)).array()) {
        if (lines.size() >= count) {
            break;
        }
        const auto o = value.toObject();
        const auto message = o.value("message").toString();
        // the line after the serial number and issue time: "ALERT: ...", "WARNING: ...", "WATCH: ...", "SUMMARY: ...", "CONTINUED ALERT: ..."
        QString headline;
        for (const auto& line : message.split('\n')) {
            const auto trimmed = line.trimmed();
            if (trimmed.startsWith("ALERT:") || trimmed.startsWith("WARNING:") || trimmed.startsWith("WATCH:") || trimmed.startsWith("SUMMARY:") || trimmed.startsWith("EXTENDED WARNING:") ||
                trimmed.startsWith("CONTINUED ALERT:") || trimmed.startsWith("CANCEL")) {
                headline = trimmed;
                break;
            }
        }
        if (headline.isEmpty()) {
            continue;
        }
        const auto when = o.value("issue_datetime").toString().left(16);
        lines.push_back((when + "Z  " + headline).toStdString());
    }
    return lines;
}

vector<UtilitySpace::Point> UtilitySpace::parseFlux(const string& json, const string& energy) {
    vector<Point> points;
    for (const auto& value : QJsonDocument::fromJson(QByteArray::fromStdString(json)).array()) {
        const auto o = value.toObject();
        if (o.value("energy").toString().toStdString() != energy) {
            continue;
        }
        Point p;
        p.seconds = timeOf(o, "time_tag");
        p.value = number(o, "flux");
        if (p.seconds > 0 && has(p.value) && p.value > 0.0) {
            points.push_back(p);
        }
    }
    std::sort(points.begin(), points.end(), [] (const Point& a, const Point& b) { return a.seconds < b.seconds; });
    return points;
}

namespace {
    // "2026-04" -> seconds at the middle of that month
    long monthOf(const QString& tag) {
        const auto parts = tag.split('-');
        if (parts.size() < 2) {
            return 0;
        }
        return UtilityMetarCache::parseTime((parts[0] + "-" + parts[1] + "-15T00:00:00").toStdString());
    }
}

vector<UtilitySpace::Point> UtilitySpace::parseCycleObserved(const string& json, int fromYear) {
    vector<Point> points;
    for (const auto& value : QJsonDocument::fromJson(QByteArray::fromStdString(json)).array()) {
        const auto o = value.toObject();
        const auto tag = o.value("time-tag").toString();
        if (tag.left(4).toInt() < fromYear) {
            continue;
        }
        Point p;
        p.seconds = monthOf(tag);
        const double ssn = number(o, "ssn");
        const double smooth = number(o, "smoothed_ssn");
        p.value = ssn >= 0.0 ? ssn : missing;      // -1 marks "none"
        p.second = smooth >= 0.0 ? smooth : missing;
        if (p.seconds > 0 && (has(p.value) || has(p.second))) {
            points.push_back(p);
        }
    }
    return points;
}

vector<UtilitySpace::Band> UtilitySpace::parseCyclePredicted(const string& json) {
    vector<Band> bands;
    for (const auto& value : QJsonDocument::fromJson(QByteArray::fromStdString(json)).array()) {
        const auto o = value.toObject();
        Band b;
        b.seconds = monthOf(o.value("time-tag").toString());
        b.mid = number(o, "predicted_ssn");
        b.low = number(o, "low_ssn");
        b.high = number(o, "high_ssn");
        if (b.seconds > 0 && has(b.mid)) {
            bands.push_back(b);
        }
    }
    return bands;
}

float UtilitySpace::Ovation::at(double lat, double lon) const {
    if (!ok) {
        return 0.0f;
    }
    int column = static_cast<int>(std::lround(lon));
    column = ((column % 360) + 360) % 360;
    const int row = std::clamp(static_cast<int>(std::lround(lat)) + 90, 0, 180);
    return grid[static_cast<size_t>(row) * 360 + static_cast<size_t>(column)];
}

UtilitySpace::Ovation UtilitySpace::parseOvation(const string& json) {
    Ovation o;
    const auto root = QJsonDocument::fromJson(QByteArray::fromStdString(json)).object();
    o.observation = root.value("Observation Time").toString().toStdString();
    o.forecast = root.value("Forecast Time").toString().toStdString();
    o.grid.assign(360 * 181, 0.0f);
    int filled = 0;
    for (const auto& value : root.value("coordinates").toArray()) {
        const auto cell = value.toArray();
        if (cell.size() < 3) {
            continue;
        }
        const int lon = ((cell[0].toInt() % 360) + 360) % 360;
        const int lat = cell[1].toInt();
        if (lat < -90 || lat > 90) {
            continue;
        }
        o.grid[static_cast<size_t>(lat + 90) * 360 + static_cast<size_t>(lon)] = static_cast<float>(cell[2].toDouble());
        filled++;
    }
    o.ok = filled > 10000;
    return o;
}

string UtilitySpace::flareClass(double flux) {
    if (!has(flux) || flux <= 0.0) {
        return {};
    }
    static const char letters[] = {'A', 'B', 'C', 'M', 'X'};
    const int exponent = static_cast<int>(std::floor(std::log10(flux)));
    const int index = std::clamp(exponent + 8, 0, 4);   // 1e-8 is A, 1e-7 B, 1e-6 C, 1e-5 M, 1e-4 X
    const double mantissa = flux / std::pow(10.0, index - 8);
    char buffer[16];
    std::snprintf(buffer, sizeof buffer, "%c%.1f", letters[index], mantissa);
    return buffer;
}

int UtilitySpace::kpScale(double kp) {
    return kp < 4.67 ? 0 : static_cast<int>(std::clamp(std::lround(kp) - 4, 1L, 5L));
}
