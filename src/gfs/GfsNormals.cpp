// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "gfs/GfsNormals.h"
#include "gfs/GdalRaster.h"
#include "gfs/GfsGrid.h"
#include "models/UtilityGrib.h"
#include "util/PermanentCache.h"
#include "util/UtilityIO.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <future>
#include <limits>
#include <mutex>
#include <vector>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QUuid>

namespace {
    constexpr int terms = GfsNormals::terms;
    constexpr int samples = 15;
    constexpr int harmonics = 6;
    constexpr double yearLength = 366.0;
    constexpr float quant = 0.05f;   // degrees F: the steps the coefficients are kept in
    constexpr int32_t magic = 0x324d524e;   // "NRM2"

    const char * tokenUrl = "https://planetarycomputer.microsoft.com/api/sas/v1/token/noaanormals/gridded-normals-cogs";
    const char * dayUrl = "https://noaanormals.blob.core.windows.net/gridded-normals-cogs/normals-daily/1991-2020/1991_2020-daily-";

    const char * variable(GfsNormals::Kind kind) {
        return kind == GfsNormals::Kind::High ? "tmax_norm" : "tmin_norm";
    }
    std::string cacheName(GfsNormals::Kind kind) {
        return std::string{"ncei_"} + variable(kind) + "_v1.bin";
    }

    struct Table {
        bool loaded{false};
        int columns{0}, rows{0};
        double lon0{0.0}, lat0{0.0}, step{0.0};
        std::vector<int32_t> index;   // a cell of the grid -> its place in coefficients, or -1 (the sea, outside the country)
        std::vector<float> coefficients;   // cell after cell, `terms` each
    };
    std::mutex tableMutex;
    Table tables[2];
    Table& tableOf(GfsNormals::Kind kind) {
        return tables[kind == GfsNormals::Kind::High ? 0 : 1];
    }

    // the coefficients of a least squares fit through the 15 sample days, as one matrix: P (terms x samples), found once by Gram-Schmidt with a second pass (the days are not evenly spread round the year:
    // the last one is two days from the first)
    const std::array<std::array<double, samples>, terms>& pseudoInverse() {
        static const auto matrix = [] {
            std::array<std::array<long double, terms>, samples> a{};   // a[row = day][column = term]
            const auto days = GfsNormals::Fit::sampleDays();
            for (int r = 0; r < samples; r++) {
                const long double t = 2.0L * 3.14159265358979323846L * (days[static_cast<size_t>(r)] - 1) / yearLength;
                a[static_cast<size_t>(r)][0] = 1.0L;
                for (int h = 1; h <= harmonics; h++) {
                    a[static_cast<size_t>(r)][static_cast<size_t>(2 * h - 1)] = std::cos(h * t);
                    a[static_cast<size_t>(r)][static_cast<size_t>(2 * h)] = std::sin(h * t);
                }
            }
            std::array<std::array<long double, samples>, terms> q{};   // q[term][row]: the orthonormal columns
            std::array<std::array<long double, terms>, terms> rr{};
            for (int c = 0; c < terms; c++) {
                std::array<long double, samples> v{};
                for (int r = 0; r < samples; r++) {
                    v[static_cast<size_t>(r)] = a[static_cast<size_t>(r)][static_cast<size_t>(c)];
                }
                for (int pass = 0; pass < 2; pass++) {
                    for (int k = 0; k < c; k++) {
                        long double dot = 0.0L;
                        for (int r = 0; r < samples; r++) {
                            dot += q[static_cast<size_t>(k)][static_cast<size_t>(r)] * v[static_cast<size_t>(r)];
                        }
                        rr[static_cast<size_t>(k)][static_cast<size_t>(c)] += dot;
                        for (int r = 0; r < samples; r++) {
                            v[static_cast<size_t>(r)] -= dot * q[static_cast<size_t>(k)][static_cast<size_t>(r)];
                        }
                    }
                }
                long double norm = 0.0L;
                for (int r = 0; r < samples; r++) {
                    norm += v[static_cast<size_t>(r)] * v[static_cast<size_t>(r)];
                }
                norm = std::sqrt(norm);
                rr[static_cast<size_t>(c)][static_cast<size_t>(c)] = norm;
                for (int r = 0; r < samples; r++) {
                    q[static_cast<size_t>(c)][static_cast<size_t>(r)] = v[static_cast<size_t>(r)] / norm;
                }
            }
            // P = R^-1 Q^T: back substitution for each sample's column
            std::array<std::array<double, samples>, terms> p{};
            for (int s = 0; s < samples; s++) {
                std::array<long double, terms> x{};
                for (int i = terms - 1; i >= 0; i--) {
                    long double sum = q[static_cast<size_t>(i)][static_cast<size_t>(s)];
                    for (int j = i + 1; j < terms; j++) {
                        sum -= rr[static_cast<size_t>(i)][static_cast<size_t>(j)] * x[static_cast<size_t>(j)];
                    }
                    x[static_cast<size_t>(i)] = sum / rr[static_cast<size_t>(i)][static_cast<size_t>(i)];
                }
                for (int i = 0; i < terms; i++) {
                    p[static_cast<size_t>(i)][static_cast<size_t>(s)] = static_cast<double>(x[static_cast<size_t>(i)]);
                }
            }
            return p;
        }();
        return matrix;
    }

    void put(std::string& out, const void * data, size_t size) {
        out.append(static_cast<const char *>(data), size);
    }

    std::string pack(const Table& t, const std::vector<std::vector<int32_t>>& quantized, size_t cells) {
        std::string out;
        const int32_t header[4] = {magic, t.columns, t.rows, static_cast<int32_t>(cells)};
        put(out, header, sizeof header);
        const double geometry[3] = {t.lon0, t.lat0, t.step};
        put(out, geometry, sizeof geometry);
        const float q = quant;
        put(out, &q, sizeof q);
        // the land cells as bits, then each term's steps (the difference from the cell before, 16 bits)
        std::string mask((t.index.size() + 7) / 8, '\0');
        for (size_t i = 0; i < t.index.size(); i++) {
            if (t.index[i] >= 0) {
                mask[i / 8] = static_cast<char>(static_cast<unsigned char>(mask[i / 8]) | (1u << (i % 8)));
            }
        }
        out += mask;
        for (int term = 0; term < terms; term++) {
            int32_t previous = 0;
            for (size_t c = 0; c < cells; c++) {
                const int32_t v = quantized[static_cast<size_t>(term)][c];
                const auto step = static_cast<int16_t>(std::clamp(v - previous, -32768, 32767));
                put(out, &step, 2);
                previous += step;
            }
        }
        return out;
    }

    bool unpack(const std::string& bytes, Table& t) {
        constexpr size_t header = 16 + 24 + 4;
        if (bytes.size() < header) {
            return false;
        }
        int32_t h[4];
        double g[3];
        float q;
        std::memcpy(h, bytes.data(), 16);
        std::memcpy(g, bytes.data() + 16, 24);
        std::memcpy(&q, bytes.data() + 40, 4);
        if (h[0] != magic || h[1] <= 0 || h[2] <= 0 || h[3] <= 0) {
            return false;
        }
        const size_t cellsOfGrid = static_cast<size_t>(h[1]) * static_cast<size_t>(h[2]), cells = static_cast<size_t>(h[3]);
        const size_t maskBytes = (cellsOfGrid + 7) / 8;
        if (bytes.size() != header + maskBytes + static_cast<size_t>(terms) * cells * 2) {
            return false;
        }
        t.columns = h[1];
        t.rows = h[2];
        t.lon0 = g[0];
        t.lat0 = g[1];
        t.step = g[2];
        t.index.assign(cellsOfGrid, -1);
        int32_t next = 0;
        for (size_t i = 0; i < cellsOfGrid; i++) {
            if (static_cast<unsigned char>(bytes[header + i / 8]) & (1u << (i % 8))) {
                t.index[i] = next++;
            }
        }
        if (static_cast<size_t>(next) != cells) {
            return false;
        }
        t.coefficients.assign(cells * terms, 0.0f);
        const char * at = bytes.data() + header + maskBytes;
        for (int term = 0; term < terms; term++) {
            int32_t value = 0;
            for (size_t c = 0; c < cells; c++) {
                int16_t step;
                std::memcpy(&step, at, 2);
                at += 2;
                value += step;
                t.coefficients[c * terms + static_cast<size_t>(term)] = static_cast<float>(value * static_cast<double>(q));
            }
        }
        t.loaded = true;
        return true;
    }

    // fetch the 15 sample days, fit, keep
    bool build(GfsNormals::Kind kind, std::string& error) {
        const QJsonObject tokenObject = QJsonDocument::fromJson(QByteArray::fromStdString(UtilityIO::getHtml(tokenUrl))).object();
        const std::string token = tokenObject.value("token").toString().toStdString();
        if (token.empty()) {
            error = "could not get a read token for the NOAA climate normals from Planetary Computer";
            return false;
        }
        const auto days = GfsNormals::Fit::sampleDays();
        const auto scratch = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/wxqt_normals_" + QUuid::createUuid().toString(QUuid::Id128);
        std::vector<GfsGrid::Grid> grids(samples);
        std::vector<std::vector<float>> values(samples);
        std::vector<std::string> problems(samples);
        std::vector<char> good(samples, 0);
        std::vector<std::future<void>> jobs;
        for (int s = 0; s < samples; s++) {
            jobs.push_back(std::async(std::launch::async, [&, s] {
                char name[16];
                std::snprintf(name, sizeof name, "%03d", days[static_cast<size_t>(s)]);
                GdalRaster::Options options;
                options.gdalBin = QString::fromStdString(UtilityGrib::gdalBinDir());
                options.band = 1;
                options.timeoutMs = 300000;
                int bands = 0;
                const QString input = QString::fromStdString(std::string{"/vsicurl/"} + dayUrl + name + "-" + variable(kind) + ".tif?" + token);
                good[static_cast<size_t>(s)] = GdalRaster::read(input, scratch + "_" + QString::number(s), options, grids[static_cast<size_t>(s)], bands, values[static_cast<size_t>(s)], problems[static_cast<size_t>(s)]) && bands == 1;
            }));
            if (s % 4 == 3) {   // four at a time
                for (size_t j = static_cast<size_t>(s) - 3; j <= static_cast<size_t>(s); j++) {
                    jobs[j].wait();
                }
            }
        }
        for (auto& job : jobs) {
            job.wait();
        }
        for (int s = 0; s < samples; s++) {
            if (!good[static_cast<size_t>(s)]) {
                error = "could not get the NOAA climate normals (day " + std::to_string(days[static_cast<size_t>(s)]) + "): " + problems[static_cast<size_t>(s)];
                return false;
            }
            if (grids[static_cast<size_t>(s)].columns != grids[0].columns || grids[static_cast<size_t>(s)].rows != grids[0].rows || values[static_cast<size_t>(s)].size() != values[0].size()) {
                error = "the NOAA climate normals are not on one grid";
                return false;
            }
        }
        Table t;
        t.columns = grids[0].columns;
        t.rows = grids[0].rows;
        t.lon0 = grids[0].lon0;
        t.lat0 = grids[0].lat0;
        t.step = grids[0].step;
        const size_t cellsOfGrid = values[0].size();
        t.index.assign(cellsOfGrid, -1);
        size_t cells = 0;
        for (size_t i = 0; i < cellsOfGrid; i++) {
            bool all = true;
            for (int s = 0; s < samples && all; s++) {
                const float v = values[static_cast<size_t>(s)][i];
                all = std::isfinite(v) && std::abs(v) < 1e30f;
            }
            if (all) {
                t.index[i] = static_cast<int32_t>(cells++);
            }
        }
        if (cells < 100000) {
            error = "the NOAA climate normals have too few land cells (" + std::to_string(cells) + ")";
            return false;
        }
        const auto& p = pseudoInverse();
        std::vector<std::vector<int32_t>> quantized(terms, std::vector<int32_t>(cells));
        for (size_t i = 0; i < cellsOfGrid; i++) {
            if (t.index[i] < 0) {
                continue;
            }
            std::array<double, samples> y{};
            for (int s = 0; s < samples; s++) {
                y[static_cast<size_t>(s)] = static_cast<double>(values[static_cast<size_t>(s)][i]) * 1.8 + 32.0;   // the files are degrees C
            }
            for (int term = 0; term < terms; term++) {
                double c = 0.0;
                for (int s = 0; s < samples; s++) {
                    c += p[static_cast<size_t>(term)][static_cast<size_t>(s)] * y[static_cast<size_t>(s)];
                }
                quantized[static_cast<size_t>(term)][static_cast<size_t>(t.index[i])] = static_cast<int32_t>(std::lround(c / static_cast<double>(quant)));
            }
        }
        const PermanentCache store{"climate"};
        if (!store.write(cacheName(kind), pack(t, quantized, cells))) {
            error = "could not keep the NOAA climate normals";
            return false;
        }
        return true;
    }
}

const GfsNormals& GfsNormals::shared() {
    static const GfsNormals instance;
    return instance;
}

bool GfsNormals::ready(Kind kind) const {
    const std::lock_guard lock{tableMutex};
    return tableOf(kind).loaded || PermanentCache{"climate"}.has(cacheName(kind));
}

bool GfsNormals::prepare(Kind kind, std::string& error) const {
    const std::lock_guard lock{tableMutex};   // one builds, the others wait for it
    Table& t = tableOf(kind);
    if (t.loaded) {
        return true;
    }
    const PermanentCache store{"climate"};
    Table fresh;
    if (unpack(store.read(cacheName(kind)), fresh)) {
        t = std::move(fresh);
        return true;
    }
    if (!build(kind, error)) {
        return false;
    }
    Table built;
    if (!unpack(store.read(cacheName(kind)), built)) {
        error = "the NOAA climate normals that were kept could not be read back";
        return false;
    }
    t = std::move(built);
    return true;
}

int GfsNormals::dayOfLeapYear(int month, int dayOfMonth) {
    static const int before[12] = {0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335};
    return before[std::clamp(month, 1, 12) - 1] + std::clamp(dayOfMonth, 1, 31);
}

GfsNormals::Day GfsNormals::day(int month, int dayOfMonth) {
    Day d;
    const double t = 2.0 * 3.14159265358979323846 * (dayOfLeapYear(month, dayOfMonth) - 1) / yearLength;
    d.basis[0] = 1.0;
    for (int h = 1; h <= harmonics; h++) {
        d.basis[static_cast<size_t>(2 * h - 1)] = std::cos(h * t);
        d.basis[static_cast<size_t>(2 * h)] = std::sin(h * t);
    }
    return d;
}

std::array<int, 15> GfsNormals::Fit::sampleDays() {
    std::array<int, 15> days{};
    for (int k = 0; k < samples; k++) {
        days[static_cast<size_t>(k)] = 1 + 26 * k;
    }
    return days;
}

std::array<double, GfsNormals::terms> GfsNormals::Fit::of(const std::array<double, 15>& y) {
    const auto& p = pseudoInverse();
    std::array<double, terms> c{};
    for (int term = 0; term < terms; term++) {
        for (int s = 0; s < samples; s++) {
            c[static_cast<size_t>(term)] += p[static_cast<size_t>(term)][static_cast<size_t>(s)] * y[static_cast<size_t>(s)];
        }
    }
    return c;
}

double GfsNormals::at(Kind kind, const Day& d, double lon, double lat) const {
    const Table * t;
    {
        const std::lock_guard lock{tableMutex};
        t = &tableOf(kind);
        if (!t->loaded) {
            return std::numeric_limits<double>::quiet_NaN();
        }
    }
    // the tables are written once (prepare) and read by many threads after that
    const double fx = (lon - t->lon0) / t->step, fy = (t->lat0 - lat) / t->step;
    const int column = static_cast<int>(std::floor(fx)), row = static_cast<int>(std::floor(fy));
    for (int ring = 0; ring <= 1; ring++) {   // the cell, else the nearest land cell one away (a coast)
        for (int dr = -ring; dr <= ring; dr++) {
            for (int dc = -ring; dc <= ring; dc++) {
                if (ring == 1 && dr == 0 && dc == 0) {
                    continue;
                }
                const int c = column + dc, r = row + dr;
                if (c < 0 || r < 0 || c >= t->columns || r >= t->rows) {
                    continue;
                }
                const int32_t at = t->index[static_cast<size_t>(r) * static_cast<size_t>(t->columns) + static_cast<size_t>(c)];
                if (at < 0) {
                    continue;
                }
                const float * k = &t->coefficients[static_cast<size_t>(at) * terms];
                double sum = 0.0;
                for (int i = 0; i < terms; i++) {
                    sum += static_cast<double>(k[i]) * d.basis[static_cast<size_t>(i)];
                }
                return sum;
            }
        }
    }
    return std::numeric_limits<double>::quiet_NaN();
}
