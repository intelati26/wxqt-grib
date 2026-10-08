// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "tornado/TornadoStatsViewer.h"
#include "tornado/TornadoYearsViewer.h"
#include "util/UtilityDate.h"
#include <algorithm>
#include <cmath>
#include <set>
#include <QListWidgetItem>
#include <QLocale>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QToolTip>
#include "ui/ChartExport.h"

namespace {
    using T = UtilityTornado::Tornado;
    constexpr int averageFirst = 1991;
    constexpr int averageLast = 2020;

    const char * monthNames[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

    QString dateOfDay(int day) {   // day of a non-leap year -> "10-08" (a day of any year: no year)
        return QString::fromStdString(UtilityDate::dayOfYear(day));
    }
}

TornadoChart::TornadoChart(QWidget * parent) : QWidget{parent} {
    setMinimumSize(600, 300);
    setMouseTracking(true);
    ChartExport::install(this, "Tornadoes");
}

int TornadoChart::buckets() const {
    return group == Group::DayOfYear ? 366 : (group == Group::Week || group == Group::Heatmap) ? 53 : 12;
}

void TornadoChart::setData(const std::shared_ptr<const TornadoData::Database>& newDb, const std::vector<const T *>& tornadoes, Group newGroup, Metric newMetric, bool newCumulative, const std::vector<int>& years) {
    db = newDb;
    group = newGroup;
    metric = newMetric;
    cumulative = newCumulative;
    series.clear();
    bars.clear();
    averageEach.clear();
    averageTotal.clear();
    lowest.clear();
    highest.clear();
    highlight = years;
    heat.clear();
    if (!db) {
        update();
        return;
    }
    if (group == Group::Heatmap) {
        std::map<int, std::vector<double>> weeks;
        for (const auto * t : tornadoes) {
            if (!t->counts()) {
                continue;
            }
            auto& v = weeks[t->year];
            if (v.empty()) {
                v.assign(54, 0.0);
            }
            v[static_cast<size_t>(std::clamp((t->dayOfYear - 1) / 7 + 1, 1, 53))] += UtilityTornado::value(*t, metric);
        }
        heatMax = 1.0;
        std::vector<double> mean(54, 0.0);
        for (int year = db->firstYear; year <= db->lastYear; year++) {
            auto v = weeks.count(year) != 0 ? weeks[year] : std::vector<double>(54, 0.0);
            for (size_t w = 1; w <= 53; w++) {
                heatMax = std::max(heatMax, v[w]);
                if (year >= averageFirst && year <= averageLast) {
                    mean[w] += v[w] / (averageLast - averageFirst + 1);
                }
            }
            heat.emplace_back(year, std::move(v));
        }
        heat.emplace_back(0, std::move(mean));
        update();
        return;
    }
    if (group == Group::Year || group == Group::Decade) {
        bars = UtilityTornado::buckets(tornadoes, group, metric);
        // the years (decades) with none still have a place
        std::map<int, double> filled;
        for (const auto& [k, v] : bars) {
            filled[k] = v;
        }
        bars.clear();
        const int step = group == Group::Decade ? 10 : 1;
        for (int k = (group == Group::Decade ? db->firstYear / 10 * 10 : db->firstYear); k <= db->lastYear; k += step) {
            bars.emplace_back(k, filled.count(k) != 0 ? filled[k] : 0.0);
        }
        double sum = 0.0;
        int n = 0;
        for (const auto& [year, v] : bars) {
            if (group == Group::Year && year >= averageFirst && year <= averageLast) {
                sum += v;
                n++;
            }
        }
        barAverage = n > 0 ? sum / n : 0.0;
        update();
        return;
    }
    // by bucket of the year: one array for each year
    std::map<int, std::vector<double>> perYear;
    const int n = buckets();
    for (const auto * t : tornadoes) {
        if (!t->counts()) {
            continue;
        }
        const int bucket = group == Group::DayOfYear ? t->dayOfYear : group == Group::Week ? (t->dayOfYear - 1) / 7 + 1 : t->month;
        auto& v = perYear[t->year];
        if (v.empty()) {
            v.assign(static_cast<size_t>(n) + 1, 0.0);
        }
        v[static_cast<size_t>(std::clamp(bucket, 1, n))] += UtilityTornado::value(*t, metric);
    }
    const auto totalOf = [] (const std::vector<double>& each) {
        std::vector<double> total(each.size(), 0.0);
        for (size_t i = 1; i < each.size(); i++) {
            total[i] = total[i - 1] + each[i];
        }
        return total;
    };
    averageEach.assign(static_cast<size_t>(n) + 1, 0.0);
    averageTotal.assign(static_cast<size_t>(n) + 1, 0.0);
    lowest.assign(static_cast<size_t>(n) + 1, 1e18);
    highest.assign(static_cast<size_t>(n) + 1, 0.0);
    int years30 = 0;
    for (int year = averageFirst; year <= averageLast; year++) {
        std::vector<double> each = perYear.count(year) != 0 ? perYear[year] : std::vector<double>(static_cast<size_t>(n) + 1, 0.0);
        const auto total = totalOf(each);
        for (size_t i = 0; i <= static_cast<size_t>(n); i++) {
            averageEach[i] += each[i];
            averageTotal[i] += total[i];
            lowest[i] = std::min(lowest[i], total[i]);
            highest[i] = std::max(highest[i], total[i]);
        }
        years30++;
    }
    for (size_t i = 0; i <= static_cast<size_t>(n); i++) {
        averageEach[i] /= years30;
        averageTotal[i] /= years30;
    }
    if (group == Group::DayOfYear) {   // a day is spiky: the average day over a week
        std::vector<double> smooth(averageEach.size(), 0.0);
        for (int d = 1; d <= n; d++) {
            double sum = 0.0;
            int k = 0;
            for (int o = -3; o <= 3; o++) {
                if (d + o >= 1 && d + o <= n) {
                    sum += averageEach[static_cast<size_t>(d + o)];
                    k++;
                }
            }
            smooth[static_cast<size_t>(d)] = sum / k;
        }
        averageEach = smooth;
    }
    static const QColor palette[] = {QColor{215, 40, 40}, QColor{40, 110, 210}, QColor{40, 160, 90}, QColor{150, 70, 200}, QColor{230, 140, 20}, QColor{20, 160, 170}, QColor{200, 60, 140}, QColor{110, 110, 40}};
    std::vector<int> wanted = years;
    for (size_t i = 0; i < wanted.size(); i++) {
        Series one;
        one.year = wanted[i];
        one.each = perYear.count(one.year) != 0 ? perYear[one.year] : std::vector<double>(static_cast<size_t>(n) + 1, 0.0);
        one.total = totalOf(one.each);
        one.color = palette[i % 8];
        series.push_back(std::move(one));
    }
    update();
}

QRectF TornadoChart::plot() const {
    return QRectF{58.0, 28.0, width() - 58.0 - 16.0, height() - 28.0 - 50.0};
}

// years down, weeks across, the colour the count that week on a log scale (so the quiet weeks show), the average of 1991-2020 as the bottom row
void TornadoChart::paintHeatmap(QPainter& p) {
    const QRectF area{58.0, 44.0, width() - 58.0 - 70.0, height() - 44.0 - 40.0};
    const int rows = static_cast<int>(heat.size());   // the years and the average row
    const double cw = area.width() / 53.0;
    const double ch = area.height() / (rows + 0.5);
    QFont small{p.font()};
    small.setPixelSize(10);
    QFont bold{small};
    bold.setBold(true);
    bold.setPixelSize(12);
    // white through yellow, orange and red to dark purple as the count rises: log(1 + n) / log(1 + the largest)
    const auto colorOf = [this] (double v) {
        if (v <= 0.0) {
            return QColor{238, 238, 242};
        }
        const double f = std::log(1.0 + v) / std::log(1.0 + heatMax);
        static const QColor stops[] = {QColor{255, 250, 190}, QColor{255, 210, 90}, QColor{245, 140, 40}, QColor{215, 50, 40}, QColor{140, 20, 90}, QColor{60, 10, 90}};
        const double x = std::clamp(f, 0.0, 1.0) * 5.0;
        const int i = std::min(4, static_cast<int>(x));
        const double t = x - i;
        return QColor::fromRgbF(stops[i].redF() + (stops[i + 1].redF() - stops[i].redF()) * t, stops[i].greenF() + (stops[i + 1].greenF() - stops[i].greenF()) * t,
                                stops[i].blueF() + (stops[i + 1].blueF() - stops[i].blueF()) * t);
    };
    p.setPen(Qt::NoPen);
    for (int r = 0; r < rows; r++) {
        const bool averageRow = heat[static_cast<size_t>(r)].first == 0;
        const double y = area.top() + (averageRow ? r + 0.5 : r) * ch;
        for (int w = 1; w <= 53; w++) {
            p.setBrush(colorOf(heat[static_cast<size_t>(r)].second[static_cast<size_t>(w)]));
            p.drawRect(QRectF{area.left() + (w - 1) * cw, y, cw + 0.4, ch + 0.4});
        }
    }
    p.setFont(small);
    p.setPen(QColor{70, 70, 70});
    for (int r = 0; r < rows; r++) {
        const int year = heat[static_cast<size_t>(r)].first;
        const double y = area.top() + (year == 0 ? r + 0.5 : r) * ch;
        if (year == 0) {
            p.drawText(QRectF{4, y, area.left() - 8, ch}, Qt::AlignRight | Qt::AlignVCenter, "average");
        } else if (year % 10 == 0) {
            p.drawText(QRectF{4, y - 4, area.left() - 8, 12}, Qt::AlignRight | Qt::AlignVCenter, QString::number(year));
        }
    }
    static const int starts[] = {1, 5, 9, 13, 18, 22, 26, 31, 35, 40, 44, 48};   // the week each month begins in
    for (int m = 0; m < 12; m++) {
        p.drawText(QRectF{area.left() + (starts[m] - 1) * cw, area.top() - 14, 40, 12}, Qt::AlignLeft, monthNames[m]);
    }
    p.setFont(bold);
    p.setPen(QColor{30, 30, 30});
    p.drawText(QPointF{area.left(), 16.0}, metricName(metric) + " by week of the year, every year (colour: the count that week, log scale)");
    // the scale
    p.setFont(small);
    const double legendLeft = area.right() + 14;
    for (int i = 0; i < 40; i++) {
        const double v = std::exp(std::log(1.0 + heatMax) * (1.0 - i / 39.0)) - 1.0;
        p.setPen(Qt::NoPen);
        p.setBrush(colorOf(v));
        p.drawRect(QRectF{legendLeft, area.top() + i * 4.0, 14, 4.2});
    }
    p.setPen(QColor{70, 70, 70});
    p.drawText(QPointF{legendLeft + 18, area.top() + 8}, QString::number(static_cast<long>(heatMax)));
    p.drawText(QPointF{legendLeft + 18, area.top() + 160}, "1");
    p.setPen(QColor{90, 90, 90});
    p.drawText(QPointF{area.left(), height() - 8.0}, "SPC tornado database. The rising counts of the 1950s to the 1990s are partly how tornadoes were found and recorded, not only the weather.");
}

void TornadoChart::paintEvent(QPaintEvent *) {
    QPainter p{this};
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), QColor{245, 245, 245});
    if (!db) {
        p.setPen(QColor{100, 100, 100});
        p.drawText(rect(), Qt::AlignCenter, "Loading...");
        return;
    }
    if (group == Group::Heatmap) {
        paintHeatmap(p);
        return;
    }
    const auto area = plot();
    QFont small{p.font()};
    small.setPixelSize(10);
    QFont bold{small};
    bold.setBold(true);
    bold.setPixelSize(12);
    const bool yearly = group == Group::Year || group == Group::Decade;
    const int n = buckets();
    // the scale
    double top = 1.0;
    if (yearly) {
        for (const auto& [k, v] : bars) {
            top = std::max(top, v);
        }
    } else {
        for (int i = 1; i <= n; i++) {
            const auto idx = static_cast<size_t>(i);
            if (cumulative) {
                top = std::max(top, highest[idx]);
                for (const auto& s : series) top = std::max(top, s.total[idx]);
            } else {
                top = std::max(top, averageEach[idx]);
                for (const auto& s : series) top = std::max(top, s.each[idx]);
            }
        }
    }
    const double stepsPick = top > 5000 ? 1000.0 : top > 2000 ? 500.0 : top > 1000 ? 200.0 : top > 400 ? 100.0 : top > 200 ? 50.0 : top > 80 ? 20.0 : top > 40 ? 10.0 : top > 16 ? 5.0 : top > 8 ? 2.0 : 1.0;
    top = std::ceil(top * 1.05 / stepsPick) * stepsPick;
    const double xCount = yearly ? static_cast<double>(bars.size()) : static_cast<double>(n);
    const auto xOf = [&] (double index) { return area.left() + area.width() * (index - (yearly ? 0.0 : 1.0)) / std::max(1.0, yearly ? xCount : xCount - 1.0); };   // index 1 .. n, or 0 .. count
    const auto yOf = [&] (double v) { return area.bottom() - area.height() * v / top; };
    p.setFont(small);
    p.setPen(QColor{210, 210, 210});
    p.setBrush(QColor{252, 252, 252});
    p.drawRect(area);
    for (double y = 0; y <= top + 1e-9; y += stepsPick) {
        p.setPen(QColor{232, 232, 232});
        p.drawLine(QPointF{area.left(), yOf(y)}, QPointF{area.right(), yOf(y)});
        p.setPen(QColor{70, 70, 70});
        p.drawText(QRectF{area.left() - 52, yOf(y) - 7, 48, 14}, Qt::AlignRight | Qt::AlignVCenter, QString::number(y, 'f', 0));
    }
    // the x axis
    p.setPen(QColor{70, 70, 70});
    if (group == Group::DayOfYear) {
        static const int starts[] = {1, 32, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335};
        for (int m = 0; m < 12; m++) {
            p.setPen(QColor{232, 232, 232});
            p.drawLine(QPointF{xOf(starts[m]), area.top()}, QPointF{xOf(starts[m]), area.bottom()});
            p.setPen(QColor{70, 70, 70});
            p.drawText(QRectF{xOf(starts[m]) + 2, area.bottom() + 3, 40, 14}, Qt::AlignLeft, monthNames[m]);
        }
    } else if (group == Group::Week) {
        for (int w = 1; w <= n; w += 4) {
            p.setPen(QColor{232, 232, 232});
            p.drawLine(QPointF{xOf(w), area.top()}, QPointF{xOf(w), area.bottom()});
            p.setPen(QColor{70, 70, 70});
            p.drawText(QRectF{xOf(w) - 14, area.bottom() + 3, 28, 14}, Qt::AlignHCenter, QString::number(w));
        }
        p.drawText(QRectF{area.left(), area.bottom() + 18, area.width(), 14}, Qt::AlignHCenter, "week of the year (week 1 is 1 to 7 January)");
    } else if (group == Group::Month) {
        for (int m = 1; m <= 12; m++) {
            p.drawText(QRectF{xOf(m) - 20, area.bottom() + 3, 40, 14}, Qt::AlignHCenter, monthNames[m - 1]);
        }
    } else {
        const size_t every = group == Group::Decade ? 1 : (bars.size() > 60 ? 10 : 5);
        for (size_t i = 0; i < bars.size(); i++) {
            const int k = bars[i].first;
            if (group == Group::Decade ? true : k % static_cast<int>(every) == 0) {
                const double x = area.left() + area.width() * (static_cast<double>(i) + 0.5) / xCount;
                p.drawText(QRectF{x - 22, area.bottom() + 3, 44, 14}, Qt::AlignHCenter, group == Group::Decade ? QString::number(k) + "s" : QString::number(k));
            }
        }
    }
    // the data
    p.save();
    p.setClipRect(area);
    if (yearly) {
        const double barWidth = area.width() / xCount;
        for (size_t i = 0; i < bars.size(); i++) {
            const int k = bars[i].first;
            const double v = bars[i].second;
            const bool chosen = group == Group::Year && std::find(highlight.begin(), highlight.end(), k) != highlight.end();
            p.setPen(Qt::NoPen);
            p.setBrush(chosen ? QColor{215, 40, 40} : (group == Group::Year && v > barAverage ? QColor{235, 140, 60} : (group == Group::Year ? QColor{90, 140, 210} : QColor{90, 140, 210})));
            p.drawRect(QRectF{area.left() + static_cast<double>(i) * barWidth + barWidth * 0.1, yOf(v), barWidth * 0.8, area.bottom() - yOf(v)});
        }
        if (group == Group::Year && barAverage > 0.0) {
            p.setPen(QPen{QColor{30, 30, 30}, 1.4, Qt::DashLine});
            p.drawLine(QPointF{area.left(), yOf(barAverage)}, QPointF{area.right(), yOf(barAverage)});
            p.drawText(QPointF{area.left() + 6, yOf(barAverage) - 4}, "1991-2020 average " + QString::number(barAverage, 'f', 0));
        }
    } else if (cumulative) {
        QPainterPath band;
        for (int i = 1; i <= n; i++) {
            const QPointF pt{xOf(i), yOf(highest[static_cast<size_t>(i)])};
            i == 1 ? band.moveTo(pt) : band.lineTo(pt);
        }
        for (int i = n; i >= 1; i--) {
            band.lineTo(QPointF{xOf(i), yOf(lowest[static_cast<size_t>(i)])});
        }
        band.closeSubpath();
        p.setPen(Qt::NoPen);
        p.setBrush(QColor{200, 200, 205, 150});
        p.drawPath(band);
        const auto line = [&] (const std::vector<double>& v, const QColor& c, double w, Qt::PenStyle style, int upTo) {
            QPainterPath path;
            for (int i = 1; i <= std::min(n, upTo); i++) {
                const QPointF pt{xOf(i), yOf(v[static_cast<size_t>(i)])};
                i == 1 ? path.moveTo(pt) : path.lineTo(pt);
            }
            p.setPen(QPen{c, w, style});
            p.setBrush(Qt::NoBrush);
            p.drawPath(path);
        };
        line(averageTotal, QColor{60, 60, 70}, 1.8, Qt::DashLine, n);
        for (const auto& s : series) {
            // the last year of the data stops at the date the data run to
            int upTo = n;
            if (s.year == db->lastYear) {
                const int day = UtilityTornado::dayOfYear(db->lastYear, db->lastMonth, db->lastDay);
                upTo = group == Group::DayOfYear ? day : group == Group::Week ? (day - 1) / 7 + 1 : db->lastMonth;
            }
            line(s.total, s.color, 2.4, Qt::SolidLine, upTo);
        }
    } else {
        const double slot = area.width() / std::max(1, n - 1);
        const double each = std::max(1.0, (slot * (group == Group::DayOfYear ? 1.0 : 0.8) + 0.6) / std::max<size_t>(1, series.size()));
        for (size_t k = 0; k < series.size(); k++) {
            QColor c = series[k].color;
            c.setAlpha(series.size() > 3 ? 150 : 215);
            p.setPen(Qt::NoPen);
            p.setBrush(c);
            for (int i = 1; i <= n; i++) {
                const double v = series[k].each[static_cast<size_t>(i)];
                if (v <= 0.0) {
                    continue;
                }
                p.drawRect(QRectF{xOf(i) - slot * 0.4 + each * static_cast<double>(k), yOf(v), each, area.bottom() - yOf(v)});
            }
        }
        QPainterPath mean;
        for (int i = 1; i <= n; i++) {
            const QPointF pt{xOf(i), yOf(averageEach[static_cast<size_t>(i)])};
            i == 1 ? mean.moveTo(pt) : mean.lineTo(pt);
        }
        p.setPen(QPen{QColor{60, 60, 70}, 1.8, Qt::DashLine});
        p.setBrush(Qt::NoBrush);
        p.drawPath(mean);
    }
    p.restore();
    // title and legend
    p.setFont(bold);
    p.setPen(QColor{30, 30, 30});
    static const char * groups[] = {"day of the year", "week of the year", "month", "year", "decade"};
    p.drawText(QPointF{area.left(), area.top() - 10}, metricName(metric) + " by " + groups[static_cast<int>(group)] + (!yearly ? (cumulative ? ", running total" : ", in each " + QString{group == Group::DayOfYear ? "day" : group == Group::Week ? "week" : "month"}) : QString{}));
    p.setFont(small);
    double lx = area.left() + 6;
    const double ly = area.top() + 12;
    const auto key = [&] (const QColor& color, Qt::PenStyle style, const QString& label) {
        p.setPen(QPen{color, 2.4, style});
        p.drawLine(QPointF{lx, ly}, QPointF{lx + 22, ly});
        p.setPen(QColor{50, 50, 50});
        p.drawText(QPointF{lx + 27, ly + 4}, label);
        lx += 40 + QFontMetricsF{small}.horizontalAdvance(label);
    };
    if (!yearly) {
        for (const auto& s : series) {
            key(s.color, Qt::SolidLine, QString::number(s.year));
        }
        key(QColor{60, 60, 70}, Qt::DashLine, QString{"average "} + QString::number(averageFirst) + "-" + QString::number(averageLast));
    }
    p.setPen(QColor{90, 90, 90});
    p.drawText(QPointF{area.left(), height() - 6.0}, "SPC tornado database (the actual tornadoes, one row each; a multi-state tornado counts once, in the state it began). Records before about 1990 are less complete than later ones." + QString{db->preliminaryCount > 0 ? "  " + QString::number(db->preliminaryFrom) + " on is SPC's preliminary point reports (it may count a tornado twice)." : ""});
}

void TornadoChart::mouseMoveEvent(QMouseEvent * event) {
    if (!db) {
        return;
    }
    if (group == Group::Heatmap) {
        if (heat.empty()) {
            return;
        }
        const QRectF cells{58.0, 44.0, width() - 58.0 - 70.0, height() - 44.0 - 40.0};
        const int rows = static_cast<int>(heat.size());
        const double cw = cells.width() / 53.0;
        const double ch = cells.height() / (rows + 0.5);
        const int w = 1 + static_cast<int>((event->position().x() - cells.left()) / cw);
        const double rowPosition = (event->position().y() - cells.top()) / ch;
        const int r = static_cast<int>(rowPosition >= rows - 0.5 ? rowPosition - 0.5 : rowPosition);
        if (w < 1 || w > 53 || r < 0 || r >= rows) {
            QToolTip::hideText();
            return;
        }
        const auto& row = heat[static_cast<size_t>(r)];
        const int firstDay = (w - 1) * 7 + 1;
        QToolTip::showText(event->globalPosition().toPoint(), (row.first == 0 ? QString{"average 1991-2020"} : QString::number(row.first)) + ", the week of " + dateOfDay(std::min(firstDay, 365)) + ": " +
            QString::number(row.second[static_cast<size_t>(w)], 'f', row.first == 0 ? 1 : 0), this);
        return;
    }
    const auto area = plot();
    const double fraction = (event->position().x() - area.left()) / area.width();
    if (fraction < 0.0 || fraction > 1.0) {
        QToolTip::hideText();
        return;
    }
    QString text;
    if (group == Group::Year || group == Group::Decade) {
        if (bars.empty()) {
            return;
        }
        const auto index = std::min(bars.size() - 1, static_cast<size_t>(fraction * static_cast<double>(bars.size())));
        text = QString::number(bars[index].first) + (group == Group::Decade ? "s" : "") + ": " + QLocale{QLocale::English}.toString(static_cast<qlonglong>(bars[index].second)) + " " + metricName(metric).toLower();
    } else {
        const int n = buckets();
        const int i = std::clamp(1 + static_cast<int>(std::lround(fraction * (n - 1))), 1, n);
        const auto idx = static_cast<size_t>(i);
        text = group == Group::DayOfYear ? dateOfDay(i) : group == Group::Week ? "week " + QString::number(i) : QString{monthNames[i - 1]};
        for (const auto& s : series) {
            text += "\n" + QString::number(s.year) + ": " + QString::number(cumulative ? s.total[idx] : s.each[idx], 'f', 0);
        }
        text += cumulative ? "\naverage: " + QString::number(averageTotal[idx], 'f', 0) + "  (" + QString::number(lowest[idx], 'f', 0) + " to " + QString::number(highest[idx], 'f', 0) + ")"
                           : "\naverage: " + QString::number(averageEach[idx], 'f', 1);
    }
    QToolTip::showText(event->globalPosition().toPoint(), text, this);
}

TornadoStatsViewer::TornadoStatsViewer(Window * parent, const std::shared_ptr<const TornadoData::Database>& database)
    : Window{parent}
    , comboMetric{this, {"Tornadoes", "Fatalities", "Injuries"}}
    , comboGroup{this, {"By day of the year", "By week of the year", "By month", "By year", "By decade", "Heatmap (every year by week)"}}
    , comboMode{this, {"Running total through the year", "Amount in each day / week / month"}}
    , comboRating{this, {"All tornadoes", "EF1 or stronger", "EF2 or stronger", "EF3 or stronger", "EF4 or stronger", "EF5 only"}}
    , comboState{this, {"All states"}}
    , buttonClear{this, None, "Clear the years"}
    , buttonTop{this, None, "The 5 highest"}
    , buttonRecent{this, None, "Last 5 years"}
    , buttonYears{this, None, "Years ranked..."}
    , textSummary{this, ""}
    , db{database}
{
    setAttribute(Qt::WA_DeleteOnClose);
    setTitle("Tornadoes over time - by day, week, month, year and decade");
    textSummary.setWordWrap(true);
    textSummary.getView()->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    chart = new TornadoChart{this};
    chart->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    list = new QListWidget{this};
    list->setFixedWidth(150);
    list->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    list->setToolTip("Tick the years to draw (the day, week and month views)");
    std::set<std::string> states;
    for (const auto& t : db->tornadoes) {
        states.insert(t.state);
    }
    std::vector<std::string> stateList{"All states"};
    stateList.insert(stateList.end(), states.begin(), states.end());
    comboState.setList(stateList);
    fillYears();
    QObject::connect(list, &QListWidget::itemChanged, [this] { apply(); });
    for (auto * combo : {&comboMetric, &comboGroup, &comboMode, &comboRating, &comboState}) {
        combo->connect([this] { fillYears(); apply(); });
    }
    buttonClear.connect([this] {
        filling = true;
        for (int i = 0; i < list->count(); i++) {
            list->item(i)->setCheckState(Qt::Unchecked);
        }
        filling = false;
        apply();
    });
    buttonRecent.connect([this] {
        filling = true;
        for (int i = 0; i < list->count(); i++) {
            const int year = list->item(i)->data(Qt::UserRole).toInt();
            list->item(i)->setCheckState(year > db->lastYear - 5 ? Qt::Checked : Qt::Unchecked);
        }
        filling = false;
        apply();
    });
    buttonTop.connect([this] {
        // the five years with the most (under the filters and the metric chosen)
        std::map<int, double> totals;
        const int rating = comboRating.getIndex();
        const std::string state = comboState.getIndex() > 0 ? comboState.getValue() : std::string{};
        const auto metric = static_cast<UtilityTornado::Metric>(comboMetric.getIndex());
        for (const auto& t : db->tornadoes) {
            if (!t.counts() || !UtilityTornado::passesRating(t, rating) || (!state.empty() && t.state != state)) {
                continue;
            }
            totals[t.year] += UtilityTornado::value(t, metric);
        }
        std::vector<std::pair<int, double>> order{totals.begin(), totals.end()};
        std::sort(order.begin(), order.end(), [] (const auto& a, const auto& b) { return a.second > b.second; });
        std::set<int> top;
        for (size_t i = 0; i < order.size() && i < 5; i++) {
            top.insert(order[i].first);
        }
        filling = true;
        for (int i = 0; i < list->count(); i++) {
            list->item(i)->setCheckState(top.count(list->item(i)->data(Qt::UserRole).toInt()) != 0 ? Qt::Checked : Qt::Unchecked);
        }
        filling = false;
        apply();
    });
    row.addWidget(comboMetric);
    row.addWidget(comboGroup);
    row.addWidget(comboMode);
    row.addWidget(comboRating);
    row.addWidget(comboState);
    row.addWidget(buttonClear);
    row.addWidget(buttonTop);
    buttonYears.connect([this] {
        new TornadoYearsViewer{this, db, comboRating.getIndex(), comboState.getIndex() > 0 ? std::string{comboState.getValue()} : std::string{}};
    });
    row.addWidget(buttonRecent);
    row.addWidget(buttonYears);
    row.addStretch();
    rowMain.addWidgetReal(list, 0, Qt::AlignTop | Qt::AlignLeft);
    rowMain.addWidgetReal(chart, 1, Qt::Alignment{});
    box.addLayout(row);
    box.addWidget(textSummary);
    box.addLayout(rowMain, 1);
    box.getAndShow(this);
    resize(1120, 660);
    apply();
}

// the years to tick: the newest first; the newest year of the data is ticked to begin with
void TornadoStatsViewer::fillYears() {
    const auto checked = [this] {
        std::set<int> on;
        for (int i = 0; i < list->count(); i++) {
            if (list->item(i)->checkState() == Qt::Checked) {
                on.insert(list->item(i)->data(Qt::UserRole).toInt());
            }
        }
        return on;
    }();
    filling = true;
    const bool first = list->count() == 0;
    list->clear();
    for (int y = db->lastYear; y >= db->firstYear; y--) {
        auto * item = new QListWidgetItem{QString::number(y) + (y == db->lastYear ? "  (to " + QString::fromStdString(UtilityTornado::isoDate(db->lastYear, db->lastMonth, db->lastDay)) + ")" : QString{}), list};
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(first ? (y == db->lastYear ? Qt::Checked : Qt::Unchecked) : (checked.count(y) != 0 ? Qt::Checked : Qt::Unchecked));
        item->setData(Qt::UserRole, y);
    }
    filling = false;
}

QString TornadoStatsViewer::standing(const std::vector<const T *>& tornadoes) const {
    // the newest year of the data against the average to the same day
    const int day = UtilityTornado::dayOfYear(db->lastYear, db->lastMonth, db->lastDay);
    std::map<int, double> through;
    const auto metric = static_cast<UtilityTornado::Metric>(comboMetric.getIndex());
    for (const auto * t : tornadoes) {
        if (t->counts() && t->dayOfYear <= day) {
            through[t->year] += UtilityTornado::value(*t, metric);
        }
    }
    double sum = 0.0;
    for (int y = averageFirst; y <= averageLast; y++) {
        sum += through[y];
    }
    const double average = sum / (averageLast - averageFirst + 1);
    const double now = through[db->lastYear];
    int more = 0;
    int years = 0;
    for (int y = averageFirst; y < db->lastYear; y++) {
        years++;
        more += through[y] > now ? 1 : 0;
    }
    QString text = QString::number(db->lastYear) + " through " + QString::fromStdString(UtilityTornado::isoDate(db->lastYear, db->lastMonth, db->lastDay)) + ": " + QString::number(static_cast<long>(now)) + " " + TornadoChart::metricName(metric).toLower();
    if (average > 0.5) {
        text += ", " + QString::number(std::lround(100.0 * now / average)) + " % of the " + QString::number(averageFirst) + "-" + QString::number(averageLast) + " average for the date (" + QString::number(average, 'f', 0) + ")";
    }
    const int rank = more + 1;
    const QString suffix = (rank % 100 >= 11 && rank % 100 <= 13) ? "th" : rank % 10 == 1 ? "st" : rank % 10 == 2 ? "nd" : rank % 10 == 3 ? "rd" : "th";
    text += ", " + QString::number(rank) + suffix + " highest of the " + QString::number(years + 1) + " years since " + QString::number(averageFirst) + " at this date";
    return text;
}

void TornadoStatsViewer::apply() {
    if (filling) {
        return;
    }
    const int rating = comboRating.getIndex();
    const std::string state = comboState.getIndex() > 0 ? comboState.getValue() : std::string{};
    std::vector<const T *> filtered;
    for (const auto& t : db->tornadoes) {
        if (!t.counts() || !UtilityTornado::passesRating(t, rating) || (!state.empty() && t.state != state)) {
            continue;
        }
        filtered.push_back(&t);
    }
    std::vector<int> years;
    for (int i = 0; i < list->count(); i++) {
        if (list->item(i)->checkState() == Qt::Checked) {
            years.push_back(list->item(i)->data(Qt::UserRole).toInt());
        }
    }
    const auto group = static_cast<UtilityTornado::Group>(comboGroup.getIndex());
    const bool yearly = group == UtilityTornado::Group::Year || group == UtilityTornado::Group::Decade || group == UtilityTornado::Group::Heatmap;
    comboMode.setVisible(!yearly);
    list->setEnabled(group != UtilityTornado::Group::Decade && group != UtilityTornado::Group::Heatmap);
    chart->setData(db, filtered, group, static_cast<UtilityTornado::Metric>(comboMetric.getIndex()), comboMode.getIndex() == 0, years);
    textSummary.setText(standing(filtered).toStdString() + (state.empty() ? std::string{} : "   (" + state + ")"));
}
