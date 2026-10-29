// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "mrms/MrmsOverlays.h"
#include <algorithm>
#include <QtGlobal>
#include <cmath>
#include <future>
#include <set>
#include <string>
#include <QImage>

using std::string;
using std::vector;

namespace {
    // the blocks of the finest grid at 4 x 4 and 16 x 16 cells, keeping the largest index (the indices rise with the value): a thin track still shows when a pixel spans kilometers
    void addPooledLevels(MrmsOverlays::Swath& swath, const UtilityMrms::Grid& fine, const QByteArray& cells) {
        swath.levels.clear();
        swath.levels.emplace_back(fine, cells);
        for (const int factor : {4, 16}) {
            UtilityMrms::Grid coarse = fine;
            coarse.columns = (fine.columns + factor - 1) / factor;
            coarse.rows = (fine.rows + factor - 1) / factor;
            coarse.cell = fine.cell * factor;
            QByteArray pooled(static_cast<qsizetype>(coarse.columns) * coarse.rows, 0);
            auto * dst = reinterpret_cast<uchar *>(pooled.data());
            const auto * src = reinterpret_cast<const uchar *>(cells.constData());
            for (int row = 0; row < fine.rows; row++) {
                const auto * line = src + static_cast<qsizetype>(row) * fine.columns;
                auto * target = dst + static_cast<qsizetype>(row / factor) * coarse.columns;
                for (int column = 0; column < fine.columns; column++) {
                    if (line[column] > target[column / factor]) {
                        target[column / factor] = line[column];
                    }
                }
            }
            swath.levels.emplace_back(coarse, pooled);
        }
    }

    void colorize(MrmsOverlays::Swath& swath, const UtilityMrms::Frame& frame, const vector<MrmsOverlays::Band>& bands) {
        swath.colors = QVector<QRgb>(256, qRgba(0, 0, 0, 0));
        for (int index = 1; index < 256; index++) {
            const double value = frame.valueAt(index);
            for (const auto& band : bands) {
                if (value >= band.from) {
                    swath.colors[index] = qPremultiply(band.color.rgba());
                }
            }
        }
    }

    // the product (known or discovered on the server), its newest scan at a level folder
    bool newest(const string& wanted, const char * level, UtilityMrms::Product& product, UtilityMrms::Frame& frame) {
        vector<UtilityMrms::Product> list = UtilityMrms::products();
        string error;
        if (std::none_of(list.begin(), list.end(), [&wanted] (const auto& p) { return p.id == wanted; })) {
            vector<UtilityMrms::Product> more;
            UtilityMrms::discoverMore(more, error);
            list.insert(list.end(), more.begin(), more.end());
        }
        const auto found = std::find_if(list.begin(), list.end(), [&wanted] (const auto& p) { return p.id == wanted; });
        if (found == list.end()) {
            return false;
        }
        product = *found;
        vector<UtilityMrms::Scan> scans;
        bool listed = UtilityMrms::scansAws(product, level, scans, error);   // the open data bucket first
        if (!listed) {
            listed = UtilityMrms::scans(product, scans, error);
        }
        return listed && !scans.empty() && UtilityMrms::frame(product, scans.back(), frame, error);
    }
}

namespace MrmsOverlays {

const std::vector<Band>& hailBands() {
    static const std::vector<Band> bands{{25.4, QColor{255, 240, 0, 245}, "1"}, {44.5, QColor{255, 150, 0, 250}, "1.75"}, {63.5, QColor{255, 30, 30, 255}, "2.5"},
                                         {101.6, QColor{255, 0, 255, 255}, "4+"}};
    return bands;
}

const std::vector<Band>& rotationBands() {
    // grays to black: the reflectivity under them uses every hue (cyan, blue, greens, yellow, reds, magenta, purple, white), so a track in any of them is lost in it; the layer edges the shapes
    // in black with a white halo outside
    static const std::vector<Band> bands{{8.0, QColor{150, 150, 150, 255}, "8"}, {12.0, QColor{100, 100, 100, 255}, "12"}, {16.0, QColor{58, 58, 58, 255}, "16"},
                                         {20.0, QColor{12, 12, 12, 255}, "20+"}};
    return bands;
}

Lightning lightning(int periods) {
    static const char * ids[] = {"NLDN_CG_001min_AvgDensity", "NLDN_CG_005min_AvgDensity", "NLDN_CG_015min_AvgDensity", "NLDN_CG_030min_AvgDensity"};
    Lightning out;
    periods = std::clamp(periods, 1, 4);
    vector<UtilityMrms::Product> more;
    string error;
    UtilityMrms::discoverMore(more, error);
    // the periods are separate small files on the open data server: fetched and decoded side by side
    struct Period {
        UtilityMrms::Frame frame;
        bool ok{false};
    };
    vector<std::future<Period>> pending;
    for (int period = 0; period < periods; period++) {
        const string wanted = ids[period];
        const auto found = std::find_if(more.begin(), more.end(), [&wanted] (const auto& p) { return p.id == wanted; });
        if (found == more.end()) {
            pending.emplace_back();   // not offered: an invalid future
            continue;
        }
        const auto product = *found;
        pending.push_back(std::async(std::launch::async, [product] {
            Period out;
            string e;
            vector<UtilityMrms::Scan> list;
            // the open data bucket on AWS first (the lightning products are on one level), the NCEP server when it gives nothing
            bool listed = UtilityMrms::scansAws(product, "00.00", list, e);
            if (!listed) {
                listed = UtilityMrms::scans(product, list, e);
            }
            out.ok = listed && !list.empty() && UtilityMrms::frame(product, list.back(), out.frame, e);
            return out;
        }));
    }
    std::set<std::pair<int, int>> seen;   // cells already placed by a shorter period
    for (int period = 0; period < periods; period++) {
        if (!pending[static_cast<size_t>(period)].valid()) {
            continue;
        }
        const auto data = pending[static_cast<size_t>(period)].get();
        if (!data.ok) {
            continue;
        }
        const auto& frame = data.frame;
        out.ok = true;
        if (period == 0) {
            out.utc = frame.utc;
        }
        const auto cells = frame.indices();
        const auto& grid = frame.grid;
        const int firstStrike = frame.valueAt(1) > 0.0 ? 1 : 2;   // the lowest index that is a density above zero (index 1 can be zero)
        for (int row = 0; row < grid.rows; row++) {
            const auto * line = reinterpret_cast<const uchar *>(cells.constData()) + static_cast<qsizetype>(row) * grid.columns;
            for (int column = 0; column < grid.columns; column++) {
                if (line[column] < firstStrike || out.points.size() >= 300000) {
                    continue;
                }
                if (!seen.insert({column, row}).second) {
                    continue;   // a shorter period has it
                }
                out.points.push_back({static_cast<float>(grid.west + (column + 0.5) * grid.cell), static_cast<float>(grid.north - (row + 0.5) * grid.cell),
                                          static_cast<unsigned char>(period)});
            }
        }
    }

    return out;
}

Chance chance(int which) {
    static const char * ids[] = {"", "LightningProbabilityNext30min", "LightningProbabilityNext60min"};
    Chance out;
    const string wanted = ids[std::clamp(which, 1, 2)];
    UtilityMrms::Product product;
    UtilityMrms::Frame frame;
    if (!newest(wanted, "Grid_scale_1", product, frame)) {
        return out;
    }
    out.ok = true;
    const int block = 5;
    const auto& grid = frame.grid;
    const int columns = grid.columns / block, rows = grid.rows / block;
    const auto cells = frame.indices();
    const auto * in = reinterpret_cast<const uchar *>(cells.constData());
    vector<float> field(static_cast<size_t>(columns) * rows, 0.0f);   // the largest chance in each block
    for (int row = 0; row < rows; row++) {
        for (int column = 0; column < columns; column++) {
            int best = 0;
            for (int dr = 0; dr < block; dr++) {
                const auto * line = in + static_cast<qsizetype>(row * block + dr) * grid.columns + column * block;
                for (int dc = 0; dc < block; dc++) {
                    best = std::max<int>(best, line[dc]);
                }
            }
            field[static_cast<size_t>(row) * columns + column] = best == 0 ? 0.0f : static_cast<float>(frame.valueAt(best));
        }
    }
    const double cell = grid.cell * block;
    const auto lon = [&] (double c) { return static_cast<float>(grid.west + (c + 0.5) * cell); };
    const auto lat = [&] (double r) { return static_cast<float>(grid.north - (r + 0.5) * cell); };
    for (int row = 0; row < rows; row++) {
        for (int column = 0; column < columns; column++) {
            const float value = field[static_cast<size_t>(row) * columns + column];
            if (value >= 10.0f) {
                out.cells.push_back({lon(column), lat(row), static_cast<float>(cell), static_cast<unsigned char>(std::min(100.0f, value))});
            }
        }
    }
    for (const int level : {10, 30, 50, 70}) {   // marching squares at each level
        const float t = static_cast<float>(level);
        for (int row = 0; row + 1 < rows; row++) {
            for (int column = 0; column + 1 < columns; column++) {
                const float a = field[static_cast<size_t>(row) * columns + column], b = field[static_cast<size_t>(row) * columns + column + 1];
                const float d = field[static_cast<size_t>(row + 1) * columns + column], c = field[static_cast<size_t>(row + 1) * columns + column + 1];
                const int code = (a >= t ? 1 : 0) | (b >= t ? 2 : 0) | (c >= t ? 4 : 0) | (d >= t ? 8 : 0);
                if (code == 0 || code == 15) {
                    continue;
                }
                const auto cross = [t] (float v1, float v2) { return v1 == v2 ? 0.5f : std::clamp((t - v1) / (v2 - v1), 0.0f, 1.0f); };
                const double x0 = column, y0 = row;
                const double topX = x0 + cross(a, b), topY = y0;
                const double rightX = x0 + 1, rightY = y0 + cross(b, c);
                const double bottomX = x0 + cross(d, c), bottomY = y0 + 1;
                const double leftX = x0, leftY = y0 + cross(a, d);
                const auto add = [&] (double x1, double y1, double x2, double y2) {
                    out.lines.push_back({static_cast<unsigned char>(level), lon(x1), lat(y1), lon(x2), lat(y2)});
                };
                switch (code) {
                    case 1: case 14: add(leftX, leftY, topX, topY); break;
                    case 2: case 13: add(topX, topY, rightX, rightY); break;
                    case 3: case 12: add(leftX, leftY, rightX, rightY); break;
                    case 4: case 11: add(rightX, rightY, bottomX, bottomY); break;
                    case 6: case 9: add(topX, topY, bottomX, bottomY); break;
                    case 7: case 8: add(leftX, leftY, bottomX, bottomY); break;
                    case 5: add(leftX, leftY, topX, topY); add(rightX, rightY, bottomX, bottomY); break;
                    case 10: add(topX, topY, rightX, rightY); add(leftX, leftY, bottomX, bottomY); break;
                    default: break;
                }
            }
        }
    }
    return out;
}

Swath hail(int which) {
    static const char * ids[] = {"", "MESH_Max_30min", "MESH_Max_60min", "MESH_Max_120min", "MESH_Max_360min", "MESH_Max_1440min"};
    Swath out;
    UtilityMrms::Product product;
    UtilityMrms::Frame frame;
    if (!newest(ids[std::clamp(which, 1, 5)], "00.50", product, frame)) {
        return out;
    }
    // only 1 in and up (25.4 mm): the indices rise with the value, so the first index at or above it is the cut
    int first = 256;
    for (int index = 1; index < 256; index++) {
        if (frame.valueAt(index) >= hailBands().front().from) {
            first = index;
            break;
        }
    }
    QByteArray cut = frame.indices();
    auto * cells = reinterpret_cast<uchar *>(cut.data());
    for (qsizetype i = 0; i < cut.size(); i++) {
        if (cells[i] < first) {
            cells[i] = 0;
        }
    }
    addPooledLevels(out, frame.grid, cut);
    colorize(out, frame, hailBands());
    out.utc = frame.utc;
    out.ok = true;
    return out;
}

// the weakest shear drawn (a development aid: WXQT_ROT_MIN lowers it, to see tracks on a quiet day)
static double rotationMinimum() {
    const double wanted = qEnvironmentVariable("WXQT_ROT_MIN").toDouble();
    return wanted > 0.0 ? wanted : rotationBands().front().from;
}

Swath rotation(int which) {
    static const char * ids[] = {"", "RotationTrack30min", "RotationTrack60min", "RotationTrack120min", "RotationTrack360min", "RotationTrack1440min",
                                 "RotationTrackML60min", "RotationTrackML120min", "RotationTrackML360min"};
    Swath result;
    UtilityMrms::Product product;
    UtilityMrms::Frame frame;
    if (!newest(ids[std::clamp(which, 1, 8)], "00.50", product, frame)) {
        return result;
    }
    // strong rotation only where the radars see well: MRMS's radar quality index (0 to 1) masks the beam-blocked and far-range areas, where clutter and the
    // edges of the coverage make false rotation (Canada, the mountains, the coasts)
    QByteArray cleaned = frame.indices();
    UtilityMrms::Frame quality;
    bool haveQuality = false;
    {
        UtilityMrms::Product rqi;
        const auto list = UtilityMrms::products();
        const auto known = std::find_if(list.begin(), list.end(), [] (const auto& p) { return p.id == "RadarQualityIndex"; });
        if (known != list.end()) {
            rqi = *known;
        } else {
            rqi.id = "RadarQualityIndex";
            rqi.label = "Radar quality index";
            rqi.validMin = -1.0;
            rqi.hi = 1.0;
            rqi.autoRange = true;
            rqi.stops = {{0.0, 0, 0, 0}, {1.0, 255, 255, 255}};
        }
        vector<UtilityMrms::Scan> qualityScans;
        string e;
        bool qualityListed = UtilityMrms::scansAws(rqi, "00.00", qualityScans, e);
        if (!qualityListed) {
            qualityListed = UtilityMrms::scans(rqi, qualityScans, e);
        }
        haveQuality = qualityListed && !qualityScans.empty() && UtilityMrms::frame(rqi, qualityScans.back(), quality, e) && quality.grid.columns > 0;
    }
    const auto& grid = frame.grid;
    auto * out = reinterpret_cast<uchar *>(cleaned.data());
    int firstStrong = 256;
    for (int index = 1; index < 256; index++) {
        if (frame.valueAt(index) >= rotationMinimum()) {
            firstStrong = index;
            break;
        }
    }
    const QByteArray qualityCells = haveQuality ? quality.indices() : QByteArray{};
    const auto * good = reinterpret_cast<const uchar *>(qualityCells.constData());
    const auto& q = quality.grid;   // the quality grid is coarser (0.01 against the tracks' 0.005 degree): the cell under each track cell
    vector<int> qualityColumn(static_cast<size_t>(grid.columns), -1);
    for (int column = 0; column < grid.columns && haveQuality; column++) {
        const int c = static_cast<int>(std::floor((grid.west + (column + 0.5) * grid.cell - q.west) / q.cell));
        qualityColumn[static_cast<size_t>(column)] = c >= 0 && c < q.columns ? c : -1;
    }
    // north of the border the grid has Canada's radars and its edges: their rotation is mostly clutter, so only the United States side is kept (the border taken
    // as a few straight pieces: 49N in the west, the Lakes, the St. Lawrence and Maine)
    const auto borderLat = [] (double lon) {
        if (lon < -95.2) return 49.05;
        if (lon < -89.0) return 49.4;
        if (lon < -84.5) return 48.3;
        if (lon < -79.0) return 46.0;
        if (lon < -74.7) return 45.05;
        if (lon < -71.0) return 45.3;
        if (lon < -67.8) return 47.5;
        return 0.0;   // New Brunswick and the sea east of Maine
    };
    for (int row = 0; row < grid.rows; row++) {
        const int qr = haveQuality ? static_cast<int>(std::floor((q.north - (grid.north - (row + 0.5) * grid.cell)) / q.cell)) : -1;
        const double lat = grid.north - (row + 0.5) * grid.cell;
        auto * line = out + static_cast<qsizetype>(row) * grid.columns;
        for (int column = 0; column < grid.columns; column++) {
            if (line[column] >= firstStrong && lat > borderLat(grid.west + (column + 0.5) * grid.cell)) {
                line[column] = 0;
                continue;
            }
            if (line[column] < firstStrong) {
                line[column] = 0;
            } else if (haveQuality) {
                const int qc = qualityColumn[static_cast<size_t>(column)];
                const int index = qr >= 0 && qr < q.rows && qc >= 0 ? good[static_cast<qsizetype>(qr) * q.columns + qc] : 0;
                if (index == 0 || quality.valueAt(index) < 0.6) {
                    line[column] = 0;
                }
            }
        }
    }

    addPooledLevels(result, frame.grid, cleaned);
    colorize(result, frame, rotationBands());
    if (rotationMinimum() < rotationBands().front().from) {   // (a lowered minimum: the weaker cells take the first band's color)
        for (int index = 1; index < 256; index++) {
            if (frame.valueAt(index) >= rotationMinimum() && qAlpha(result.colors[index]) == 0) {
                result.colors[index] = qPremultiply(rotationBands().front().color.rgba());
            }
        }
    }
    result.utc = frame.utc;
    result.ok = true;
    return result;
}

}  // namespace MrmsOverlays
