// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "mapkit/GlobeViewer.h"
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

// the sphere
class GlobeCanvas : public QWidget {
public:
    explicit GlobeCanvas(QWidget * parent) : QWidget{parent} {
        setMouseTracking(true);
        setMinimumSize(420, 420);
    }
    std::shared_ptr<std::vector<GlobeViewer::Source>> sources;
    QString title;
    double lat0{35.0}, lon0{-95.0}, zoom{1.0};

protected:
    double radius() const { return std::min(width(), height() - 70) / 2.0 * 0.96 * zoom; }
    QPointF center() const { return QPointF{width() / 2.0, (height() - 70) / 2.0 + 28.0}; }

    // the place under a pixel: false when it is not on the globe
    bool place(const QPointF& p, double& lat, double& lon) const {
        const double R = radius();
        const double x = (p.x() - center().x()) / R, y = (center().y() - p.y()) / R;
        const double rho = std::hypot(x, y);
        if (rho > 1.0) {
            return false;
        }
        const double c = std::asin(rho), phi0 = lat0 * rad;
        if (rho < 1e-9) {
            lat = lat0;
            lon = lon0;
            return true;
        }
        lat = std::asin(std::cos(c) * std::sin(phi0) + y * std::sin(c) * std::cos(phi0) / rho) / rad;
        lon = lon0 + std::atan2(x * std::sin(c), rho * std::cos(phi0) * std::cos(c) - y * std::sin(phi0) * std::sin(c)) / rad;
        while (lon > 180.0) {
            lon -= 360.0;
        }
        while (lon < -180.0) {
            lon += 360.0;
        }
        return true;
    }

    // a place as a point of the picture; false on the far side
    bool project(double lat, double lon, QPointF& out) const {
        const double phi = lat * rad, phi0 = lat0 * rad, dl = (lon - lon0) * rad;
        const double visible = std::sin(phi0) * std::sin(phi) + std::cos(phi0) * std::cos(phi) * std::cos(dl);
        if (visible < 0.0) {
            return false;
        }
        out = QPointF{center().x() + radius() * std::cos(phi) * std::sin(dl), center().y() - radius() * (std::cos(phi0) * std::sin(phi) - std::sin(phi0) * std::cos(phi) * std::cos(dl))};
        return true;
    }

    // the globe's picture: the finest model that has a value at each place, colored by its scale
    QImage globe(double scale) const {
        const int w = static_cast<int>(width() * scale), h = static_cast<int>(height() * scale);
        QImage image(w, h, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        const double R = radius() * scale, cx = center().x() * scale, cy = center().y() * scale, phi0 = lat0 * rad;
        const double sinP0 = std::sin(phi0), cosP0 = std::cos(phi0);
        const int top = std::max(0, static_cast<int>(cy - R) - 1), bottom = std::min(h - 1, static_cast<int>(cy + R) + 1);
        const int left = std::max(0, static_cast<int>(cx - R) - 1), right = std::min(w - 1, static_cast<int>(cx + R) + 1);
        const auto& list = *sources;
        for (int py = top; py <= bottom; py++) {
            auto * row = reinterpret_cast<QRgb *>(image.scanLine(py));
            const double y = (cy - py) / R;
            for (int px = left; px <= right; px++) {
                const double x = (px - cx) / R;
                const double rho = std::hypot(x, y);
                if (rho > 1.0) {
                    continue;
                }
                const double c = std::asin(rho);
                double lat = lat0, lon = lon0;
                if (rho > 1e-9) {
                    lat = std::asin(std::cos(c) * sinP0 + y * std::sin(c) * cosP0 / rho) / rad;
                    lon = lon0 + std::atan2(x * std::sin(c), rho * cosP0 * std::cos(c) - y * sinP0 * std::sin(c)) / rad;
                }
                QRgb color = qRgb(30, 38, 52);   // the sea / land where no model has a value: dark
                for (size_t i = list.size(); i > 0; i--) {   // the finest first
                    const float v = sampleAt(list[i - 1].grid, lon, lat);
                    if (!std::isnan(v)) {
                        const QRgb c2 = list[i - 1].ramp.at(v);
                        if (qAlpha(c2) > 0) {
                            color = c2;
                            break;
                        }
                    }
                }
                // a little shading toward the edge, so that it reads as a ball
                const double shade = 0.55 + 0.45 * std::cos(c);
                row[px] = qRgb(static_cast<int>(qRed(color) * shade), static_cast<int>(qGreen(color) * shade), static_cast<int>(qBlue(color) * shade));
            }
        }
        return image;
    }

    void paintEvent(QPaintEvent *) override {
        QPainter p{this};
        p.fillRect(rect(), QColor{8, 10, 18});
        if (!sources || sources->empty()) {
            p.setPen(Qt::white);
            p.drawText(rect(), Qt::AlignCenter, "reading the models ...");
            return;
        }
        // while turning, a coarser picture (every other pixel) keeps it smooth
        const double scale = dragging ? 0.5 : 1.0;
        p.drawImage(rect(), globe(scale));
        p.setRenderHint(QPainter::Antialiasing, true);
        // the graticule every 30 degrees
        p.setPen(QPen{QColor{255, 255, 255, 40}, 1.0});
        for (int lat = -60; lat <= 60; lat += 30) {
            QPainterPath path;
            bool started = false;
            for (int lon = -180; lon <= 180; lon += 3) {
                QPointF q;
                if (project(lat, lon, q)) {
                    started ? path.lineTo(q) : path.moveTo(q);
                    started = true;
                } else {
                    started = false;
                }
            }
            p.drawPath(path);
        }
        for (int lon = -180; lon < 180; lon += 30) {
            QPainterPath path;
            bool started = false;
            for (int lat = -90; lat <= 90; lat += 3) {
                QPointF q;
                if (project(lat, lon, q)) {
                    started ? path.lineTo(q) : path.moveTo(q);
                    started = true;
                } else {
                    started = false;
                }
            }
            p.drawPath(path);
        }
        // coastlines and borders
        p.setPen(QPen{QColor{20, 20, 28, 210}, 1.0});
        for (const auto& line : Coast::borders()) {
            QPainterPath path;
            bool started = false;
            for (const auto& [lon, lat] : line) {
                QPointF q;
                if (project(lat, lon, q)) {
                    started ? path.lineTo(q) : path.moveTo(q);
                    started = true;
                } else {
                    started = false;
                }
            }
            p.drawPath(path);
        }
        // the limb
        p.setPen(QPen{QColor{200, 210, 230, 160}, 1.5});
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(center(), radius(), radius());
        // the title and the scale
        p.setPen(QColor{235, 238, 245});
        QFont bold = p.font();
        bold.setBold(true);
        p.setFont(bold);
        p.drawText(QRectF{8, 4, width() - 16.0, 22}, Qt::AlignLeft | Qt::AlignVCenter, title);
        p.setFont(font());
        legend(p);
    }

    void legend(QPainter& p) {
        const auto& finest = sources->back();
        const auto& stops = finest.ramp.stops;
        if (stops.size() < 2) {
            return;
        }
        const double barLeft = 20, barWidth = width() - 40.0, barTop = height() - 38.0;
        const size_t n = stops.size();
        for (int i = 0; i < static_cast<int>(barWidth); i++) {
            const double t = i / barWidth;
            const double v = stops.front().first + t * (stops.back().first - stops.front().first);
            p.setPen(QColor::fromRgba(finest.ramp.at(v)));
            p.drawLine(QPointF{barLeft + i, barTop}, QPointF{barLeft + i, barTop + 12});
        }
        p.setPen(QColor{235, 238, 245});
        const auto * product = GfsChart::product(param, finest.model);
        const auto quantity = product != nullptr ? product->quantity : GfsChart::Quantity::Other;
        const size_t step = std::max<size_t>(1, n / 8);
        for (size_t i = 0; i < n; i += step) {
            const double t = (stops[i].first - stops.front().first) / std::max(1e-9, stops.back().first - stops.front().first);
            const double v = GfsChart::shownValue(quantity, stops[i].first, UIPreferences::unitsF);
            p.drawText(QRectF{barLeft + t * barWidth - 24, barTop + 13, 48, 16}, Qt::AlignCenter, QString::number(v, 'f', std::abs(v) < 10 && v != std::round(v) ? 1 : 0));
        }
        const auto unit = GfsChart::unitLabel(quantity, UIPreferences::unitsF);
        if (!unit.empty()) {
            p.drawText(QRectF{barLeft, barTop - 18, 200, 16}, Qt::AlignLeft, QString::fromStdString(unit));
        }
    }

    void mousePressEvent(QMouseEvent * e) override {
        last = e->position();
        dragging = true;
    }
    void mouseReleaseEvent(QMouseEvent *) override {
        dragging = false;
        update();
    }
    void mouseMoveEvent(QMouseEvent * e) override {
        if (dragging && (e->buttons() & Qt::LeftButton)) {
            // the globe follows the pointer: the degrees a pixel is worth is 1 / radius radians
            const double perPixel = 1.0 / radius() / rad;
            lon0 -= (e->position().x() - last.x()) * perPixel / std::max(0.2, std::cos(lat0 * rad));
            lat0 = std::clamp(lat0 + (e->position().y() - last.y()) * perPixel, -90.0, 90.0);
            while (lon0 > 180.0) {
                lon0 -= 360.0;
            }
            while (lon0 < -180.0) {
                lon0 += 360.0;
            }
            last = e->position();
            update();
            return;
        }
        double lat, lon;
        if (sources && !sources->empty() && place(e->position(), lat, lon)) {
            const auto& list = *sources;
            for (size_t i = list.size(); i > 0; i--) {
                const float v = sampleAt(list[i - 1].grid, lon, lat);
                if (!std::isnan(v)) {
                    const auto * product = GfsChart::product(param, list[i - 1].model);
                    const auto quantity = product != nullptr ? product->quantity : GfsChart::Quantity::Other;
                    QToolTip::showText(e->globalPosition().toPoint(),
                        QString{"%1 (%2 km)\\n%3, %4: %5 %6"}.arg(QString::fromStdString(list[i - 1].model)).arg(list[i - 1].km, 0, 'f', 1).arg(lat, 0, 'f', 1).arg(lon, 0, 'f', 1)
                            .arg(GfsChart::shownValue(quantity, v, UIPreferences::unitsF), 0, 'f', 1).arg(QString::fromStdString(GfsChart::unitLabel(quantity, UIPreferences::unitsF))), this);
                    return;
                }
            }
        }
        QToolTip::hideText();
    }
    void wheelEvent(QWheelEvent * e) override {
        zoom = std::clamp(zoom * std::pow(1.0015, e->angleDelta().y()), 0.6, 12.0);
        update();
    }

public:
    std::string param;

private:
    QPointF last;
    bool dragging{false};
};

GlobeViewer::GlobeViewer(Window * parent, const std::string& chart, const std::vector<std::string>& modelList, const QDateTime& when, double lat, double lon)
    : Window{parent}, param{chart}, models{modelList}, valid{when.toUTC()} {
    setAttribute(Qt::WA_DeleteOnClose);
    setTitle("Globe");
    auto * page = new QWidget{this};
    auto * column = new QVBoxLayout{page};
    auto * row = new QHBoxLayout;
    back = new QPushButton{"< 3 h", page};
    forward = new QPushButton{"3 h >", page};
    status = new QLabel{page};
    row->addWidget(back);
    row->addWidget(forward);
    row->addWidget(status, 1);
    column->addLayout(row);
    canvas = new GlobeCanvas{page};
    canvas->param = param;
    canvas->lat0 = lat;
    canvas->lon0 = lon;
    column->addWidget(canvas, 1);
    setCentralWidget(page);
    setSize(1000, 900);
    QObject::connect(back, &QPushButton::clicked, this, [this] { valid = valid.addSecs(-3 * 3600); load(); });
    QObject::connect(forward, &QPushButton::clicked, this, [this] { valid = valid.addSecs(3 * 3600); load(); });
    QWidget::show();
    load();
}

// the fill grid of each model for the valid time (its own run and hour), coarse to fine: blocking, for a thread
std::vector<GlobeViewer::Source> GlobeViewer::loadSources(const std::string& chart, const std::vector<std::string>& names, const QDateTime& when) {
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
            std::string error;
            GfsRender::png(session, model, chart, "GLOBAL", s.run, hour, {}, error, nullptr, variant);
            if (!s.grid.empty()) {
                result->push_back(std::move(s));
            }
        }
    return std::move(*result);
}

// the fill grid of each model for the valid time: its own run and hour
void GlobeViewer::load() {
    const int mine = ++generation;
    status->setText("Reading " + QString::number(models.size()) + " models for " + valid.toString("ddd yyyy-MM-dd HH:mm") + "Z ...");
    auto result = std::make_shared<std::vector<Source>>();
    const auto names = models;
    const auto chart = param;
    const auto when = valid;
    new FutureVoid{this, [=] {
        *result = loadSources(chart, names, when);
    }, [=] {
        if (closed || mine != generation) {
            return;
        }
        if (result->empty()) {
            status->setText("No model has this chart for that time.");
            return;
        }
        sources = result;
        canvas->sources = sources;
        const auto * product = GfsChart::product(param, result->back().model);
        QString used;
        for (const auto& s : *result) {
            used += (used.isEmpty() ? "" : ", ") + QString::fromStdString(s.model + " " + s.run + " F" + std::to_string(s.hour));
        }
        canvas->title = QString{"%1  -  %2Z  (finest on top)"}.arg(product != nullptr ? QString::fromStdString(product->label) : QString::fromStdString(param)).arg(valid.toString("yyyy-MM-dd HH"));
        status->setText(used + ". Drag to turn, wheel to zoom.");
        canvas->update();
    }};
}

// the world on an export projection, painted from the grids at their own resolution
QImage GlobeViewer::renderWorld(const std::vector<Source>& sources, int projectionId, double centerLon, int width, std::atomic<int> * done) {
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
    return image;
}
