// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "gfs/GeosOpendap.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <map>
#include <mutex>
#include <vector>
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

bool GeosOpendap::read(const GfsData::Run& run, int forecastHour, const std::string& file, const std::string& variable, const std::string& region, GfsGrid::Grid& out, std::string& error) {
    const bool aerosol = file == "tavg3_2d_aer_Nx";   // the 3 hour averages of the aerosol species (260 MB a file; stamped in the middle of the 3 hours)
    if ((file != "tavg1_2d_slv_Nx" && !aerosol) || variable.empty()) {   // (the other collections are read as files)
        error = "not a collection read this way";
        return false;
    }
    double west = -180.0, south = -90.0, east = 180.0, north = 90.0;
    const bool part = !region.empty() && std::sscanf(region.c_str(), "%lf,%lf,%lf,%lf", &west, &south, &east, &north) == 4 && west < east && south < north;
    std::string name = variable;
    std::transform(name.begin(), name.end(), name.begin(), [] (unsigned char c) { return static_cast<char>(std::tolower(c)); });
    auto t = QDateTime::fromString(QString::fromStdString(run.id()), "yyyyMMddHH");
    t.setTimeZone(QTimeZone::utc());
    const long long valid = t.toSecsSinceEpoch() + static_cast<long long>(forecastHour) * 3600 + (aerosol ? -90 : 30) * 60;   // (an average is stamped in the middle of its hour: the 3 hour ones end at the hour)
    // the datasets to try: the run asked for; for a part of the world also the newest 00Z or 12Z run before it, and the one before that (the server keeps only those two a day)
    std::vector<std::string> datasets{base + file + "/" + file + "." + run.date + "_" + run.cycle};
    if (part) {
        auto later = t;
        for (int k = 0; k < 3; k++) {
            later = later.addSecs(-3600 * (later.time().hour() % 12 == 0 ? 12 : later.time().hour() % 12));   // the 12Z or 00Z before
            datasets.push_back(base + file + "/" + file + "." + later.toString("yyyyMMdd").toStdString() + "_" + later.toString("HH").toStdString());
        }
    }
    std::string dataset;
    Axis axis;
    long long index = 0;
    for (const auto& candidate : datasets) {
        axis = axisOf(candidate);
        if (!axis.ok) {
            continue;
        }
        const long long offset = valid - axis.start;
        const long long step = static_cast<long long>(axis.stepMinutes) * 60;
        if (offset < 0 || offset % step != 0 || offset / step >= axis.count) {
            continue;
        }
        dataset = candidate;
        index = offset / step;
        break;
    }
    if (dataset.empty()) {
        error = "the GrADS Data Server has no dataset with this hour" + std::string{part ? " (this run or the 00Z / 12Z before it)" : " of this run"};
        return false;
    }
    // the native cells to ask for: the whole grid, or the box and a degree round it
    int x0 = 0, x1 = nativeColumns - 1, y0 = 0, y1 = nativeRows - 1;
    if (part) {
        constexpr double margin = 1.0;
        x0 = std::clamp(static_cast<int>(std::floor((std::max(-180.0, west - margin) + 180.0) / 0.3125)), 0, nativeColumns - 1);
        x1 = std::clamp(static_cast<int>(std::ceil((std::min(179.6875, east + margin) + 180.0) / 0.3125)), x0 + 1, nativeColumns - 1);
        y0 = std::clamp(static_cast<int>(std::floor((std::max(-90.0, south - margin) + 90.0) / 0.25)), 0, nativeRows - 2);
        y1 = std::clamp(static_cast<int>(std::ceil((std::min(90.0, north + margin) + 90.0) / 0.25)), y0 + 1, nativeRows - 1);
    }
    const int columnsAsked = x1 - x0 + 1, rowsAsked = y1 - y0 + 1;
    const std::string request = dataset + ".dods?" + name + "[" + std::to_string(index) + ":" + std::to_string(index) + "][" + std::to_string(y0) + ":" + std::to_string(y1) + "][" + std::to_string(x0) + ":" + std::to_string(x1) + "]";
    const auto reply = UtilityIO::downloadAsByteArray(request).toStdString();
    std::vector<float> native;
    if (!parseDods(reply, static_cast<size_t>(rowsAsked) * static_cast<size_t>(columnsAsked), native)) {
        error = "the GrADS Data Server did not give " + name;
        return false;
    }
    // onto the model's grid: 0.25 degrees, the cell centres at -179.875 and 89.875 (the way the file is warped), the whole world or the part; bilinear between the native points
    GfsGrid::Grid g;
    int c0 = 0, c1 = 1439, r0 = 0, r1 = 719;
    if (part) {
        c0 = std::max(0, static_cast<int>(std::ceil((-180.0 + x0 * 0.3125 + 0.3125 - -179.875) / 0.25)));   // (inside the native cells asked for, so that the interpolation has both neighbours)
        c1 = std::min(1439, static_cast<int>(std::floor((-180.0 + x1 * 0.3125 - 0.3125 - -179.875) / 0.25)));
        r0 = std::max(0, static_cast<int>(std::ceil((89.875 - (-90.0 + y1 * 0.25 - 0.25)) / 0.25)));
        r1 = std::min(719, static_cast<int>(std::floor((89.875 - (-90.0 + y0 * 0.25 + 0.25)) / 0.25)));
        if (c1 <= c0 || r1 <= r0) {
            error = "the part asked for is too small";
            return false;
        }
    }
    g.columns = c1 - c0 + 1;
    g.rows = r1 - r0 + 1;
    g.lon0 = -179.875 + c0 * 0.25;
    g.lat0 = 89.875 - r0 * 0.25;
    g.step = 0.25;
    g.values.assign(static_cast<size_t>(g.columns) * static_cast<size_t>(g.rows), std::nanf(""));
    for (int r = 0; r < g.rows; r++) {
        const double lat = g.lat0 - r * g.step;
        const double fy = std::clamp((lat + 90.0) / 0.25 - y0, 0.0, static_cast<double>(rowsAsked - 1));
        const int ya = std::min(static_cast<int>(fy), rowsAsked - 2);
        const double wy = fy - ya;
        for (int c = 0; c < g.columns; c++) {
            const double lon = g.lon0 + c * g.step;
            double fx = (lon + 180.0) / 0.3125;
            if (!part) {
                fx = std::fmod(fx + nativeColumns, nativeColumns);
            }
            fx -= x0;
            if (fx < 0.0 || fx > columnsAsked - 1) {
                continue;
            }
            const int xa = std::min(static_cast<int>(fx), columnsAsked - 1), xb = part ? std::min(xa + 1, columnsAsked - 1) : (xa + 1) % nativeColumns;
            const double wx = fx - std::floor(fx);
            const float a = native[static_cast<size_t>(ya) * columnsAsked + xa], b = native[static_cast<size_t>(ya) * columnsAsked + xb];
            const float c0v = native[static_cast<size_t>(ya + 1) * columnsAsked + xa], d = native[static_cast<size_t>(ya + 1) * columnsAsked + xb];
            if (!std::isfinite(a) || !std::isfinite(b) || !std::isfinite(c0v) || !std::isfinite(d)) {
                continue;
            }
            g.values[static_cast<size_t>(r) * static_cast<size_t>(g.columns) + static_cast<size_t>(c)] =
                static_cast<float>((a * (1 - wx) + b * wx) * (1 - wy) + (c0v * (1 - wx) + d * wx) * wy);
        }
    }
    out = std::move(g);
    return true;
}
