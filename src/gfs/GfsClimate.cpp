// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "gfs/GfsClimate.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <future>
#include <map>
#include <mutex>
#include <set>
#include <thread>
#include <limits>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QUuid>
#include "util/PermanentCache.h"

namespace {
    constexpr int pressureLevels[17] = {1000, 925, 850, 700, 600, 500, 400, 300, 250, 200, 150, 100, 70, 50, 30, 20, 10};
}

const int * GfsClimate::levels() {
    return pressureLevels;
}

GfsClimate::Field GfsClimate::height(int hPa) {
    for (int i = 0; i < 17; i++) {
        if (pressureLevels[i] == hPa) {
            return {"pressure/hgt.day.ltm.1991-2020.nc", "hgt", i};
        }
    }
    return {};
}

GfsClimate::Field GfsClimate::temperature(int hPa) {
    for (int i = 0; i < 17; i++) {
        if (pressureLevels[i] == hPa) {
            return {"pressure/air.day.ltm.1991-2020.nc", "air", i};
        }
    }
    return {};
}

GfsClimate::Field GfsClimate::precipitableWater() {
    return {"surface/pr_wtr.eatm.day.ltm.1991-2020.nc", "pr_wtr", -1};
}

GfsClimate::Field GfsClimate::seaLevelPressure() {
    return {"surface/slp.day.ltm.1991-2020.nc", "slp", -1};
}

int GfsClimate::dayIndex(int year, int month, int day) {
    static const int before[12] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
    const bool leap = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
    if (leap && month == 2 && day == 29) {
        day = 28;
    }
    // counted from the month and day, so 1 March is day 59 in every year
    return std::clamp(before[month - 1] + day - 1, 0, 364);
}

long GfsClimate::band(const Field& field, int day) {
    return field.level < 0 ? day + 1 : static_cast<long>(day) * 17 + field.level + 1;
}

void GfsClimate::anchors(int day, int& first, int& second, double& weight) {
    first = day / 4 * 4;
    second = first + 4;
    weight = (day - first) / 4.0;
    if (second > 364) {   // 364 is the last stored day; the one after it is 1 January again, a day later
        weight = (day - first) / 1.0;
        second = 0;
    }
}

std::string GfsClimate::pack(const GfsGrid::Grid& grid) {
    double lo = std::numeric_limits<double>::max(), hi = std::numeric_limits<double>::lowest(), sum = 0.0;
    size_t count = 0;
    for (float v : grid.values) {
        if (std::isfinite(v) && std::abs(v) < 1e30f) {
            lo = std::min<double>(lo, v);
            hi = std::max<double>(hi, v);
            sum += v;
            count++;
        }
    }
    if (count == 0) {
        lo = hi = 0.0;
    }
    const double fill = count ? sum / static_cast<double>(count) : 0.0;
    const double scale = hi > lo ? (hi - lo) / 65535.0 : 1.0;
    std::string out(4 + 4 + 8 * 5 + grid.values.size() * 2, '\0');
    const int32_t columns = grid.columns, rows = grid.rows;
    char * at = out.data();
    std::memcpy(at, &columns, 4);
    std::memcpy(at + 4, &rows, 4);
    const double header[5] = {grid.lon0, grid.lat0, grid.step, lo, scale};
    std::memcpy(at + 8, header, sizeof header);
    uint16_t previous = 0;
    char * steps = at + 8 + sizeof header;
    for (size_t i = 0; i < grid.values.size(); i++) {
        const float v = grid.values[i];
        const double value = std::isfinite(v) && std::abs(v) < 1e30f ? v : fill;
        const auto q = static_cast<uint16_t>(std::lround((value - lo) / scale));
        const uint16_t step = static_cast<uint16_t>(q - previous);   // modulo 65536: exact when added back
        std::memcpy(steps + i * 2, &step, 2);
        previous = q;
    }
    return out;
}

bool GfsClimate::unpack(const std::string& bytes, GfsGrid::Grid& out) {
    constexpr size_t header = 8 + 8 * 5;
    if (bytes.size() < header) {
        return false;
    }
    int32_t columns, rows;
    double h[5];
    std::memcpy(&columns, bytes.data(), 4);
    std::memcpy(&rows, bytes.data() + 4, 4);
    std::memcpy(h, bytes.data() + 8, sizeof h);
    if (columns <= 0 || rows <= 0 || bytes.size() != header + static_cast<size_t>(columns) * static_cast<size_t>(rows) * 2) {
        return false;
    }
    out.columns = columns;
    out.rows = rows;
    out.lon0 = h[0];
    out.lat0 = h[1];
    out.step = h[2];
    out.values.resize(static_cast<size_t>(columns) * static_cast<size_t>(rows));
    uint16_t q = 0;
    for (size_t i = 0; i < out.values.size(); i++) {
        uint16_t step;
        std::memcpy(&step, bytes.data() + header + i * 2, 2);
        q = static_cast<uint16_t>(q + step);
        out.values[i] = static_cast<float>(h[3] + q * h[4]);
    }
    return true;
}

bool GfsClimate::anchor(const Field& field, int day, GfsGrid::Grid& out, std::string& error) const {
    const PermanentCache store{"climate"};
    const std::string name = field.variable + "_" + std::to_string(field.level) + "_d" + std::to_string(day) + ".bin";
    if (unpack(store.read(name), out)) {
        return true;
    }
    // one band, straight from the file on the web
    const auto scratch = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/wxqt_climate_" + QUuid::createUuid().toString(QUuid::Id128);
    const auto rawPath = scratch + ".raw";
    const auto hdrPath = scratch + ".hdr";
    const auto cleanup = [&] {
        QFile::remove(rawPath);
        QFile::remove(hdrPath);
        QFile::remove(rawPath + ".aux.xml");
    };
    QProcess translate;
    translate.start(QString::fromStdString(gdalBin) + "/gdal_translate",
                    {"-q", "-b", QString::number(band(field, day)), "-of", "ENVI", "-ot", "Float32",
                     QString::fromStdString("NETCDF:/vsicurl/" + baseUrl + field.file + ":" + field.variable), rawPath});
    translate.waitForFinished(600000);   // the climatology server is sometimes very slow: wait
    // GDAL prints errors about drivers this machine lacks; only the exit code and the size of the result count
    if (translate.exitStatus() != QProcess::NormalExit || translate.exitCode() != 0) {
        error = "could not get the climatology (" + field.variable + ") from NOAA PSL: " + translate.readAllStandardError().trimmed().toStdString();
        cleanup();
        return false;
    }
    QFile hdr{hdrPath};
    const auto hdrText = hdr.open(QIODevice::ReadOnly) ? QString::fromUtf8(hdr.readAll()) : QString{};
    const auto number = [&hdrText] (const QString& key) {
        const auto m = QRegularExpression{"\\b" + key + "\\s*=\\s*([-0-9.]+)"}.match(hdrText);
        return m.hasMatch() ? m.captured(1).toDouble() : -1e9;
    };
    const auto info = QRegularExpression{"map info = \\{[^,]*,\\s*1,\\s*1,\\s*([-0-9.]+),\\s*([-0-9.]+),\\s*([-0-9.]+),\\s*([-0-9.]+)"}.match(hdrText);
    GfsGrid::Grid g;
    g.columns = static_cast<int>(number("samples"));
    g.rows = static_cast<int>(number("lines"));
    QFile raw{rawPath};
    const QByteArray bytes = raw.open(QIODevice::ReadOnly) ? raw.readAll() : QByteArray{};
    cleanup();
    if (g.columns <= 0 || g.rows <= 0 || !info.hasMatch() || bytes.size() != static_cast<qsizetype>(g.columns) * g.rows * 4) {
        error = "the climatology slice was not in the expected form";
        return false;
    }
    g.step = info.captured(3).toDouble();
    g.lon0 = info.captured(1).toDouble() + g.step / 2.0;
    g.lat0 = info.captured(2).toDouble() - g.step / 2.0;
    if (g.lon0 < 0.0) {
        // a grid that starts west of zero (-1.25 .. ) is the same one: the first column is at 0 here
        g.lon0 = std::fmod(g.lon0 + 360.0, 360.0);
    }
    g.values.resize(static_cast<size_t>(g.columns) * static_cast<size_t>(g.rows));
    std::memcpy(g.values.data(), bytes.constData(), static_cast<size_t>(bytes.size()));
    if (QRegularExpression{"byte order\\s*=\\s*1"}.match(hdrText).hasMatch()) {
        for (auto& value : g.values) {
            unsigned char b[4];
            std::memcpy(b, &value, 4);
            std::swap(b[0], b[3]);
            std::swap(b[1], b[2]);
            std::memcpy(&value, b, 4);
        }
    }
    store.write(name, pack(g));
    // use what was stored, so a chart looks the same the first time and every time after
    return unpack(pack(g), out);
}

bool GfsClimate::at(const Field& field, int day, GfsGrid::Grid& out, std::string& error) const {
    if (field.file.empty()) {
        error = "no climatology for this level";
        return false;
    }
    int a, b;
    double weight;
    anchors(std::clamp(day, 0, 364), a, b, weight);
    GfsGrid::Grid first, second;
    std::string errorA, errorB;
    auto other = std::async(std::launch::async, [&] { return anchor(field, b, second, errorB); });
    const bool okA = anchor(field, a, first, errorA);
    const bool okB = other.get();
    if (!okA || !okB) {
        error = okA ? errorB : errorA;
        return false;
    }
    out = first;
    for (size_t i = 0; i < out.values.size(); i++) {
        out.values[i] = static_cast<float>(first.values[i] * (1.0 - weight) + second.values[i] * weight);
    }
    return true;
}

// ---- the daily standard deviation ----

namespace {
    std::mutex deviationMutex;
    std::set<std::string> deviationBuilding;
    std::map<std::string, int> deviationPercent;
    std::map<std::string, std::string> deviationFailed;

    std::string deviationName(const GfsClimate::Field& field) {
        return "dev_" + field.variable + "_" + std::to_string(field.level);
    }
}

bool GfsClimate::storedDeviation(const Field& field, int day, GfsGrid::Grid& out) const {
    const PermanentCache store{"climate"};
    return unpack(store.read(deviationName(field) + "_d" + std::to_string(day) + ".bin"), out);
}

bool GfsClimate::deviation(const Field& field, int day, GfsGrid::Grid& out, std::string& error) const {
    if (field.file.empty()) {
        error = "no climatology for this level";
        return false;
    }
    const PermanentCache store{"climate"};
    const auto name = deviationName(field);
    if (store.read(name + "_done").empty()) {
        std::lock_guard lock{deviationMutex};
        if (deviationFailed.count(name)) {
            error = "could not work out the standard deviations: " + deviationFailed[name];
            return false;
        }
        if (!deviationBuilding.count(name)) {
            deviationBuilding.insert(name);
            deviationPercent[name] = 0;
            std::thread{[this, field] { buildDeviation(field); }}.detach();
        }
        error = "The standard deviations of " + field.variable + " are being worked out from 30 years of the reanalysis (once only, in the background): " + std::to_string(deviationPercent[name]) +
            " per cent. Choose this chart again in a few minutes.";
        return false;
    }
    int a, b;
    double weight;
    anchors(std::clamp(day, 0, 364), a, b, weight);
    GfsGrid::Grid first, second;
    if (!storedDeviation(field, a, first) || !storedDeviation(field, b, second)) {
        error = "the stored standard deviations are incomplete";
        return false;
    }
    out = first;
    for (size_t i = 0; i < out.values.size(); i++) {
        out.values[i] = static_cast<float>(first.values[i] * (1.0 - weight) + second.values[i] * weight);
    }
    return true;
}

// Every fourth day of the year (the days the means are kept for), in each of the 30 years: the day's value less the mean of the day, squared, added up. A year is one slice of 92 days of the
// year's file of daily averages, read in one go. The days either side of each (four before, four after) are pooled, so each stored deviation is of about 90 values.
void GfsClimate::buildDeviation(Field field) const {
    const auto name = deviationName(field);
    const auto fail = [&name] (const std::string& why) {
        std::lock_guard lock{deviationMutex};
        deviationFailed[name] = why;
        deviationBuilding.erase(name);
    };
    constexpr int days = 92, firstYear = 1991, lastYear = 2020;
    // the means of those days
    std::vector<GfsGrid::Grid> means(days);
    {
        std::vector<std::future<bool>> jobs;
        std::vector<std::string> problems(days);
        for (int k = 0; k < days; k++) {
            jobs.push_back(std::async(std::launch::async, [this, &field, &means, &problems, k] { return anchor(field, k * 4, means[static_cast<size_t>(k)], problems[static_cast<size_t>(k)]); }));
            if (k % 8 == 7) {   // eight at a time
                for (size_t j = static_cast<size_t>(k) - 7; j <= static_cast<size_t>(k); j++) {
                    jobs[j].wait();
                }
            }
        }
        for (int k = 0; k < days; k++) {
            if (!jobs[static_cast<size_t>(k)].get()) {
                fail(problems[static_cast<size_t>(k)]);
                return;
            }
        }
    }
    const size_t cells = means[0].values.size();
    std::vector<std::vector<double>> sumsq(days, std::vector<double>(cells, 0.0));
    std::vector<int> counts(days, 0);
    std::mutex sumMutex;
    std::string dailyBase = baseUrl;
    if (const auto at = dailyBase.find("ncep.reanalysis.derived"); at != std::string::npos) {
        dailyBase.replace(at, std::string{"ncep.reanalysis.derived"}.size(), "ncep.reanalysis.dailyavgs");
    }
    const std::string suffix = ".day.ltm.1991-2020.nc";
    const auto at = field.file.find(suffix);
    const std::string stem = field.file.substr(0, at == std::string::npos ? field.file.size() : at);
    int done = 0, good = 0;
    const auto year = [&] (int y) {
        const bool leap = (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;
        QStringList arguments{"-q"};
        for (int k = 0; k < days; k++) {
            const int day = k * 4 + (leap && k * 4 >= 59 ? 1 : 0);
            arguments << "-b" << QString::number(band(field, day));
        }
        const auto scratch = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/wxqt_dev_" + QUuid::createUuid().toString(QUuid::Id128);
        const auto rawPath = scratch + ".raw", hdrPath = scratch + ".hdr";
        arguments << "-of" << "ENVI" << "-ot" << "Float32" << QString::fromStdString("NETCDF:/vsicurl/" + dailyBase + stem + "." + std::to_string(y) + ".nc:" + field.variable) << rawPath;
        QProcess translate;
        translate.start(QString::fromStdString(gdalBin) + "/gdal_translate", arguments);
        translate.waitForFinished(900000);
        QFile raw{rawPath};
        QByteArray bytes = translate.exitCode() == 0 && raw.open(QIODevice::ReadOnly) ? raw.readAll() : QByteArray{};
        raw.close();
        QFile hdr{hdrPath};
        const auto hdrText = hdr.open(QIODevice::ReadOnly) ? QString::fromUtf8(hdr.readAll()) : QString{};
        hdr.close();
        QFile::remove(rawPath);
        QFile::remove(hdrPath);
        QFile::remove(rawPath + ".aux.xml");
        bool ok = bytes.size() == static_cast<qsizetype>(cells) * days * 4;
        std::lock_guard lock{sumMutex};
        done++;
        if (ok) {
            const bool swap = QRegularExpression{"byte order\\s*=\\s*1"}.match(hdrText).hasMatch();
            std::vector<float> values(cells * static_cast<size_t>(days));
            std::memcpy(values.data(), bytes.constData(), static_cast<size_t>(bytes.size()));
            if (swap) {
                for (auto& value : values) {
                    unsigned char b[4];
                    std::memcpy(b, &value, 4);
                    std::swap(b[0], b[3]);
                    std::swap(b[1], b[2]);
                    std::memcpy(&value, b, 4);
                }
            }
            for (int k = 0; k < days; k++) {
                bool any = false;
                for (size_t i = 0; i < cells; i++) {
                    const float v = values[static_cast<size_t>(k) * cells + i];
                    if (std::isfinite(v) && std::abs(v) < 1e30f) {
                        const double d = v - means[static_cast<size_t>(k)].values[i];
                        sumsq[static_cast<size_t>(k)][i] += d * d;
                        any = true;
                    }
                }
                counts[static_cast<size_t>(k)] += any ? 1 : 0;
            }
            good++;
        }
        const std::lock_guard other{deviationMutex};
        deviationPercent[name] = done * 100 / (lastYear - firstYear + 1);
    };
    {
        std::vector<std::future<void>> jobs;
        for (int y = firstYear; y <= lastYear; y++) {
            jobs.push_back(std::async(std::launch::async, year, y));
            if ((y - firstYear) % 4 == 3) {   // four years at a time
                for (size_t j = jobs.size() - 4; j < jobs.size(); j++) {
                    jobs[j].wait();
                }
            }
        }
        for (auto& job : jobs) {
            job.get();
        }
    }
    if (good < 20) {
        fail("only " + std::to_string(good) + " of the 30 years could be read");
        return;
    }
    const PermanentCache store{"climate"};
    for (int k = 0; k < days; k++) {
        GfsGrid::Grid g = means[static_cast<size_t>(k)];
        for (size_t i = 0; i < cells; i++) {
            double total = 0.0;
            int n = 0;
            for (const int j : {(k + days - 1) % days, k, (k + 1) % days}) {   // this day and the ones four either side
                total += sumsq[static_cast<size_t>(j)][i];
                n += counts[static_cast<size_t>(j)];
            }
            g.values[i] = n > 0 ? static_cast<float>(std::sqrt(total / n)) : 0.0f;
        }
        store.write(name + "_d" + std::to_string(k * 4) + ".bin", pack(g));
    }
    store.write(name + "_done", "1");
    std::lock_guard lock{deviationMutex};
    deviationBuilding.erase(name);
}
