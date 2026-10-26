// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "gfs/GeosOpendap.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <ctime>
#include <map>
#include <mutex>
#include <QDateTime>
#include <QRegularExpression>
#include <QTimeZone>
#include "util/UtilityIO.h"

namespace {
    const std::string base{"https://opendap.nccs.nasa.gov/dods/GEOS-5/fp/0.25_deg/fcast/"};
    constexpr int nativeColumns = 1152, nativeRows = 721;   // 0.3125 degrees of longitude (from -180), 0.25 of latitude (from -90)

    struct Axis {
        long long start{0};   // the first time, seconds since 1970
        int stepMinutes{60};
        int count{0};
        std::time_t read{0};
        bool ok{false};
    };

    // a dataset's time axis, read once and kept ten minutes
    Axis axisOf(const std::string& dataset) {
        static std::mutex lock;
        static std::map<std::string, Axis> kept;
        {
            const std::lock_guard guard{lock};
            const auto found = kept.find(dataset);
            if (found != kept.end() && std::time(nullptr) - found->second.read < 600) {
                return found->second;
            }
        }
        Axis axis;
        axis.read = std::time(nullptr);
        const auto das = UtilityIO::downloadAsByteArray(dataset + ".das").toStdString();
        axis.ok = GeosOpendap::parseStart(das, axis.start, axis.stepMinutes, axis.count);
        const std::lock_guard guard{lock};
        kept[dataset] = axis;
        return axis;
    }
}

bool GeosOpendap::parseStart(const std::string& das, long long& startSeconds, int& stepMinutes, int& count) {
    const auto text = QString::fromStdString(das);
    // "grads_min "03:30z26oct2026"" or "06z26oct2026"; "grads_step "60mn""; "grads_size "33"" (the one of the time axis: the line after "time {")
    const auto timeBlock = text.mid(std::max(0, static_cast<int>(text.indexOf(QRegularExpression{"\\n\\s*time \\{"}))));
    const auto start = QRegularExpression{R"re(grads_min "([0-9]{2})(?::([0-9]{2}))?z([0-9]{2})([a-z]{3})([0-9]{4})")re"}.match(timeBlock);
    const auto step = QRegularExpression{R"re(grads_step "([0-9]+)(mn|hr)")re"}.match(timeBlock);
    const auto size = QRegularExpression{R"re(grads_size "([0-9]+)")re"}.match(timeBlock);
    if (!start.hasMatch() || !step.hasMatch() || !size.hasMatch()) {
        return false;
    }
    static const QStringList months{"jan", "feb", "mar", "apr", "may", "jun", "jul", "aug", "sep", "oct", "nov", "dec"};
    const int month = static_cast<int>(months.indexOf(start.captured(4))) + 1;
    if (month < 1) {
        return false;
    }
    const QDateTime when{QDate{start.captured(5).toInt(), month, start.captured(3).toInt()}, QTime{start.captured(1).toInt(), start.captured(2).toInt()}, QTimeZone::utc()};
    if (!when.isValid()) {
        return false;
    }
    startSeconds = when.toSecsSinceEpoch();
    stepMinutes = step.captured(1).toInt() * (step.captured(2) == "hr" ? 60 : 1);
    count = size.captured(1).toInt();
    return stepMinutes > 0 && count > 0;
}

bool GeosOpendap::parseDods(const std::string& reply, size_t expected, std::vector<float>& values) {
    const auto marker = reply.find("Data:\n");
    if (marker == std::string::npos) {
        return false;
    }
    const size_t at = marker + 6;
    if (reply.size() < at + 8 + expected * 4) {
        return false;
    }
    const auto word = [&reply] (size_t i) {
        const auto * p = reinterpret_cast<const unsigned char *>(reply.data() + i);
        return (static_cast<uint32_t>(p[0]) << 24) | (static_cast<uint32_t>(p[1]) << 16) | (static_cast<uint32_t>(p[2]) << 8) | p[3];
    };
    if (word(at) != expected || word(at + 4) != expected) {   // the array's length, given twice
        return false;
    }
    values.resize(expected);
    for (size_t i = 0; i < expected; i++) {
        const uint32_t w = word(at + 8 + i * 4);
        float v;
        std::memcpy(&v, &w, 4);
        values[i] = std::abs(v) >= 1.0e14f || v != v ? std::nanf("") : v;   // (the server's "undefined" is 1e15 in the file, 1e34 in a GrADS reply: under the ground, a pressure level has none)
    }
    return true;
}

bool GeosOpendap::read(const GfsData::Run& run, int forecastHour, const std::string& file, const std::string& variable, GfsGrid::Grid& out, std::string& error) {
    if (file != "tavg1_2d_slv_Nx" || variable.empty()) {   // (the other collections are read as files)
        error = "not a collection read this way";
        return false;
    }
    std::string name = variable;
    std::transform(name.begin(), name.end(), name.begin(), [] (unsigned char c) { return static_cast<char>(std::tolower(c)); });
    const std::string dataset = base + file + "/" + file + "." + run.date + "_" + run.cycle;
    const auto axis = axisOf(dataset);
    if (!axis.ok) {
        error = "the GrADS Data Server has no dataset for this run";
        return false;
    }
    auto t = QDateTime::fromString(QString::fromStdString(run.id()), "yyyyMMddHH");
    t.setTimeZone(QTimeZone::utc());
    const long long valid = t.toSecsSinceEpoch() + static_cast<long long>(forecastHour) * 3600 + 30 * 60;   // (an hour's average is stamped in the middle of it)
    const long long offset = valid - axis.start;
    if (offset < 0 || offset % (static_cast<long long>(axis.stepMinutes) * 60) != 0 || offset / (static_cast<long long>(axis.stepMinutes) * 60) >= axis.count) {
        error = "the hour is not in the server's dataset of this run";
        return false;
    }
    const long long index = offset / (static_cast<long long>(axis.stepMinutes) * 60);
    const std::string request = dataset + ".dods?" + name + "[" + std::to_string(index) + ":" + std::to_string(index) + "][0:" + std::to_string(nativeRows - 1) + "][0:" + std::to_string(nativeColumns - 1) + "]";
    const auto reply = UtilityIO::downloadAsByteArray(request).toStdString();
    std::vector<float> native;
    if (!parseDods(reply, static_cast<size_t>(nativeRows) * nativeColumns, native)) {
        error = "the GrADS Data Server did not give " + name;
        return false;
    }
    // onto the model's grid: 0.25 degrees over the world, the cell centres at -179.875 and 89.875 (the way the file is warped); bilinear between the native points
    GfsGrid::Grid g;
    g.columns = 1440;
    g.rows = 720;
    g.lon0 = -179.875;
    g.lat0 = 89.875;
    g.step = 0.25;
    g.values.assign(static_cast<size_t>(g.columns) * static_cast<size_t>(g.rows), std::nanf(""));
    for (int r = 0; r < g.rows; r++) {
        const double lat = g.lat0 - r * g.step;
        const double fy = std::clamp((lat + 90.0) / 0.25, 0.0, static_cast<double>(nativeRows - 1));
        const int y0 = std::min(static_cast<int>(fy), nativeRows - 2);
        const double wy = fy - y0;
        for (int c = 0; c < g.columns; c++) {
            const double lon = g.lon0 + c * g.step;
            double fx = (lon + 180.0) / 0.3125;
            fx = std::fmod(fx + nativeColumns, nativeColumns);
            const int x0 = static_cast<int>(fx) % nativeColumns, x1 = (x0 + 1) % nativeColumns;
            const double wx = fx - std::floor(fx);
            const float a = native[static_cast<size_t>(y0) * nativeColumns + x0], b = native[static_cast<size_t>(y0) * nativeColumns + x1];
            const float c0 = native[static_cast<size_t>(y0 + 1) * nativeColumns + x0], d = native[static_cast<size_t>(y0 + 1) * nativeColumns + x1];
            if (!std::isfinite(a) || !std::isfinite(b) || !std::isfinite(c0) || !std::isfinite(d)) {
                continue;
            }
            g.values[static_cast<size_t>(r) * static_cast<size_t>(g.columns) + static_cast<size_t>(c)] =
                static_cast<float>((a * (1 - wx) + b * wx) * (1 - wy) + (c0 * (1 - wx) + d * wx) * wy);
        }
    }
    out = std::move(g);
    return true;
}
