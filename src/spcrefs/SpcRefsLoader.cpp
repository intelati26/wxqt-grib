// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "spcrefs/SpcRefsLoader.h"
#include <cmath>
#include <map>
#include <memory>
#include <mutex>
#include <QDateTime>
#include "gfs/GfsData.h"
#include "spcrefs/LambertGrid.h"
#include "zarr/ZarrStore.h"

namespace {
    struct Opened {
        std::shared_ptr<ZarrStore> store;
        double initSeconds{0.0};
        std::map<std::string, std::vector<double>> times;   // the time coordinates read so far
    };

    std::mutex mutex;
    std::map<std::string, Opened> stores;

    // the store of a cycle, opened once (its metadata is a request) and kept while the program runs
    bool open(const std::string& url, const std::string& runId, Opened& out, std::string& error) {
        {
            const std::lock_guard lock{mutex};
            const auto found = stores.find(url);
            if (found != stores.end()) {
                out = found->second;
                return true;
            }
        }
        Opened opened;
        opened.store = std::make_shared<ZarrStore>();
        if (!opened.store->open(url, error)) {
            return false;
        }
        auto init = QDateTime::fromString(QString::fromStdString(runId), "yyyyMMddHH");   // the cycle's own time
        init.setTimeSpec(Qt::UTC);
        opened.initSeconds = static_cast<double>(init.toSecsSinceEpoch());
        const std::lock_guard lock{mutex};
        stores[url] = opened;
        out = opened;
        return true;
    }

    // the grid cell values of one slice (the member and time already chosen) of an array as the whole 1059 x 1799 grid
    bool readSlice(const ZarrStore& store, const std::string& name, const ZarrStore::Array& a, int member, int time, std::vector<float>& grid, std::string& error) {
        const int rank = static_cast<int>(a.shape.size());
        grid.assign(static_cast<size_t>(LambertGrid::columns) * LambertGrid::rows, std::nanf(""));
        const int cy = a.chunks[static_cast<size_t>(rank - 2)], cx = a.chunks[static_cast<size_t>(rank - 1)];
        for (int y0 = 0; y0 < LambertGrid::rows; y0 += cy) {
            for (int x0 = 0; x0 < LambertGrid::columns; x0 += cx) {
                std::vector<int> index(static_cast<size_t>(rank), 0);
                int inner = 0;   // the offset inside the chunk of the leading (member and time) cells
                if (rank == 4) {
                    index[0] = member / a.chunks[0];
                    index[1] = time / a.chunks[1];
                    inner = (member % a.chunks[0]) * a.chunks[1] + (time % a.chunks[1]);
                } else {
                    index[0] = time / a.chunks[0];
                    inner = time % a.chunks[0];
                }
                index[static_cast<size_t>(rank - 2)] = y0 / cy;
                index[static_cast<size_t>(rank - 1)] = x0 / cx;
                std::vector<float> chunk;
                if (!store.readChunk(name, index, chunk, error)) {
                    return false;
                }
                const size_t plane = static_cast<size_t>(cy) * static_cast<size_t>(cx);
                if (chunk.size() < static_cast<size_t>(inner + 1) * plane) {
                    error = "a chunk of " + name + " is smaller than its shape says";
                    return false;
                }
                for (int y = 0; y < cy && y0 + y < LambertGrid::rows; y++) {
                    for (int x = 0; x < cx && x0 + x < LambertGrid::columns; x++) {
                        grid[static_cast<size_t>(y0 + y) * LambertGrid::columns + static_cast<size_t>(x0 + x)] = chunk[static_cast<size_t>(inner) * plane + static_cast<size_t>(y) * static_cast<size_t>(cx) + static_cast<size_t>(x)];
                    }
                }
            }
        }
        return true;
    }

    bool load(const GfsData::Run& run, int hour, const GfsData::Want& want, GfsGrid::Grid& out, std::string& error) {
        const auto url = GfsData::spcRefs().fileUrl(run, hour, "");
        Opened opened;
        if (!open(url, run.id(), opened, error)) {
            error = "could not open the REFS store of " + run.id() + ": " + error;
            return false;
        }
        const auto * a = opened.store->array(want.variable);
        if (a == nullptr || a->shape.size() < 3 || a->shape.back() != LambertGrid::columns || a->shape[a->shape.size() - 2] != LambertGrid::rows) {
            error = "the REFS store has no field " + want.variable;
            return false;
        }
        const int rank = static_cast<int>(a->shape.size());
        // the time axis of this array: the index whose valid time is `hour` hours after the cycle
        const auto& dimension = a->dims[static_cast<size_t>(rank - 3)];
        std::vector<double> times;
        {
            const std::lock_guard lock{mutex};
            const auto found = stores[url].times.find(dimension);
            if (found != stores[url].times.end()) {
                times = found->second;
            }
        }
        if (times.empty()) {
            if (!opened.store->readNumbers(dimension, times, error)) {
                return false;
            }
            const std::lock_guard lock{mutex};
            stores[url].times[dimension] = times;
        }
        int time = -1;
        for (size_t i = 0; i < times.size(); i++) {
            if (std::lround((times[i] - opened.initSeconds) / 3600.0) == hour) {
                time = static_cast<int>(i);
                break;
            }
        }
        if (time < 0) {
            error = "REFS has no " + want.variable + " for forecast hour " + std::to_string(hour) + " (its hours: " + dimension + ")";
            return false;
        }
        int member = 0;
        if (rank == 4 && want.stat.size() == 4 && want.stat[0] == 'm') {
            member = std::max(0, std::atoi(want.stat.c_str() + 1) - 1);
        }
        if (rank == 4 && member >= a->shape[0]) {
            error = "REFS has no member " + want.stat;
            return false;
        }
        std::vector<float> lambert;
        if (!readSlice(*opened.store, want.variable, *a, member, time, lambert, error)) {
            return false;
        }
        const bool hasFill = a->hasFill;
        const double fill = a->fillValue;
        // to latitude / longitude: the cell of each point, bilinear
        constexpr double step = 0.03, west = -127.0, south = 22.0, east = -65.0, north = 52.0;
        const int columns = static_cast<int>(std::lround((east - west) / step)), rows = static_cast<int>(std::lround((north - south) / step));
        static const LambertGrid grid;
        out.columns = columns;
        out.rows = rows;
        out.step = step;
        out.lon0 = west + step / 2.0;
        out.lat0 = north - step / 2.0;
        out.values.assign(static_cast<size_t>(columns) * static_cast<size_t>(rows), std::nanf(""));
        const auto at = [&] (int c, int r) {
            const float v = lambert[static_cast<size_t>(r) * LambertGrid::columns + static_cast<size_t>(c)];
            return (hasFill && !std::isnan(fill) && v == static_cast<float>(fill)) || std::abs(v) > 1e30f ? std::nanf("") : v;
        };
        for (int r = 0; r < rows; r++) {
            const double lat = out.lat0 - r * step;
            for (int c = 0; c < columns; c++) {
                double column, row;
                grid.toGrid(lat, out.lon0 + c * step, column, row);
                if (column < 0.0 || row < 0.0 || column > LambertGrid::columns - 1.0 || row > LambertGrid::rows - 1.0) {
                    continue;
                }
                const int c0 = static_cast<int>(std::floor(column)), r0 = static_cast<int>(std::floor(row));
                const int c1 = std::min(c0 + 1, LambertGrid::columns - 1), r1 = std::min(r0 + 1, LambertGrid::rows - 1);
                const double fx = column - c0, fy = row - r0;
                const float v00 = at(c0, r0), v10 = at(c1, r0), v01 = at(c0, r1), v11 = at(c1, r1);
                if (std::isnan(v00) || std::isnan(v10) || std::isnan(v01) || std::isnan(v11)) {
                    // a mask (paintball bits) or an edge: the nearest cell, so a value is not blended with a hole
                    out.values[static_cast<size_t>(r) * static_cast<size_t>(columns) + static_cast<size_t>(c)] = at(static_cast<int>(std::lround(column)), static_cast<int>(std::lround(row)));
                    continue;
                }
                const bool mask = want.variable.find("_pb_") != std::string::npos;   // bit masks: not averaged
                out.values[static_cast<size_t>(r) * static_cast<size_t>(columns) + static_cast<size_t>(c)] = mask
                    ? at(static_cast<int>(std::lround(column)), static_cast<int>(std::lround(row)))
                    : static_cast<float>((v00 * (1 - fx) + v10 * fx) * (1 - fy) + (v01 * (1 - fx) + v11 * fx) * fy);
            }
        }
        return true;
    }
}

void SpcRefsLoader::registerLoader() {
    GfsData::setLoader("SPCREFS", load);
}
