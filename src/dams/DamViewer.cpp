// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include <array>
#include "dams/DamViewer.h"
#include <algorithm>
#include <cmath>
#include <QLocale>
#include <QPainter>
#include <QPainterPath>
#include "hurricane/ChartKit.h"
#include "hurricane/UtilityHdob.h"
#include "objects/FutureVoid.h"

namespace {
    using Series = UtilityDams::Series;

    QString number(double v, int digits = 0) {
        return QLocale{QLocale::English}.toString(v, 'f', digits);
    }
}

QString DamViewer::summary(const DamData::Latest& l) {
    if (!l.ok) {
        return "no recent data";
    }
    QString text = QString::fromStdString(UtilityHdob::timeText(l.seconds)) + ":  ";
    if (UtilityDams::has(l.outflow)) {
        text += "release " + number(l.outflow) + " cfs";
        if (UtilityDams::has(l.power) && l.power > 0.5) {
            text += " (" + number(l.power) + " through the turbines)";
        }
    }
    if (UtilityDams::has(l.generation)) {
        text += l.generation > 0.0 ? ";  generating " + number(l.generation, l.generation < 10 ? 1 : 0) + " MWh in the hour" : QString{";  not generating"};
    }
    if (UtilityDams::has(l.pool)) {
        text += ";  pool " + number(l.pool, 2) + " ft";
    }
    if (UtilityDams::has(l.tailwater)) {
        text += ", tailwater " + number(l.tailwater, 2) + " ft";
    }
    if (UtilityDams::has(l.inflow)) {
        text += ";  inflow " + number(l.inflow) + " cfs";
    }
    return text;
}

void DamChart::setData(const std::shared_ptr<DamData::Data>& newData, int newHours) {
    data = newData;
    hours = newHours;
    update();
}

void DamChart::paintEvent(QPaintEvent *) {
    QPainter p{this};
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), QColor{245, 245, 245});
    if (!data) {
        p.setPen(QColor{100, 100, 100});
        p.drawText(rect(), Qt::AlignCenter, "Loading...");
        return;
    }
    long newest = 0;
    for (const auto * s : {&data->pool, &data->tailwater, &data->outflow, &data->power, &data->inflow, &data->generation}) {
        if (!s->points.empty()) {
            newest = std::max(newest, s->points.back().seconds);
        }
    }
    if (newest == 0) {
        p.setPen(QColor{100, 100, 100});
        p.drawText(rect(), Qt::AlignCenter, data->error.empty() ? "No data" : QString::fromStdString(data->error));
        return;
    }
    const double span = hours;
    struct Line {
        const Series * series;
        QColor color;
        double width;
        bool filled;   // bars from zero (the generation)
    };
    struct Strip {
        QString title;
        QString unit;
        vector<Line> lines;
        double floor;  // the lowest the axis may go (0 for flows)
        bool fromZero;
    };
    vector<Strip> strips;
    if (!data->pool.points.empty()) {
        strips.push_back({"Pool elevation", "ft", {{&data->pool, QColor{20, 90, 180}, 2.0, false}}, -1e9, false});
    }
    if (!data->outflow.points.empty() || !data->power.points.empty()) {
        strips.push_back({"Release (blue) and flow through the turbines (orange)", "cfs", {{&data->outflow, QColor{20, 90, 180}, 2.2, false}, {&data->power, QColor{230, 120, 20}, 1.6, false}}, 0.0, true});
    }
    if (!data->generation.points.empty()) {
        strips.push_back({"Power generated each hour", "MWh", {{&data->generation, QColor{200, 150, 20}, 2.0, true}}, 0.0, true});
    }
    if (!data->inflow.points.empty()) {
        strips.push_back({"Inflow to the lake", "cfs", {{&data->inflow, QColor{40, 150, 90}, 2.0, false}}, 0.0, true});
    }
    if (!data->tailwater.points.empty()) {
        strips.push_back({"Tailwater elevation", "ft", {{&data->tailwater, QColor{110, 60, 160}, 2.0, false}}, -1e9, false});
    }
    const double left = 62.0;
    const double width = this->width() - left - 14.0;
    const double gap = 24.0;
    const double each = (this->height() - 44.0 - gap * (static_cast<double>(strips.size()) - 1)) / static_cast<double>(std::max<size_t>(1, strips.size()));
    const double xStep = span > 400 ? 120.0 : span > 200 ? 48.0 : span > 100 ? 24.0 : span > 40 ? 12.0 : 6.0;
    const auto xLabel = [&] (double x) {
        const long t = newest - static_cast<long>((span - x) * 3600.0);
        const auto text = QString::fromStdString(UtilityHdob::timeText(t));   // "07 Oct 10:00Z"
        return text.left(6) + "\n" + text.mid(7);
    };
    for (size_t index = 0; index < strips.size(); index++) {
        const auto& strip = strips[index];
        double lo = 1e18;
        double hi = -1e18;
        for (const auto& line : strip.lines) {
            for (const auto& pt : line.series->points) {
                if (static_cast<double>(newest - pt.seconds) / 3600.0 <= span) {
                    lo = std::min(lo, pt.value);
                    hi = std::max(hi, pt.value);
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
        double range = std::max(hi - lo, strip.fromZero ? 1.0 : 0.5);
        double top = hi + range * 0.08;
        double bottom = strip.fromZero ? 0.0 : lo - range * 0.08;
        const double step = (top - bottom) > 3000 ? 1000.0 : (top - bottom) > 1000 ? 500.0 : (top - bottom) > 300 ? 100.0 : (top - bottom) > 100 ? 50.0 : (top - bottom) > 30 ? 10.0 :
            (top - bottom) > 10 ? 2.0 : (top - bottom) > 3 ? 0.5 : (top - bottom) > 1 ? 0.2 : 0.1;
        if (!strip.fromZero) {
            bottom = std::floor(bottom / step) * step;
        }
        top = std::ceil(top / step) * step;
        const ChartKit::Axes a{QRectF{left, 22.0 + static_cast<double>(index) * (each + gap), width, each}, 0.0, span, bottom, top};
        ChartKit::frame(p, a, strip.title, strip.unit, step, xStep, xLabel);
        for (const auto& line : strip.lines) {
            p.setPen(QPen{line.color, line.width});
            p.setBrush(Qt::NoBrush);
            QPainterPath path;
            bool started = false;
            double lastX = -1e9;
            for (const auto& pt : line.series->points) {
                const double age = static_cast<double>(newest - pt.seconds) / 3600.0;
                if (age > span) {
                    continue;
                }
                const double x = span - age;
                const auto at = a.at(x, pt.value);
                if (line.filled) {
                    const auto base = a.at(x, 0.0);
                    p.drawLine(base, at);
                } else {
                    // a gap of more than three hours breaks the line
                    (!started || x - lastX > 3.0) ? path.moveTo(at) : path.lineTo(at);
                    started = true;
                    lastX = x;
                }
            }
            if (!line.filled) {
                p.drawPath(path);
            }
        }
    }
    p.setPen(QColor{90, 90, 90});
    QFont small = p.font();
    small.setPixelSize(10);
    p.setFont(small);
    p.drawText(QPointF{left, this->height() - 4.0}, "U.S. Army Corps of Engineers hourly data (CWMS Data API), times in UTC. Provisional: subject to revision.");
}

DamViewer::DamViewer(Window * parent, const UtilityDams::Project& project)
    : Window{parent}
    , project{&project}
    , comboRange{this, {"Last 24 hours", "Last 3 days", "Last 7 days", "Last 14 days", "Last 30 days"}}
    , textStatus{this, ""}
{
    setAttribute(Qt::WA_DeleteOnClose);
    setTitle("Dam - " + project.name);
    textStatus.setWordWrap(true);
    textStatus.getView()->setMinimumHeight(56);
    textStatus.setText(QString::fromStdString(project.name + (project.city.empty() ? "" : "  near " + project.city + ", " + project.state) + "  (Corps of Engineers " + project.office + ")") + "\nLoading...");
    chart = new DamChart{this};
    comboRange.setIndex(2);
    comboRange.connect([this] { load(); });
    box.addWidget(comboRange);
    box.addWidget(textStatus);
    box.addWidgetReal(chart, 1, Qt::Alignment{});
    box.getAndShow(this);
    resize(900, 780);
    load();
}

void DamViewer::load() {
    static const int days[] = {1, 3, 7, 14, 30};
    const int hours = days[std::clamp(comboRange.getIndex(), 0, 4)] * 24;
    const int mine = ++generation;
    auto fresh = std::make_shared<DamData::Data>();
    const auto * p = project;
    new FutureVoid{this,
        [fresh, p, hours] { DamData::loadProject(*p, hours, *fresh); },
        [this, fresh, mine, hours] {
            if (closed || mine != generation) {
                return;
            }
            data = fresh;
            chart->setData(data, hours);
            // the latest of each, in words
            DamData::Latest latest;
            latest.project = project;
            const auto last = [&] (const UtilityDams::Series& s, double& into) {
                if (!s.points.empty()) {
                    into = s.points.back().value;
                    latest.seconds = std::max(latest.seconds, s.points.back().seconds);
                }
            };
            last(data->pool, latest.pool);
            last(data->tailwater, latest.tailwater);
            last(data->outflow, latest.outflow);
            last(data->power, latest.power);
            last(data->inflow, latest.inflow);
            last(data->generation, latest.generation);
            latest.ok = latest.seconds > 0;
            QString text = QString::fromStdString(project->name + (project->city.empty() ? "" : "  near " + project->city + ", " + project->state) + "  (Corps of Engineers " + project->office + ")") +
                "\n" + summary(latest);
            // statistics over the span shown
            const auto stats = [&] (const UtilityDams::Series& s) {
                double lo = 1e18, hi = -1e18, sum = 0.0;
                for (const auto& pt : s.points) {
                    lo = std::min(lo, pt.value);
                    hi = std::max(hi, pt.value);
                    sum += pt.value;
                }
                return std::array<double, 3>{lo, sum / static_cast<double>(s.points.size()), hi};
            };
            const QString span = number(hours / 24.0, hours % 24 == 0 ? 0 : 1) + " days shown";
            if (!data->outflow.points.empty()) {
                const auto r = stats(data->outflow);
                text += "\nRelease over the " + span + ":  low " + number(r[0]) + ", average " + number(r[1]) + ", high " + number(r[2]) + " cfs";
            }
            if (!data->pool.points.empty()) {
                const auto r = stats(data->pool);
                text += "\nPool:  low " + number(r[0], 2) + ", average " + number(r[1], 2) + ", high " + number(r[2], 2) + " ft";
            }
            if (!data->generation.points.empty()) {
                double total = 0.0;
                int running = 0;
                for (const auto& pt : data->generation.points) {
                    total += pt.value;
                    running += pt.value > 0.0 ? 1 : 0;
                }
                const auto r = stats(data->generation);
                text += "\nGeneration:  " + number(total) + " MWh in the " + span + ", peak " + number(r[2], r[2] < 10 ? 1 : 0) + " MWh in an hour, running " +
                    number(100.0 * running / static_cast<double>(data->generation.points.size())) + "% of the hours";
            }
            textStatus.setText(data->error.empty() ? text : QString::fromStdString(data->error));
        }};
}
