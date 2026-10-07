// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "buoys/BuoyViewer.h"
#include <algorithm>
#include <cmath>
#include <QPainter>
#include <QPainterPath>
#include "hurricane/ChartKit.h"
#include "objects/FutureVoid.h"

namespace {
    using Obs = UtilityBuoys::Obs;
    bool has(double v) { return UtilityBuoys::has(v); }

    QString number(double v, int digits = 0) {
        return QString::number(v, 'f', digits);
    }

    const char * compass(double degrees) {
        static const char * names[] = {"N", "NNE", "NE", "ENE", "E", "ESE", "SE", "SSE", "S", "SSW", "SW", "WSW", "W", "WNW", "NW", "NNW"};
        return names[static_cast<int>(std::lround(std::fmod(degrees + 360.0, 360.0) / 22.5)) % 16];
    }
}

QString BuoyViewer::windText(const Obs& o) {
    if (!has(o.windSpeed)) {
        return "no wind reported";
    }
    QString text = (has(o.windDirection) ? QString{"from the "} + compass(o.windDirection) + " (" + number(o.windDirection) + " deg) at " : QString{"at "}) + number(UtilityBuoys::knots(o.windSpeed)) + " kt";
    if (has(o.gust)) {
        text += ", gusts " + number(UtilityBuoys::knots(o.gust)) + " kt";
    }
    return text;
}

QString BuoyViewer::summary(const BuoyData::Marker& m) {
    const auto& o = m.obs;
    QString text = QString::fromStdString(UtilityBuoys::timeText(o.seconds)) + ":  wind " + windText(o);
    if (has(o.waveHeight)) {
        text += ";  waves " + number(UtilityBuoys::feet(o.waveHeight), 1) + " ft";
        if (has(o.dominantPeriod)) {
            text += " at " + number(o.dominantPeriod) + " s";
        }
        if (has(o.waveDirection)) {
            text += " from the " + QString{compass(o.waveDirection)};
        }
    }
    if (has(o.pressure)) {
        text += ";  pressure " + number(o.pressure, 1) + " hPa";
        if (has(o.tendency)) {
            text += " (" + QString{o.tendency >= 0 ? "+" : ""} + number(o.tendency, 1) + " in 3 h)";
        }
    }
    if (has(o.airTemperature)) {
        text += ";  air " + number(UtilityBuoys::fahrenheit(o.airTemperature)) + " F";
    }
    if (has(o.waterTemperature)) {
        text += ", water " + number(UtilityBuoys::fahrenheit(o.waterTemperature)) + " F";
    }
    return text;
}

void BuoyChart::setData(const std::shared_ptr<std::vector<Obs>>& data, double newHours) {
    series = data;
    hours = newHours;
    update();
}

void BuoyChart::paintEvent(QPaintEvent *) {
    QPainter p{this};
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), QColor{245, 245, 245});
    if (!series || series->empty()) {
        p.setPen(QColor{100, 100, 100});
        p.drawText(rect(), Qt::AlignCenter, "No data");
        return;
    }
    const long newest = series->front().seconds;
    const double span = std::min(hours, static_cast<double>(newest - series->back().seconds) / 3600.0);
    const double left = 52.0;
    const double width = this->width() - left - 14.0;
    const int strips = 5;
    const double gap = 22.0;
    const double each = (this->height() - 44.0 - gap * (strips - 1)) / strips;
    // x: hours before the newest observation, drawn left (old) to right (new)
    const double xStep = span > 240 ? 240.0 : span > 96 ? 48.0 : span > 40 ? 12.0 : 6.0;
    const auto xLabel = [&] (double x) {
        // x counts from the start of the window
        const long t = newest - static_cast<long>((span - x) * 3600.0);
        const auto text = QString::fromStdString(UtilityBuoys::timeText(t));   // "07 Oct 10:00Z"
        return text.left(6) + "\n" + text.mid(7);
    };
    int index = 0;
    const auto axes = [&] (double lo, double hi) {
        const ChartKit::Axes a{QRectF{left, 22.0 + index * (each + gap), width, each}, 0.0, span, lo, hi};
        index++;
        return a;
    };
    // the values of a field over the window as (x, value), oldest first
    const auto collect = [&] (double Obs::*field, auto convert) {
        std::vector<std::pair<double, double>> points;
        for (auto it = series->rbegin(); it != series->rend(); ++it) {
            const double age = static_cast<double>(newest - it->seconds) / 3600.0;
            if (age > span) {
                continue;
            }
            const double v = (*it).*field;
            if (has(v)) {
                points.emplace_back(span - age, convert(v));
            }
        }
        return points;
    };
    const auto identity = [] (double v) { return v; };
    const auto range = [&] (const std::vector<std::vector<std::pair<double, double>>>& all, double minSpan, double floorLow, double& lo, double& hi) {
        lo = 1e9;
        hi = -1e9;
        for (const auto& points : all) {
            for (const auto& [x, v] : points) {
                lo = std::min(lo, v);
                hi = std::max(hi, v);
            }
        }
        if (lo > hi) {
            lo = 0;
            hi = minSpan;
        }
        const double pad = std::max(minSpan - (hi - lo), 0.0) / 2.0;
        lo = std::max(floorLow, lo - pad - (hi - lo) * 0.05);
        hi = hi + pad + (hi - lo) * 0.05;
    };
    const auto draw = [&] (const ChartKit::Axes& a, const std::vector<std::pair<double, double>>& points, const QPen& pen, bool dots) {
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        QPainterPath path;
        bool started = false;
        double lastX = -1e9;
        for (const auto& [x, v] : points) {
            const auto pt = a.at(x, v);
            // a gap longer than three hours breaks the line (a station that stopped reporting)
            if (!started || x - lastX > 3.0) {
                path.moveTo(pt);
            } else {
                path.lineTo(pt);
            }
            started = true;
            lastX = x;
        }
        p.drawPath(path);
        if (dots && points.size() < 80) {
            p.setBrush(pen.color());
            for (const auto& [x, v] : points) {
                p.drawEllipse(a.at(x, v), 2.2, 2.2);
            }
        }
    };
    const auto stepFor = [] (double lo, double hi) {
        const double s = hi - lo;
        return s > 100 ? 20.0 : s > 40 ? 10.0 : s > 20 ? 5.0 : s > 8 ? 2.0 : s > 4 ? 1.0 : 0.5;
    };
    double lo = 0, hi = 0;
    // 1: waves
    {
        const auto height = collect(&Obs::waveHeight, [] (double v) { return UtilityBuoys::feet(v); });
        range({height}, 4.0, 0.0, lo, hi);
        const auto a = axes(0.0, std::ceil(hi / 2.0) * 2.0);
        ChartKit::frame(p, a, "Significant wave height (blue); hanging orange bars: dominant period, 0 to 20 s", "ft", stepFor(0, a.yMax), xStep, xLabel);
        draw(a, height, QPen{QColor{20, 90, 180}, 2.0}, true);
        const auto period = collect(&Obs::dominantPeriod, identity);
        if (!period.empty()) {
            // the dominant period as small marks hanging from the top, scaled to its own range (0 to 20 s)
            p.setPen(QPen{QColor{200, 90, 30}, 1.2});
            for (const auto& [x, v] : period) {
                const auto base = a.at(x, a.yMax);
                p.drawLine(base, base + QPointF{0, std::min(v, 20.0) / 20.0 * a.area.height() * 0.35});
            }
        }
    }
    // 2: wind
    {
        const auto wind = collect(&Obs::windSpeed, [] (double v) { return UtilityBuoys::knots(v); });
        const auto gust = collect(&Obs::gust, [] (double v) { return UtilityBuoys::knots(v); });
        range({wind, gust}, 10.0, 0.0, lo, hi);
        const auto a = axes(0.0, std::ceil(hi / 5.0) * 5.0);
        ChartKit::frame(p, a, "Wind speed (blue) and gust (red)", "kt", stepFor(0, a.yMax), xStep, xLabel);
        draw(a, gust, QPen{QColor{210, 60, 60}, 1.4}, false);
        draw(a, wind, QPen{QColor{20, 90, 180}, 2.0}, true);
    }
    // 3: pressure
    {
        const auto pressure = collect(&Obs::pressure, identity);
        range({pressure}, 8.0, 800.0, lo, hi);
        const auto a = axes(std::floor(lo), std::ceil(hi));
        ChartKit::frame(p, a, "Sea-level pressure", "hPa", stepFor(a.yMin, a.yMax), xStep, xLabel);
        draw(a, pressure, QPen{QColor{110, 50, 160}, 2.0}, true);
    }
    // 4: temperatures
    {
        const auto air = collect(&Obs::airTemperature, [] (double v) { return UtilityBuoys::fahrenheit(v); });
        const auto water = collect(&Obs::waterTemperature, [] (double v) { return UtilityBuoys::fahrenheit(v); });
        range({air, water}, 10.0, -60.0, lo, hi);
        const auto a = axes(std::floor(lo), std::ceil(hi));
        ChartKit::frame(p, a, "Air (orange) and water (blue) temperature", "F", stepFor(a.yMin, a.yMax), xStep, xLabel);
        draw(a, air, QPen{QColor{230, 120, 20}, 2.0}, false);
        draw(a, water, QPen{QColor{20, 90, 180}, 2.0}, false);
    }
    // 5: dew point
    {
        const auto dew = collect(&Obs::dewPoint, [] (double v) { return UtilityBuoys::fahrenheit(v); });
        range({dew}, 10.0, -60.0, lo, hi);
        const auto a = axes(std::floor(lo), std::ceil(hi));
        ChartKit::frame(p, a, "Dew point", "F", stepFor(a.yMin, a.yMax), xStep, xLabel);
        draw(a, dew, QPen{QColor{30, 150, 80}, 2.0}, false);
    }
    p.setPen(QColor{90, 90, 90});
    QFont small = p.font();
    small.setPixelSize(10);
    p.setFont(small);
    p.drawText(QPointF{left, this->height() - 4.0}, "NDBC realtime data, times in UTC. Gaps: the station did not report that quantity.");
}

BuoyViewer::BuoyViewer(Window * parent, const BuoyData::Marker& marker)
    : Window{parent}
    , comboRange{this, {"Last 24 hours", "Last 3 days", "Last 5 days", "All (45 days)"}}
    , textStatus{this, ""}
{
    setAttribute(Qt::WA_DeleteOnClose);
    const auto& s = marker.station;
    setTitle("Buoy " + s.id + (s.name.empty() ? "" : " - " + s.name));
    textStatus.setWordWrap(true);
    textStatus.getView()->setMinimumHeight(56);
    textStatus.setText(QString::fromStdString("Station " + s.id + (s.name.empty() ? "" : "  " + s.name) + (s.owner.empty() ? "" : "  (" + s.owner + ")")) + "\n" + summary(marker));
    chart = new BuoyChart{this};
    comboRange.setIndex(2);
    comboRange.connect([this] { showRange(); });
    box.addWidget(comboRange);
    box.addWidget(textStatus);
    box.addWidgetReal(chart, 1, Qt::Alignment{});
    box.getAndShow(this);
    resize(900, 760);
    series = std::make_shared<std::vector<Obs>>();
    auto error = std::make_shared<std::string>();
    const auto id = s.id;
    new FutureVoid{this,
        [this, id, error] { BuoyData::loadHistory(id, *series, *error); },
        [this, error] {
            if (closed) {
                return;
            }
            if (!error->empty()) {
                textStatus.setText(*error);
            }
            showRange();
        }};
}

void BuoyViewer::showRange() {
    static const double ranges[] = {24.0, 72.0, 120.0, 24.0 * 46.0};
    chart->setData(series, ranges[std::clamp(comboRange.getIndex(), 0, 3)]);
}
