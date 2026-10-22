// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "mapkit/WorldRender.h"
#include "gfs/BarbGrid.h"
#include "ui/WindBarb.h"
#include <algorithm>
#include <cmath>
#include <thread>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QTimeZone>
#include <QToolTip>
#include <QVBoxLayout>
#include <QWheelEvent>
#include "gfs/GfsModels.h"
#include "hurricane/Coast.h"
#include "mapkit/ComboChartLayer.h"
#include "mapkit/GlobeMapView.h"
#include "objects/FutureVoid.h"
#include "settings/UIPreferences.h"

namespace {
    constexpr double pi = 3.14159265358979323846;
    constexpr double rad = pi / 180.0;

    // the value of a model's grid at a place (the longitude may be in either way round)
    float sampleAt(const GfsGrid::Grid& g, double lon, double lat) {
        float v = g.sample(lon, lat);
        if (std::isnan(v)) {
            v = g.sample(lon + 360.0, lat);
        }
        if (std::isnan(v)) {
            v = g.sample(lon - 360.0, lat);
        }
        return v;
    }
}


std::vector<WorldRender::Source> WorldRender::loadSources(const std::string& chart, const std::vector<std::string>& names, const QDateTime& when) {
    auto result = std::make_shared<std::vector<Source>>();
    GfsRender::Session session;
        std::vector<std::pair<double, std::string>> order;
        for (const auto& m : names) {
            order.emplace_back(ComboChartLayer::kmOf(m), m);
        }
        std::stable_sort(order.begin(), order.end(), [] (const auto& a, const auto& b) { return a.first > b.first; });
        for (const auto& [km, model] : order) {
            const auto * def = GfsModels::find(model);
            if (def == nullptr || GfsChart::product(chart, model) == nullptr) {
                continue;
            }
            // the run: the one at or before the time on the model's own steps (an analysis is the run), the newest one when that is newer than what has been published
            const int step = def->hourlyRuns ? 1 : std::max(1, def->cycleStep);
            auto runTime = QDateTime{when.date(), QTime{when.time().hour() / step * step, 0}, QTimeZone::utc()};
            std::string cycle, date;
            if (GfsRender::latestCycle(model, cycle, "", &date) && cycle.size() >= 2) {
                const auto newest = QDateTime::fromString(QString::fromStdString(date + cycle.substr(0, 2)), "yyyyMMddHH");
                auto newestUtc = QDateTime{newest.date(), newest.time(), QTimeZone::utc()};
                if (newestUtc.isValid() && newestUtc < runTime) {
                    runTime = newestUtc;
                }
            }
            // the hour: the nearest one the model has at or before the time
            int hour = static_cast<int>(runTime.secsTo(when) / 3600);
            int best = 0;
            for (const auto& range : def->hours) {
                for (int h = range.from; h <= range.to; h += std::max(1, range.step)) {
                    if (h <= hour) {
                        best = h;
                    }
                }
            }
            hour = best;
            Source s;
            s.model = model;
            s.km = km;
            s.run = runTime.toString("yyyyMMddHH").toStdString();
            s.hour = hour;
            GfsRender::Variant variant;
            variant.fillOut = &s.grid;
            variant.rampOut = &s.ramp;
            variant.windUOut = &s.windU;
            variant.windVOut = &s.windV;
            variant.contoursOut = &s.contours;
            std::string error;
            GfsRender::png(session, model, chart, "GLOBAL", s.run, hour, {}, error, nullptr, variant);
            if (!s.grid.empty()) {
                result->push_back(std::move(s));
            }
        }
    return std::move(*result);
}

// the fill grid of each model for the valid time: its own run and hour

// the world on an export projection, painted from the grids at their own resolution
QImage WorldRender::renderWorld(const std::vector<Source>& sources, int projectionId, double centerLon, int width, std::atomic<int> * done) {
    const auto projection = static_cast<GlobeMapView::Projection>(projectionId);
    double halfW, halfH;
    GlobeMapView::extent(projection, halfW, halfH);
    const int height = static_cast<int>(std::lround(width * halfH / halfW));
    QImage image(width, height, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    const double scale = width / (2.0 * halfW);
    const QRgb land = qRgb(24, 30, 44);   // where no model has a value
    const unsigned threads = std::max(1u, std::thread::hardware_concurrency());
    std::vector<std::thread> workers;
    for (unsigned t = 0; t < threads; t++) {
        workers.emplace_back([&, t] {
            for (int py = static_cast<int>(t); py < height; py += static_cast<int>(threads)) {
                auto * row = reinterpret_cast<QRgb *>(image.scanLine(py));
                const double y = (height / 2.0 - py - 0.5) / scale;
                for (int px = 0; px < width; px++) {
                    double lat, relative;
                    if (!GlobeMapView::inverse(projection, (px + 0.5 - width / 2.0) / scale, y, lat, relative)) {
                        continue;
                    }
                    const double lon = centerLon + relative;
                    QRgb color = land;
                    for (size_t i = sources.size(); i > 0; i--) {   // the finest first
                        const float v = sampleAt(sources[i - 1].grid, lon, lat);
                        if (!std::isnan(v)) {
                            const QRgb c = sources[i - 1].ramp.at(v);
                            if (qAlpha(c) > 0) {
                                color = c;
                                break;
                            }
                        }
                    }
                    row[px] = color | 0xff000000u;
                }
                if (done != nullptr) {
                    done->fetch_add(1);
                }
            }
        });
    }
    for (auto& w : workers) {
        w.join();
    }
    // the coastlines and borders
    QPainter p{&image};
    p.setRenderHint(QPainter::Antialiasing, true);
    if (GlobeMapView::isPolar(projection) && !GlobeMapView::polarWide()) {   // the map is a circle: nothing outside it
        QPainterPath disc;
        disc.addEllipse(QRectF{0.0, 0.0, static_cast<double>(width), static_cast<double>(height)});
        p.setClipPath(disc);
    }
    p.setPen(QPen{QColor{20, 20, 28, 210}, std::max(1.0, width / 6000.0)});
    for (const auto& line : Coast::borders()) {
        QPainterPath path;
        bool started = false;
        double previous = 0.0;
        for (const auto& [lon, lat] : line) {
            double relative = lon - centerLon;
            while (relative > 180.0) {
                relative -= 360.0;
            }
            while (relative < -180.0) {
                relative += 360.0;
            }
            double x, y;
            GlobeMapView::forward(projection, lat, relative, x, y);
            const QPointF q{width / 2.0 + x * scale, height / 2.0 - y * scale};
            if (started && std::abs(relative - previous) < 180.0) {
                path.lineTo(q);
            } else {
                path.moveTo(q);
            }
            started = true;
            previous = relative;
        }
        p.drawPath(path);
    }
    // the lines of the chart (the heights): from the finest model that covers the whole world, so that no regional model adds a second set of lines over its area; every second level, as the
    // chart does on a map of the whole earth; a number on the long ones
    {
        const Source * lines = nullptr;
        for (const auto& s : sources) {   // coarse to fine: the last global one with lines
            if (s.grid.global() && !s.contours.empty()) {
                lines = &s;
            }
        }
        if (lines != nullptr) {
            const double marksLines = width / 2800.0;
            QFont labelFont{p.font()};
            labelFont.setPixelSize(std::max(10, static_cast<int>(12 * marksLines)));
            labelFont.setBold(true);
            std::vector<QRectF> placed;
            for (const auto& out : lines->contours) {
                const auto& set = out.set;
                if (set.interval <= 0.0) {
                    continue;
                }
                const double interval = set.interval * 2.0;
                const GfsGrid::Grid reduced = GfsGrid::reducedForLines(out.grid);
                const GfsGrid::Grid& grid = reduced.empty() ? out.grid : reduced;
                double lo = 1e18, hi = -1e18;
                for (int y = 0; y < grid.rows; y += 4) {
                    for (int x = 0; x < grid.columns; x += 4) {
                        const float v = grid.at(x, y);
                        if (!std::isnan(v)) {
                            lo = std::min<double>(lo, v * set.scale);
                            hi = std::max<double>(hi, v * set.scale);
                        }
                    }
                }
                if (set.onlyBelowZero) {
                    hi = std::min(hi, -interval / 2.0);
                }
                for (double level = std::max(std::ceil((lo - set.base) / interval) * interval + set.base, std::ceil((set.minimum - set.base) / interval) * interval + set.base); level <= hi; level += interval) {
                    const bool heavy = !set.uniform && std::fmod(std::abs(level - set.base), interval * 5) < 1e-6;
                    const QColor color = set.colorBelow.isValid() && level <= set.split ? set.colorBelow : set.color;
                    QPen pen{QColor{color.red(), color.green(), color.blue(), 230}, (heavy ? 1.6 : 1.0) * set.width * std::max(1.0, marksLines)};
                    if (set.dashed) {
                        pen.setStyle(Qt::DashLine);
                    }
                    p.setPen(pen);
                    p.setBrush(Qt::NoBrush);
                    for (const auto& line : GfsGrid::contour(grid, level / set.scale, -180.0, -90.0, 180.0, 90.0)) {
                        QPainterPath path;
                        bool started = false;
                        double previous = 0.0;
                        QPointF middle;
                        size_t index = 0;
                        for (const auto& pt : line) {
                            double relative = pt.lon - centerLon;
                            while (relative > 180.0) relative -= 360.0;
                            while (relative < -180.0) relative += 360.0;
                            double x, y;
                            GlobeMapView::forward(projection, pt.lat, relative, x, y);
                            const QPointF q{width / 2.0 + x * scale, height / 2.0 - y * scale};
                            if (started && std::abs(relative - previous) < 180.0) {
                                path.lineTo(q);
                            } else {
                                path.moveTo(q);
                            }
                            started = true;
                            previous = relative;
                            if (++index == line.size() / 2) {
                                middle = q;
                            }
                        }
                        p.drawPath(path);
                        if (set.labelled && path.length() > 220.0 * marksLines && QRectF{0, 0, double(width), double(height)}.adjusted(30, 30, -30, -30).contains(middle)) {
                            const QString text = QString::number(static_cast<int>(std::lround(level)));
                            const QRectF box{middle.x() - text.size() * 4.5 * marksLines - 3, middle.y() - 9 * marksLines, text.size() * 9.0 * marksLines + 6, 18 * marksLines};
                            if (!std::any_of(placed.begin(), placed.end(), [&box] (const QRectF& other) { return other.intersects(box); })) {
                                placed.push_back(box);
                                p.setFont(labelFont);
                                p.fillRect(box, QColor{255, 255, 255, 190});
                                p.setPen(QColor{30, 30, 30});
                                p.drawText(box, Qt::AlignCenter, text);
                                p.setPen(pen);
                            }
                        }
                    }
                }
            }
        }
    }
    // the wind barbs on the world grid, the size they have on a world map 2800 pixels wide (as the chart pictures are) (and the same points as the map's): each from the finest model that has the wind there
    const double marks = width / 2800.0;
    // pixels along a parallel for one degree of longitude: at the equator, and on a polar map (which stops at 20 degrees) at 55 degrees
    const bool polar = projection == GlobeMapView::Projection::PolarNorth || projection == GlobeMapView::Projection::PolarSouth;
    const double referenceLat = polar ? (projection == GlobeMapView::Projection::PolarNorth ? 55.0 : -55.0) : 0.0;
    double x0 = 0.0, y0 = 0.0, x1 = 0.0, y1 = 0.0;
    GlobeMapView::forward(projection, referenceLat, 0.0, x0, y0);
    GlobeMapView::forward(projection, referenceLat, 1.0, x1, y1);
    const double perDegree = std::hypot(x1 - x0, y1 - y0) * scale;
    bool anyWind = false;
    for (const auto& s : sources) {
        anyWind = anyWind || (!s.windU.empty() && !s.windV.empty());
    }
    if (anyWind && perDegree > 0.0) {
        const auto look = BarbGrid::look(360.0, perDegree, marks);
        const double step = look.step;
        p.setPen(QPen{QColor{20, 20, 20, 230}, std::max(1.0, marks)});
        p.setBrush(QColor{20, 20, 20});
        for (double lat = std::ceil(-85.0 / step) * step; lat <= 85.0; lat += step) {
            const double lonStep = BarbGrid::lonStep(step, lat);
            for (double lon = std::ceil(-180.0 / lonStep) * lonStep; lon < 180.0 - 1e-6; lon += lonStep) {
                double x, y;
                if (!GlobeMapView::forward(projection, lat, lon - centerLon > 180.0 ? lon - centerLon - 360.0 : (lon - centerLon < -180.0 ? lon - centerLon + 360.0 : lon - centerLon), x, y)) {
                    continue;
                }
                for (size_t i = sources.size(); i > 0; i--) {   // the finest first
                    const auto& s = sources[i - 1];
                    if (s.windU.empty() || s.windV.empty()) {
                        continue;
                    }
                    const float u = sampleAt(s.windU, lon, lat), v = sampleAt(s.windV, lon, lat);
                    if (!std::isnan(u) && !std::isnan(v)) {
                        const double from = std::fmod(std::atan2(-u, -v) * 180.0 / 3.14159265358979 + 360.0, 360.0);
                        double nx = x, ny = y;   // where north points on this map (up on the world maps, along the meridian on the polar ones)
                        GlobeMapView::forward(projection, std::min(89.9, lat + 0.5), lon - centerLon > 180.0 ? lon - centerLon - 360.0 : (lon - centerLon < -180.0 ? lon - centerLon + 360.0 : lon - centerLon), nx, ny);
                        const double tilt = std::atan2((nx - x), (ny - y)) * 180.0 / 3.14159265358979;
                        WindBarb::draw(p, QPointF{width / 2.0 + x * scale, height / 2.0 - y * scale}, std::fmod(from + tilt + 720.0, 360.0), std::hypot(u, v) * 1.943844, look.length, lat < 0.0);
                        break;
                    }
                }
            }
        }
    }
    return image;
}
