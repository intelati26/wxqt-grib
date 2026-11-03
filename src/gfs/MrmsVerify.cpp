// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "gfs/MrmsVerify.h"
#include <algorithm>
#include <cmath>
#include <QColor>
#include <QPainter>
#include "hurricane/Coast.h"

using std::vector;

namespace {
    constexpr double none = -50.0;   // no echo: below every threshold
    const double missing = std::nan("");

    double ratio(double top, double bottom) {
        return bottom > 0.0 ? top / bottom : missing;
    }

    QRgb colorOf(double dbz) {
        static const struct { double dbz; int r, g, b; } stops[] = {
            {5, 160, 230, 250}, {15, 60, 160, 235}, {20, 40, 205, 70}, {30, 20, 150, 30}, {35, 250, 240, 40}, {45, 250, 160, 30}, {50, 240, 50, 30}, {60, 190, 20, 20}, {65, 220, 60, 200}, {75, 250, 250, 250},
        };
        if (!(dbz >= 5.0)) {
            return qRgb(246, 246, 246);
        }
        constexpr int count = sizeof stops / sizeof stops[0];
        for (int i = 1; i < count; i++) {
            if (dbz < stops[i].dbz) {
                return qRgb(stops[i - 1].r, stops[i - 1].g, stops[i - 1].b);
            }
        }
        return qRgb(stops[count - 1].r, stops[count - 1].g, stops[count - 1].b);
    }

    double wrapLon(double lon) {
        while (lon > 180.0) {
            lon -= 360.0;
        }
        while (lon < -180.0) {
            lon += 360.0;
        }
        return lon;
    }

    // the grid's cell at a place, or -1
    long cellAt(const GfsGrid::Grid& grid, double lon, double lat) {
        const double fy = (grid.lat0 - lat) / grid.step;
        double fx = (wrapLon(lon) - grid.lon0) / grid.step;
        if (fx < 0.0) {
            fx += 360.0 / grid.step;
        }
        const int row = static_cast<int>(std::lround(fy)), column = static_cast<int>(std::lround(fx));
        if (row < 0 || row >= grid.rows || column < 0 || column >= grid.columns) {
            return -1;
        }
        return static_cast<long>(row) * grid.columns + column;
    }
}

double MrmsVerify::Contingency::pod() const { return ratio(static_cast<double>(hits), static_cast<double>(hits + misses)); }
double MrmsVerify::Contingency::far() const { return ratio(static_cast<double>(falseAlarms), static_cast<double>(hits + falseAlarms)); }
double MrmsVerify::Contingency::csi() const { return ratio(static_cast<double>(hits), static_cast<double>(hits + misses + falseAlarms)); }
double MrmsVerify::Contingency::bias() const { return ratio(static_cast<double>(hits + falseAlarms), static_cast<double>(hits + misses)); }

GfsGrid::Grid MrmsVerify::observed(const vector<UtilityMrms::Frame>& frames, const GfsGrid::Grid& model) {
    GfsGrid::Grid out;
    out.columns = model.columns;
    out.rows = model.rows;
    out.lon0 = model.lon0;
    out.lat0 = model.lat0;
    out.step = model.step;
    out.values.assign(model.values.size(), static_cast<float>(missing));
    if (frames.empty() || model.empty()) {
        return out;
    }
    vector<QByteArray> indices;
    for (const auto& frame : frames) {
        indices.push_back(frame.indices());
    }
    const auto& g = frames.front().grid;
    const int block = std::max(1, static_cast<int>(std::lround(model.step / g.cell)));
    for (int r = 0; r < model.rows; r++) {
        const double lat = model.lat0 - r * model.step;
        const int centerRow = static_cast<int>(std::floor((g.north - lat) / g.cell));
        const int rowFrom = centerRow - block / 2, rowTo = rowFrom + block;
        if (rowFrom < 0 || rowTo > g.rows) {
            continue;
        }
        for (int c = 0; c < model.columns; c++) {
            const double lon = wrapLon(model.lon0 + c * model.step);
            const int centerColumn = static_cast<int>(std::floor((lon - g.west) / g.cell));
            const int columnFrom = centerColumn - block / 2, columnTo = columnFrom + block;
            if (columnFrom < 0 || columnTo > g.columns) {
                continue;
            }
            double best = none;
            for (size_t f = 0; f < frames.size(); f++) {
                const auto * data = reinterpret_cast<const unsigned char *>(indices[f].constData());
                for (int rr = rowFrom; rr < rowTo; rr++) {
                    for (int cc = columnFrom; cc < columnTo; cc++) {
                        if (const int index = data[static_cast<size_t>(rr) * static_cast<size_t>(g.columns) + static_cast<size_t>(cc)]; index > 0) {
                            best = std::max(best, frames[f].valueAt(index));
                        }
                    }
                }
            }
            out.values[static_cast<size_t>(r) * static_cast<size_t>(model.columns) + static_cast<size_t>(c)] = static_cast<float>(best);
        }
    }
    return out;
}

MrmsVerify::Result MrmsVerify::score(const GfsGrid::Grid& model, const GfsGrid::Grid& observedGrid, const Box& box, const vector<double>& thresholds, const vector<double>& radiiKm) {
    Result result;
    result.radiiKm = radiiKm;
    if (model.empty() || observedGrid.values.size() != model.values.size()) {
        return result;
    }
    const int row0 = std::max(0, static_cast<int>(std::ceil((model.lat0 - box.north) / model.step)));
    const int row1 = std::min(model.rows - 1, static_cast<int>(std::floor((model.lat0 - box.south) / model.step)));
    const int column0 = std::max(0, static_cast<int>(std::ceil((box.west - model.lon0) / model.step)));
    const int column1 = std::min(model.columns - 1, static_cast<int>(std::floor((box.east - model.lon0) / model.step)));
    const int height = row1 - row0 + 1, width = column1 - column0 + 1;
    if (height < 1 || width < 1) {
        return result;
    }
    // the model's value (no echo where it has none) and MRMS's in the box, and whether the cell counts (MRMS covers it)
    vector<float> m(static_cast<size_t>(height) * static_cast<size_t>(width)), o(m.size());
    vector<char> valid(m.size(), 0);
    for (int r = 0; r < height; r++) {
        for (int c = 0; c < width; c++) {
            const size_t from = static_cast<size_t>(row0 + r) * static_cast<size_t>(model.columns) + static_cast<size_t>(column0 + c);
            const size_t to = static_cast<size_t>(r) * static_cast<size_t>(width) + static_cast<size_t>(c);
            const float obs = observedGrid.values[from], mod = model.values[from];
            valid[to] = std::isfinite(obs) ? 1 : 0;
            o[to] = obs;
            m[to] = std::isfinite(mod) ? mod : static_cast<float>(none);
            result.cells += valid[to];
        }
    }
    // the sum over a window from an integral table
    const size_t stride = static_cast<size_t>(width) + 1;
    const auto table = [&] (const auto& value) {
        vector<double> sum((static_cast<size_t>(height) + 1) * stride, 0.0);
        for (int r = 0; r < height; r++) {
            double row = 0.0;
            for (int c = 0; c < width; c++) {
                row += value(static_cast<size_t>(r) * static_cast<size_t>(width) + static_cast<size_t>(c));
                sum[static_cast<size_t>(r + 1) * stride + static_cast<size_t>(c + 1)] = sum[static_cast<size_t>(r) * stride + static_cast<size_t>(c + 1)] + row;
            }
        }
        return sum;
    };
    const auto window = [&] (const vector<double>& sum, int r, int c, int half) {
        const size_t r0 = static_cast<size_t>(std::max(0, r - half)), r1 = static_cast<size_t>(std::min(height, r + half + 1));
        const size_t c0 = static_cast<size_t>(std::max(0, c - half)), c1 = static_cast<size_t>(std::min(width, c + half + 1));
        return sum[r1 * stride + c1] - sum[r0 * stride + c1] - sum[r1 * stride + c0] + sum[r0 * stride + c0];
    };
    const auto countTable = table([&] (size_t i) { return valid[i] ? 1.0 : 0.0; });
    for (const double dbz : thresholds) {
        Score score;
        score.dbz = dbz;
        const auto mTable = table([&] (size_t i) { return valid[i] && m[i] >= dbz ? 1.0 : 0.0; });
        const auto oTable = table([&] (size_t i) { return valid[i] && o[i] >= dbz ? 1.0 : 0.0; });
        for (size_t i = 0; i < m.size(); i++) {
            if (!valid[i]) {
                continue;
            }
            const bool a = m[i] >= dbz, b = o[i] >= dbz;
            (a && b ? score.table.hits : b ? score.table.misses : a ? score.table.falseAlarms : score.table.correctNegatives)++;
        }
        for (const double km : radiiKm) {
            const int half = std::max(0, static_cast<int>(std::lround(km / (model.step * 111.2))));
            double numerator = 0.0, denominator = 0.0;
            for (int r = 0; r < height; r++) {
                for (int c = 0; c < width; c++) {
                    if (!valid[static_cast<size_t>(r) * static_cast<size_t>(width) + static_cast<size_t>(c)]) {
                        continue;
                    }
                    const double count = window(countTable, r, c, half);
                    const double pm = window(mTable, r, c, half) / count, po = window(oTable, r, c, half) / count;
                    numerator += (pm - po) * (pm - po);
                    denominator += pm * pm + po * po;
                }
            }
            score.fss.push_back(denominator > 0.0 ? 1.0 - numerator / denominator : missing);
        }
        result.scores.push_back(std::move(score));
    }
    return result;
}

QImage MrmsVerify::picture(const GfsGrid::Grid& model, const GfsGrid::Grid& observedGrid, const Box& box, double threshold, const QString& modelTitle, const QString& observedTitle, int panelWidth) {
    const double midLat = (box.south + box.north) / 2.0;
    const double aspect = (box.north - box.south) / std::max(0.1, (box.east - box.west) * std::cos(midLat * M_PI / 180.0));
    const int panelHeight = std::max(60, static_cast<int>(panelWidth * aspect));
    constexpr int top = 24, gap = 8, bottom = 22;
    QImage image{3 * panelWidth + 2 * gap, top + panelHeight + bottom, QImage::Format_RGB32};
    image.fill(qRgb(255, 255, 255));
    for (int panel = 0; panel < 3; panel++) {
        const int left = panel * (panelWidth + gap);
        for (int y = 0; y < panelHeight; y++) {
            const double lat = box.north - (box.north - box.south) * (y + 0.5) / panelHeight;
            for (int x = 0; x < panelWidth; x++) {
                const double lon = box.west + (box.east - box.west) * (x + 0.5) / panelWidth;
                const long cell = cellAt(model, lon, lat);
                if (cell < 0) {
                    image.setPixel(left + x, top + y, qRgb(230, 230, 230));
                    continue;
                }
                const float mod = model.values[static_cast<size_t>(cell)], obs = observedGrid.values[static_cast<size_t>(cell)];
                const double modelValue = std::isfinite(mod) ? mod : none;
                QRgb color;
                if (panel == 0) {
                    color = colorOf(modelValue);
                } else if (panel == 1) {
                    color = std::isfinite(obs) ? colorOf(obs) : qRgb(230, 230, 230);
                } else if (!std::isfinite(obs)) {
                    color = qRgb(230, 230, 230);
                } else {
                    const bool a = modelValue >= threshold, b = obs >= threshold;
                    color = a && b ? qRgb(40, 170, 60) : b ? qRgb(225, 50, 50) : a ? qRgb(60, 110, 230) : qRgb(246, 246, 246);
                }
                image.setPixel(left + x, top + y, color);
            }
        }
    }
    QPainter p{&image};
    p.setRenderHint(QPainter::Antialiasing);
    for (int panel = 0; panel < 3; panel++) {
        const int left = panel * (panelWidth + gap);
        p.setClipRect(left, top, panelWidth, panelHeight);
        p.setPen(QPen{QColor{40, 40, 40, 200}, 0.8});
        for (const auto& line : Coast::borders()) {
            QPolygonF shape;
            for (const auto& [lon, lat] : line) {
                shape << QPointF{left + (lon - box.west) / (box.east - box.west) * panelWidth, top + (box.north - lat) / (box.north - box.south) * panelHeight};
            }
            p.drawPolyline(shape);
        }
        p.setClipping(false);
        p.setPen(Qt::black);
        p.drawRect(left, top, panelWidth - 1, panelHeight - 1);
    }
    QFont font = p.font();
    font.setPixelSize(13);
    font.setBold(true);
    p.setFont(font);
    p.drawText(0 * (panelWidth + gap) + 2, 17, modelTitle);
    p.drawText(1 * (panelWidth + gap) + 2, 17, observedTitle);
    p.drawText(2 * (panelWidth + gap) + 2, 17, QString{"Agreement at %1 dBZ"}.arg(threshold, 0, 'f', 0));
    font.setBold(false);
    font.setPixelSize(12);
    p.setFont(font);
    const int keyLeft = 2 * (panelWidth + gap);
    const auto key = [&] (int slot, QRgb color, const QString& text) {
        p.fillRect(keyLeft + slot * 78, top + panelHeight + 6, 10, 10, QColor{color});
        p.drawText(keyLeft + slot * 78 + 14, top + panelHeight + 16, text);
    };
    key(0, qRgb(40, 170, 60), "hit");
    key(1, qRgb(225, 50, 50), "miss");
    key(2, qRgb(60, 110, 230), "false alarm");
    return image;
}
