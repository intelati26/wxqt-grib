// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "spcrefs/UtilitySpcRefs.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <mutex>
#include <QBuffer>
#include <QFont>
#include <QLineF>
#include <QPainter>
#include <QRegularExpression>
#include "objects/URL.h"
#include "radar/RadarGeomInfo.h"
#include "radar/RadarGeometryTypeEnum.h"
#include "spcrefs/LambertGrid.h"

namespace UtilitySpcRefs {
    string cycleUrl(const QDateTime& initUtc) {
        const auto utc = initUtc.toUTC();
        return "https://www.spc.noaa.gov/exper/refs/data/" + utc.toString("yyyy/MM/dd").toStdString() + "/refs-spc_" +
            utc.toString("yyyyMMdd_HHmm").toStdString() + ".zarr";
    }

    vector<QDateTime> findCycles(int wanted) {
        vector<QDateTime> found;
        auto when = QDateTime::currentDateTimeUtc();
        const int hour = (when.time().hour() / 6) * 6;
        when.setTime(QTime{hour, 0});
        for (int probe = 0; probe < 20 && static_cast<int>(found.size()) < wanted; probe += 1, when = when.addSecs(-6 * 3600)) {
            int status = 0;
            const auto bytes = URL::getBytesWithStatus(cycleUrl(when) + "/.zgroup", status);
            if (status == 200 && !bytes.isEmpty()) {
                found.push_back(when);
            }
        }
        return found;
    }

    double shownValue(const Product& p, const vector<float>& values, int column, int row) {
        if (column < 0 || column >= LambertGrid::columns || row < 0 || row >= LambertGrid::rows) {
            return std::nan("");
        }
        const float v = values[static_cast<size_t>(row) * LambertGrid::columns + column];
        if (std::isnan(v)) {
            return std::nan("");
        }
        return v * p.factor + p.offset;
    }

    namespace {
        QRgb colourFor(const Product& p, double shown, double lo, double hi) {
            if (std::isnan(shown) || shown < p.hideBelow) {
                return 0;
            }
            const auto& stops = p.stops;
            if (stops.empty()) {
                return 0;
            }
            if (p.kind == Kind::Banded) {
                if (shown < stops.front().value - 1e-9) {
                    return 0;
                }
                size_t band = 0;
                while (band + 1 < stops.size() && shown >= stops[band + 1].value - 1e-9) {
                    band += 1;
                }
                return qRgba(stops[band].r, stops[band].g, stops[band].b, 255);
            }
            // blended
            if (shown <= stops.front().value) {
                return qRgba(stops.front().r, stops.front().g, stops.front().b, 255);
            }
            for (size_t i = 1; i < stops.size(); i += 1) {
                if (shown <= stops[i].value) {
                    const double t = (shown - stops[i - 1].value) / (stops[i].value - stops[i - 1].value);
                    return qRgba(static_cast<int>(stops[i - 1].r + t * (stops[i].r - stops[i - 1].r)),
                                 static_cast<int>(stops[i - 1].g + t * (stops[i].g - stops[i - 1].g)),
                                 static_cast<int>(stops[i - 1].b + t * (stops[i].b - stops[i - 1].b)), 255);
                }
            }
            (void)lo;
            (void)hi;
            return qRgba(stops.back().r, stops.back().g, stops.back().b, 255);
        }

        // a product with no colour table gets one spanning the picture's own range
        Product withAutoStops(const Product& p, const vector<float>& values) {
            if (!p.stops.empty() || p.kind == Kind::Paintball) {
                return p;
            }
            double lo = 1e30;
            double hi = -1e30;
            for (const float v : values) {
                if (!std::isnan(v)) {
                    const double shown = v * p.factor + p.offset;
                    lo = std::min(lo, shown);
                    hi = std::max(hi, shown);
                }
            }
            Product out = p;
            out.kind = Kind::Blended;
            out.stops = rainbowStops(lo, hi > lo ? hi : lo + 1.0);
            return out;
        }
    }

    View viewForBox(const string& name, double west, double south, double east, double north, int pixelsPerCell) {
        static const LambertGrid grid;
        double minCol = 1e9, maxCol = -1e9, minRow = 1e9, maxRow = -1e9;
        // the box is a rectangle in latitude / longitude, which bends on the Lambert grid: walk its outline
        for (int i = 0; i <= 40; i += 1) {
            const double t = i / 40.0;
            const double lon = west + (east - west) * t;
            const double lat = south + (north - south) * t;
            for (const auto& [la, lo] : {std::pair<double, double>{south, lon}, {north, lon}, {lat, west}, {lat, east}}) {
                double c, r;
                grid.toGrid(la, lo, c, r);
                minCol = std::min(minCol, c);
                maxCol = std::max(maxCol, c);
                minRow = std::min(minRow, r);
                maxRow = std::max(maxRow, r);
            }
        }
        View v;
        v.name = name;
        v.col0 = std::clamp(static_cast<int>(std::floor(minCol)), 0, LambertGrid::columns - 2);
        v.row0 = std::clamp(static_cast<int>(std::floor(minRow)), 0, LambertGrid::rows - 2);
        const int col1 = std::clamp(static_cast<int>(std::ceil(maxCol)), v.col0 + 1, LambertGrid::columns - 1);
        const int row1 = std::clamp(static_cast<int>(std::ceil(maxRow)), v.row0 + 1, LambertGrid::rows - 1);
        v.cols = col1 - v.col0 + 1;
        v.rows = row1 - v.row0 + 1;
        v.scale = std::clamp(pixelsPerCell, 1, 8);
        return v;
    }

    void cellAt(const View& view, double fx, double fy, int& column, int& row) {
        column = view.col0 + static_cast<int>(std::floor(fx * view.cols));
        row = view.row0 + view.rows - 1 - static_cast<int>(std::floor(fy * view.rows));
    }

    namespace {
        float sampleAt(const vector<float>& values, double column, double row, bool smooth) {
            const int w = LambertGrid::columns;
            const int h = LambertGrid::rows;
            if (!smooth) {
                const int c = std::clamp(static_cast<int>(std::lround(column)), 0, w - 1);
                const int r = std::clamp(static_cast<int>(std::lround(row)), 0, h - 1);
                return values[static_cast<size_t>(r) * w + c];
            }
            const double cc = std::clamp(column, 0.0, w - 1.0);
            const double rr = std::clamp(row, 0.0, h - 1.0);
            const int c0 = std::min(static_cast<int>(cc), w - 2);
            const int r0 = std::min(static_cast<int>(rr), h - 2);
            const double fx = cc - c0;
            const double fy = rr - r0;
            const float a = values[static_cast<size_t>(r0) * w + c0];
            const float b = values[static_cast<size_t>(r0) * w + c0 + 1];
            const float c = values[static_cast<size_t>(r0 + 1) * w + c0];
            const float d = values[static_cast<size_t>(r0 + 1) * w + c0 + 1];
            if (std::isnan(a) || std::isnan(b) || std::isnan(c) || std::isnan(d)) {
                return fx < 0.5 ? (fy < 0.5 ? a : c) : (fy < 0.5 ? b : d);
            }
            return static_cast<float>((a * (1 - fx) + b * fx) * (1 - fy) + (c * (1 - fx) + d * fx) * fy);
        }
    }

    QImage dataImage(const Product& product, const vector<float>& values, const vector<string>& memberNames, const View& view) {
        const int outW = view.cols * view.scale;
        const int outH = view.rows * view.scale;
        QImage image{outW, outH, QImage::Format_ARGB32};
        image.fill(0);
        if (values.size() < static_cast<size_t>(LambertGrid::columns) * LambertGrid::rows) {
            return image;
        }
        const bool paintball = product.kind == Kind::Paintball;
        vector<QRgb> colours;
        if (paintball) {
            // each member that is over the threshold paints its colour over the ones before it (SPC's "paintball")
            for (size_t i = 0; i < 10; i += 1) {
                const string name = i < memberNames.size() ? memberNames[i] : string{};
                colours.push_back(memberColour(name));
            }
        }
        const auto p = paintball ? product : withAutoStops(product, values);
        for (int y = 0; y < outH; y += 1) {
            auto * out = reinterpret_cast<QRgb *>(image.scanLine(y));
            const double row = view.row0 + view.rows - (y + 0.5) / view.scale - 0.5;
            for (int x = 0; x < outW; x += 1) {
                const double column = view.col0 + (x + 0.5) / view.scale - 0.5;
                if (paintball) {
                    const float raw = sampleAt(values, column, row, false);
                    const int mask = std::isnan(raw) ? 0 : static_cast<int>(raw);
                    if (mask == 0) {
                        continue;
                    }
                    double r = 255;
                    double g = 255;
                    double b = 255;
                    for (int bit = 0; bit < 10; bit += 1) {
                        if (mask & (1 << bit)) {
                            const double alpha = 0.55;
                            r = r * (1 - alpha) + ((colours[static_cast<size_t>(bit)] >> 16) & 255) * alpha;
                            g = g * (1 - alpha) + ((colours[static_cast<size_t>(bit)] >> 8) & 255) * alpha;
                            b = b * (1 - alpha) + (colours[static_cast<size_t>(bit)] & 255) * alpha;
                        }
                    }
                    out[x] = qRgba(static_cast<int>(r), static_cast<int>(g), static_cast<int>(b), 255);
                } else {
                    const float raw = sampleAt(values, column, row, view.scale > 1);
                    out[x] = colourFor(p, std::isnan(raw) ? std::nan("") : raw * p.factor + p.offset, 0, 0);
                }
            }
        }
        return image;
    }

    namespace {
        // line segments of the radar screens' geometry files (latitude, west-positive longitude pairs)
        const vector<float>& geometry(RadarGeometryTypeEnum type) {
            static std::mutex mutex;
            static std::map<RadarGeometryTypeEnum, vector<float>> loaded;
            std::lock_guard<std::mutex> lock{mutex};
            auto& data = loaded[type];
            if (data.empty()) {
                RadarGeomInfo::loadData(RadarGeomInfo::typeToFileName.at(type), data);
            }
            return data;
        }

        void drawLines(QPainter& painter, RadarGeometryTypeEnum type, const QColor& colour, double width, const LambertGrid& grid, const View& view) {
            const auto& data = geometry(type);
            painter.setPen(QPen{colour, width});
            QVector<QLineF> lines;
            for (size_t i = 0; i + 3 < data.size(); i += 4) {
                double c1, r1, c2, r2;
                grid.toGrid(data[i], -data[i + 1], c1, r1);
                grid.toGrid(data[i + 2], -data[i + 3], c2, r2);
                const double left = view.col0 - 1.0;
                const double right = view.col0 + view.cols;
                const double bottom = view.row0 - 1.0;
                const double top = view.row0 + view.rows;
                if ((c1 < left && c2 < left) || (c1 > right && c2 > right) || (r1 < bottom && r2 < bottom) || (r1 > top && r2 > top)) {
                    continue;
                }
                const auto px = [&] (double c) { return (c - view.col0 + 0.5) * view.scale; };
                const auto py = [&] (double r) { return (view.row0 + view.rows - r - 0.5) * view.scale; };
                lines.push_back({px(c1), py(r1), px(c2), py(r2)});
            }
            painter.drawLines(lines);
        }

        void drawKey(QPainter& painter, const Product& p, const vector<string>& memberNames, int w, int h) {
            QFont font{painter.font()};
            font.setPixelSize(std::min(24, 15 + (w / 1000)));
            painter.setFont(font);
            const QFontMetrics metrics{font};
            const double y0 = h - 44 - (font.pixelSize() - 15) * 2;
            if (p.kind == Kind::Paintball) {
                double x = 10;
                painter.fillRect(QRectF{4, y0 - 6, w - 8.0, 46}, QColor{255, 255, 255, 215});
                for (size_t i = 0; i < 10 && i < memberNames.size(); i += 1) {
                    const auto name = memberDisplayName(memberNames[i]);
                    const QColor colour{static_cast<QRgb>(memberColour(memberNames[i]))};
                    painter.setPen(Qt::NoPen);
                    painter.setBrush(colour);
                    painter.drawRect(QRectF{x, y0, 18, 18});
                    painter.setPen(Qt::black);
                    painter.drawText(QPointF{x + 23, y0 + 14}, QString::fromStdString(name));
                    x += 23 + metrics.horizontalAdvance(QString::fromStdString(name)) + 22;
                }
                return;
            }
            const auto& stops = p.stops;
            if (stops.empty()) {
                return;
            }
            const double boxWidth = std::min(60.0, (w - 140.0) / static_cast<double>(stops.size()));
            painter.fillRect(QRectF{4, y0 - 6, 10 + boxWidth * stops.size() + 20 + metrics.horizontalAdvance(QString::fromStdString(p.units)) + 20, 46}, QColor{255, 255, 255, 215});
            for (size_t i = 0; i < stops.size(); i += 1) {
                const QRectF box{10 + boxWidth * static_cast<double>(i), y0, boxWidth, 16};
                painter.setPen(Qt::NoPen);
                painter.setBrush(QColor{stops[i].r, stops[i].g, stops[i].b});
                painter.drawRect(box);
                painter.setPen(Qt::black);
                painter.drawText(QRectF{box.left() - 12, y0 + 17, boxWidth + 24, 18}, Qt::AlignCenter, QString::number(stops[i].value, 'g', 3));
            }
            painter.setPen(Qt::black);
            painter.drawText(QPointF{10 + boxWidth * static_cast<double>(stops.size()) + 8, y0 + 13}, QString::fromStdString(p.units));
        }
    }

    QByteArray renderPng(const Product& product, const vector<float>& values, const vector<string>& memberNames, const View& view) {
        const int w = view.cols * view.scale;
        const int h = view.rows * view.scale;
        static const LambertGrid grid;
        QImage image{w, h, QImage::Format_ARGB32_Premultiplied};
        image.fill(QColor{244, 244, 240});
        const auto withStops = withAutoStops(product, values);
        const double line = std::max(1.0, view.scale * 0.45);   // line weights grow with the picture
        {
            QPainter painter{&image};
            painter.setRenderHint(QPainter::Antialiasing, true);
            drawLines(painter, LakeLines, QColor{190, 205, 225}, 0.8 * line, grid, view);
            drawLines(painter, CountyLines, QColor{205, 205, 200, 150}, 0.5 * line, grid, view);
            painter.drawImage(0, 0, dataImage(withStops, values, memberNames, view));
            drawLines(painter, CaLines, QColor{90, 90, 90}, 1.0 * line, grid, view);
            drawLines(painter, MxLines, QColor{90, 90, 90}, 1.0 * line, grid, view);
            drawLines(painter, StateLines, QColor{50, 50, 50}, 1.3 * line, grid, view);
            drawKey(painter, withStops, memberNames, w, h);
        }
        QByteArray png;
        QBuffer buffer{&png};
        buffer.open(QIODevice::WriteOnly);
        image.save(&buffer, "PNG");
        return png;
    }

    string readout(const Product& p, const vector<float>& values, int column, int row, const vector<string>& memberNames) {
        if (column < 0 || column >= LambertGrid::columns || row < 0 || row >= LambertGrid::rows ||
            values.size() < static_cast<size_t>(LambertGrid::columns) * LambertGrid::rows) {
            return {};
        }
        static const LambertGrid grid;
        double lat = 0.0;
        double lon = 0.0;
        grid.toLatLon(column + 0.5, row + 0.5, lat, lon);
        string text = QString{"%1%2 %3%4   "}.arg(std::fabs(lat), 0, 'f', 2).arg(lat >= 0 ? "N" : "S").arg(std::fabs(lon), 0, 'f', 2).arg(lon < 0 ? "W" : "E").toStdString();
        const float raw = values[static_cast<size_t>(row) * LambertGrid::columns + column];
        if (std::isnan(raw)) {
            return text + "no data";
        }
        if (p.kind == Kind::Paintball) {
            const int mask = static_cast<int>(raw);
            int count = 0;
            string names;
            for (int bit = 0; bit < 10; bit += 1) {
                if (mask & (1 << bit)) {
                    count += 1;
                    if (static_cast<size_t>(bit) < memberNames.size()) {
                        names += (names.empty() ? "" : ", ") + memberDisplayName(memberNames[static_cast<size_t>(bit)]);
                    }
                }
            }
            return text + std::to_string(count) + " of 10 members" + (names.empty() ? "" : ": " + names);
        }
        return text + QString::number(raw * p.factor + p.offset, 'f', p.digits).toStdString() + (p.units.empty() ? "" : " " + p.units);
    }
}
