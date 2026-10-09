// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "ui/ChartPainter.h"
#include "obs/SurfaceHistoryViewer.h"
#include <algorithm>
#include <cmath>
#include <QPainter>
#include "hurricane/ChartKit.h"
#include "hurricane/UtilityHdob.h"
#include "objects/FutureVoid.h"
#include "obs/SurfaceViewer.h"
#include "settings/UIPreferences.h"
#include "util/UtilityIO.h"

void SurfaceHistoryChart::setData(const std::vector<UtilityMetarHistory::Ob>& newObs, bool useFahrenheit, const QString& newMessage) {
    obs = newObs;
    fahrenheit = useFahrenheit;
    message = newMessage;
    update();
}

void SurfaceHistoryChart::paintEvent(QPaintEvent *) {
    ChartPainter p{this};
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), QColor{245, 245, 245});
    if (obs.empty()) {
        p.setPen(QColor{100, 100, 100});
        p.drawText(rect(), Qt::AlignCenter, message);
        return;
    }
    using Ob = UtilityMetarHistory::Ob;
    const auto has = [] (double v) { return SurfaceStation::has(v); };
    const long newest = obs.back().seconds;
    const double span = 24.0;
    struct Line {
        std::function<double(const Ob&)> value;
        QColor color;
    };
    struct Strip {
        QString title;
        QString unit;
        std::vector<Line> lines;
        bool fromZero;
    };
    const auto degrees = [this] (double c) { return fahrenheit ? c * 1.8 + 32.0 : c; };
    std::vector<Strip> strips;
    strips.push_back({"Temperature (red) and dew point (green)", fahrenheit ? "F" : "C",
        {{[&] (const Ob& o) { return has(o.temperature) ? degrees(o.temperature) : SurfaceStation::missing; }, QColor{210, 50, 40}},
         {[&] (const Ob& o) { return has(o.dewPoint) ? degrees(o.dewPoint) : SurfaceStation::missing; }, QColor{40, 150, 70}}}, false});
    strips.push_back({"Wind speed (blue) and gust (orange)", "kt",
        {{[] (const Ob& o) { return o.windSpeed; }, QColor{30, 90, 190}}, {[] (const Ob& o) { return o.windGust; }, QColor{230, 130, 20}}}, true});
    strips.push_back({"Altimeter setting", "inHg", {{[] (const Ob& o) { return o.altimeter; }, QColor{110, 60, 160}}}, false});
    const double left = 62.0;
    const double width = this->width() - left - 14.0;
    const double gap = 26.0;
    const double each = (this->height() - 52.0 - gap * 2.0) / 3.0;
    const auto xLabel = [&] (double x) {
        const auto text = QString::fromStdString(UtilityHdob::timeText(newest - static_cast<long>((span - x) * 3600.0)));
        return text.left(2) + " " + text.mid(7);
    };
    for (size_t index = 0; index < strips.size(); index++) {
        const auto& strip = strips[index];
        double lo = 1e18;
        double hi = -1e18;
        for (const auto& line : strip.lines) {
            for (const auto& o : obs) {
                const double v = line.value(o);
                if (has(v)) {
                    lo = std::min(lo, v);
                    hi = std::max(hi, v);
                }
            }
        }
        if (lo > hi) {
            lo = 0.0;
            hi = 1.0;
        }
        if (strip.fromZero) {
            lo = 0.0;
        }
        const double range = std::max(hi - lo, strip.fromZero ? 5.0 : (strip.unit == "inHg" ? 0.1 : 4.0));
        double top = hi + range * 0.1;
        double bottom = strip.fromZero ? 0.0 : lo - range * 0.1;
        const double step = strip.unit == "inHg" ? ((top - bottom) > 0.5 ? 0.1 : 0.05) : (top - bottom) > 60 ? 10.0 : (top - bottom) > 25 ? 5.0 : (top - bottom) > 10 ? 2.0 : 1.0;
        if (!strip.fromZero) {
            bottom = std::floor(bottom / step) * step;
        }
        top = std::ceil(top / step) * step;
        const ChartKit::Axes a{QRectF{left, 24.0 + static_cast<double>(index) * (each + gap), width, each}, 0.0, span, bottom, top};
        ChartKit::frame(p, a, strip.title, strip.unit, step, 4.0, xLabel, strip.unit == "inHg" ? 2 : 0);
        for (const auto& line : strip.lines) {
            p.setPen(QPen{line.color, 2.0});
            p.setBrush(Qt::NoBrush);
            QPainterPath path;
            bool started = false;
            double lastX = -1e9;
            for (const auto& o : obs) {
                const double v = line.value(o);
                const double x = span - static_cast<double>(newest - o.seconds) / 3600.0;
                if (!has(v) || x < 0.0) {
                    continue;
                }
                const auto at = a.at(x, v);
                (!started || x - lastX > 3.0) ? path.moveTo(at) : path.lineTo(at);
                started = true;
                lastX = x;
            }
            p.drawPath(path);
        }
    }
    p.setPen(QColor{90, 90, 90});
    QFont small = p.font();
    small.setPixelSize(10);
    p.setFont(small);
    p.drawText(QPointF{left, this->height() - 6.0}, "Aviation Weather Center METARs, times in UTC. As reported.");
}

SurfaceHistoryViewer::SurfaceHistoryViewer(Window * parent, const SurfaceStation& station)
    : Window{parent}
    , textCard{this, SurfaceViewer::details(station).toStdString()}
{
    setAttribute(Qt::WA_DeleteOnClose);
    setTitle(station.id + (station.name.empty() ? "" : "  " + station.name));
    QFont mono{"monospace"};
    mono.setStyleHint(QFont::Monospace);
    textCard.getView()->setFont(mono);
    textCard.setWordWrap(true);
    chart = new SurfaceHistoryChart{this};
    box.addWidget(textCard);
    box.addWidgetReal(chart, 1, Qt::Alignment{});
    box.getAndShow(this);
    resize(760, 900);
    auto obs = std::make_shared<std::vector<UtilityMetarHistory::Ob>>();
    const auto id = station.id;
    new FutureVoid{this,
        [obs, id] { *obs = UtilityMetarHistory::parse(UtilityIO::downloadAsByteArray(UtilityMetarHistory::url(id)).toStdString()); },
        [this, obs] {
            if (!closed) {
                chart->setData(*obs, UIPreferences::unitsF, obs->empty() ? "No reports in the last 24 hours." : "");
            }
        }};
}
