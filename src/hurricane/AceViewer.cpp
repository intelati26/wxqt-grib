// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "hurricane/AceViewer.h"
#include <algorithm>
#include <cmath>
#include <QListWidgetItem>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QToolTip>

namespace {
    constexpr int firstDay = 121;   // 1 May
    constexpr int lastDayShown = 365;

    QString dateOf(int day) {   // day of a non-leap year -> "8 Oct"
        static const int before[] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365};
        static const char * names[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
        for (int m = 0; m < 12; m++) {
            if (day <= before[m + 1]) {
                return QString::number(day - before[m]) + " " + names[m];
            }
        }
        return "31 Dec";
    }

    int daysOfNewest(const std::vector<UtilitySeason::Storm>& storms) {
        int newest = 0;
        for (const auto& s : storms) {
            if (s.last.size() >= 8) {
                newest = std::max(newest, UtilitySeason::dayOfYear(s.last.substr(0, 8)));
            }
        }
        return newest;
    }
}

int AceChart::lastDay(const HurricaneData::SeasonData& data) {
    // the newest of the best-track records of this season
    return daysOfNewest(data.current);
}

void AceChart::setData(const std::shared_ptr<HurricaneData::SeasonData>& newData, Metric newMetric, bool newDaily, const std::vector<int>& years) {
    data = newData;
    metric = newMetric;
    daily = newDaily;
    series.clear();
    meanPerDay.clear();
    climatology = UtilitySeason::Climatology{};
    if (data && data->error.empty()) {
        static const QColor palette[] = {QColor{215, 40, 40}, QColor{40, 110, 210}, QColor{40, 160, 90}, QColor{150, 70, 200}, QColor{230, 140, 20}, QColor{20, 160, 170}, QColor{200, 60, 140}, QColor{110, 110, 40}};
        std::vector<int> wanted = years;
        if (wanted.empty()) {
            wanted.push_back(data->currentYear);
        }
        for (size_t i = 0; i < wanted.size(); i++) {
            Series one;
            one.year = wanted[i];
            const bool current = one.year == data->currentYear;
            one.total = UtilitySeason::cumulativeByDay(current ? data->current : data->history, one.year, metric);
            one.perDay.assign(367, 0.0);
            for (size_t d = 1; d < one.total.size(); d++) {
                one.perDay[d] = one.total[d] - one.total[d - 1];
            }
            one.color = palette[i % 8];
            series.push_back(std::move(one));
        }
        const int first = metric == Metric::Ace ? 1991 : 2004;
        climatology = UtilitySeason::climatology(data->history, first, 2020, metric);
        meanPerDay.assign(367, 0.0);
        for (size_t d = 1; d < 367; d++) {
            meanPerDay[d] = climatology.mean[d] - climatology.mean[d - 1];
        }
        // the average day is smoothed over a week, or it is a spiky line
        std::vector<double> smooth(367, 0.0);
        for (int d = 1; d < 367; d++) {
            double sum = 0.0;
            int n = 0;
            for (int k = -3; k <= 3; k++) {
                if (d + k >= 1 && d + k <= 366) {
                    sum += meanPerDay[static_cast<size_t>(d + k)];
                    n++;
                }
            }
            smooth[static_cast<size_t>(d)] = n > 0 ? sum / n : 0.0;
        }
        meanPerDay = smooth;
        today = lastDay(*data);
    }
    update();
}

QString AceChart::standing(const HurricaneData::SeasonData& data, Metric metric) {
    if (!data.error.empty() || data.current.empty()) {
        return {};
    }
    const int day = lastDay(data);
    if (day <= 0) {
        return {};
    }
    const bool tike = metric == Metric::Tike;
    bool anyRadii = false;
    for (const auto& s : data.current) {
        anyRadii = anyRadii || s.hasRadii;
    }
    if (tike && !anyRadii) {
        return {};
    }
    const auto now = UtilitySeason::cumulativeByDay(data.current, data.currentYear, metric);
    const int first = tike ? 2004 : 1991;
    const auto clim = UtilitySeason::climatology(data.history, first, 2020, metric);
    const double value = now[static_cast<size_t>(day)];
    const double normal = clim.mean[static_cast<size_t>(day)];
    // how many of the years since 1950 (2004 for TIKE) had more by this day
    int more = 0;
    int years = 0;
    for (int year = tike ? 2004 : 1950; year < data.currentYear; year++) {
        const auto c = UtilitySeason::cumulativeByDay(data.history, year, metric);
        years++;
        more += c[static_cast<size_t>(day)] > value ? 1 : 0;
    }
    const QString unit = tike ? " TJ" : "";
    QString text = unitsName(metric) + " " + QString::number(value, 'f', tike ? 0 : 1) + unit + " through " + dateOf(day) + ": ";
    if (normal > 0.5) {
        text += QString::number(std::lround(100.0 * value / normal)) + " % of the " + QString::number(first) + "-2020 average for the date (" + QString::number(normal, 'f', tike ? 0 : 1) + unit + ")";
    } else {
        text += "the " + QString::number(first) + "-2020 average for the date is " + QString::number(normal, 'f', tike ? 0 : 1) + unit;
    }
    if (years > 0) {
        text += ", " + QString::number(more + 1) + QString{more + 1 == 1 ? "st" : more + 1 == 2 ? "nd" : more + 1 == 3 ? "rd" : "th"} + " highest of the " + QString::number(years + 1) + " seasons since " +
            QString::number(tike ? 2004 : 1950) + " at this date";
    }
    return text;
}

QRectF AceChart::plot() const {
    return QRectF{54.0, 26.0, width() - 54.0 - 14.0, (height() - 26.0 - 48.0) * 0.72};
}

QRectF AceChart::barsArea() const {
    const auto p = plot();
    return QRectF{p.left(), p.bottom() + 22.0, p.width(), (height() - 26.0 - 48.0) * 0.28 - 6.0};
}

void AceChart::paintEvent(QPaintEvent *) {
    QPainter p{this};
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), QColor{245, 245, 245});
    if (!data || !data->error.empty() || series.empty()) {
        p.setPen(QColor{100, 100, 100});
        p.drawText(rect(), Qt::AlignCenter, data && !data->error.empty() ? QString::fromStdString(data->error) : "Loading...");
        return;
    }
    const bool tike = metric == Metric::Tike;
    const int firstClimatology = tike ? 2004 : 1991;
    const auto area = daily ? QRectF{54.0, 26.0, width() - 54.0 - 14.0, height() - 26.0 - 48.0} : plot();
    const auto bars = barsArea();
    // the largest value drawn sets the scale
    double top = 1.0;
    for (size_t d = firstDay; d <= lastDayShown; d++) {
        if (daily) {
            top = std::max(top, meanPerDay[d]);
            for (const auto& one : series) {
                top = std::max(top, one.perDay[d]);
            }
        } else {
            top = std::max(top, climatology.highest[d]);
            for (const auto& one : series) {
                top = std::max(top, one.total[d]);
            }
        }
    }
    const double step = tike ? (top > 20000 ? 5000.0 : top > 8000 ? 2000.0 : top > 3000 ? 1000.0 : top > 1000 ? 500.0 : top > 400 ? 100.0 : 50.0)
                             : (top > 400 ? 100.0 : top > 200 ? 50.0 : top > 100 ? 25.0 : top > 40 ? 10.0 : top > 16 ? 5.0 : top > 8 ? 2.0 : 1.0);
    top = std::ceil(top * 1.05 / step) * step;
    const auto xOf = [&] (double day) { return area.left() + area.width() * (day - firstDay) / (lastDayShown - firstDay); };
    const auto yOf = [&] (double value) { return area.bottom() - area.height() * value / top; };
    QFont small{p.font()};
    small.setPixelSize(10);
    p.setFont(small);
    p.setPen(QColor{210, 210, 210});
    p.setBrush(QColor{252, 252, 252});
    p.drawRect(area);
    for (double y = 0; y <= top + 1e-9; y += step) {
        p.setPen(QColor{232, 232, 232});
        p.drawLine(QPointF{area.left(), yOf(y)}, QPointF{area.right(), yOf(y)});
        p.setPen(QColor{70, 70, 70});
        p.drawText(QRectF{area.left() - 46, yOf(y) - 7, 42, 14}, Qt::AlignRight | Qt::AlignVCenter, QString::number(y, 'f', 0));
    }
    static const int months[] = {121, 152, 182, 213, 244, 274, 305, 335};
    static const char * monthNames[] = {"May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    for (int i = 0; i < 8; i++) {
        p.setPen(QColor{232, 232, 232});
        p.drawLine(QPointF{xOf(months[i]), area.top()}, QPointF{xOf(months[i]), daily ? area.bottom() : bars.bottom()});
        p.setPen(QColor{70, 70, 70});
        p.drawText(QRectF{xOf(months[i]) + 2, area.bottom() + 3, 40, 14}, Qt::AlignLeft, monthNames[i]);
    }
    if (!daily) {
        // the band of the lowest and highest of the average's years, and its mean
        QPainterPath band;
        for (int d = firstDay; d <= lastDayShown; d++) {
            const QPointF pt{xOf(d), yOf(climatology.highest[static_cast<size_t>(d)])};
            d == firstDay ? band.moveTo(pt) : band.lineTo(pt);
        }
        for (int d = lastDayShown; d >= firstDay; d--) {
            band.lineTo(QPointF{xOf(d), yOf(climatology.lowest[static_cast<size_t>(d)])});
        }
        band.closeSubpath();
        p.setPen(Qt::NoPen);
        p.setBrush(QColor{200, 200, 205, 150});
        p.drawPath(band);
        const auto line = [&] (const std::vector<double>& values, int upTo, const QColor& color, double width, Qt::PenStyle style) {
            QPainterPath path;
            for (int d = firstDay; d <= std::min(upTo, lastDayShown); d++) {
                const QPointF pt{xOf(d), yOf(values[static_cast<size_t>(d)])};
                d == firstDay ? path.moveTo(pt) : path.lineTo(pt);
            }
            p.setPen(QPen{color, width, style});
            p.setBrush(Qt::NoBrush);
            p.drawPath(path);
        };
        line(climatology.mean, lastDayShown, QColor{60, 60, 70}, 1.8, Qt::DashLine);
        for (const auto& one : series) {
            const bool now = one.year == data->currentYear;
            line(one.total, now && today > 0 ? today : lastDayShown, one.color, now ? 2.8 : 2.0, Qt::SolidLine);
            if (now && today >= firstDay && today <= lastDayShown) {
                p.setBrush(one.color);
                p.setPen(QColor{255, 255, 255});
                p.drawEllipse(QPointF{xOf(today), yOf(one.total[static_cast<size_t>(today)])}, 4.0, 4.0);
            }
        }
        // the amount added each day by the first season chosen, underneath
        const auto& lead = series.front();
        double barTop = 1.0;
        for (size_t d = firstDay; d <= lastDayShown; d++) {
            barTop = std::max(barTop, lead.perDay[d]);
        }
        p.setPen(QColor{210, 210, 210});
        p.setBrush(QColor{252, 252, 252});
        p.drawRect(bars);
        for (int d = firstDay; d <= lastDayShown; d++) {
            const double v = lead.perDay[static_cast<size_t>(d)];
            if (v <= 0.0) {
                continue;
            }
            const double h = bars.height() * v / barTop;
            p.setPen(Qt::NoPen);
            QColor c = lead.color;
            c.setAlpha(210);
            p.setBrush(c);
            p.drawRect(QRectF{xOf(d) - 1.0, bars.bottom() - h, std::max(2.0, area.width() / (lastDayShown - firstDay)), h});
        }
        p.setPen(QColor{70, 70, 70});
        p.setFont(small);
        p.drawText(QRectF{bars.left() + 4, bars.top() + 1, 320, 12}, Qt::AlignLeft, unitsName(metric) + " added each day in " + QString::number(lead.year) + " (peak " + QString::number(barTop, 'f', tike ? 0 : 1) + ")");
    } else {
        // the amount of each day: a thin bar for each season chosen, the seasons side by side within the day when there are few, over each other when many
        const double daySpan = area.width() / (lastDayShown - firstDay);
        const double each = std::max(1.0, (daySpan + 0.6) / static_cast<double>(series.size()));
        for (size_t k = 0; k < series.size(); k++) {
            QColor c = series[k].color;
            c.setAlpha(series.size() > 3 ? 150 : 215);
            p.setPen(Qt::NoPen);
            p.setBrush(c);
            for (int d = firstDay; d <= lastDayShown; d++) {
                const double v = series[k].perDay[static_cast<size_t>(d)];
                if (v <= 0.0) {
                    continue;
                }
                p.drawRect(QRectF{xOf(d) - 0.3 + each * static_cast<double>(k), yOf(v), each, area.bottom() - yOf(v)});
            }
        }
        QPainterPath mean;
        for (int d = firstDay; d <= lastDayShown; d++) {
            const QPointF pt{xOf(d), yOf(meanPerDay[static_cast<size_t>(d)])};
            d == firstDay ? mean.moveTo(pt) : mean.lineTo(pt);
        }
        p.setPen(QPen{QColor{60, 60, 70}, 1.8, Qt::DashLine});
        p.setBrush(Qt::NoBrush);
        p.drawPath(mean);
    }
    if (today >= firstDay && today <= lastDayShown && std::any_of(series.begin(), series.end(), [&] (const Series& one) { return one.year == data->currentYear; })) {
        p.setPen(QPen{QColor{215, 40, 40}, 1.0, Qt::DotLine});
        p.drawLine(QPointF{xOf(today), area.top()}, QPointF{xOf(today), area.bottom()});
    }
    // title and legend
    QFont bold{small};
    bold.setBold(true);
    bold.setPixelSize(12);
    p.setFont(bold);
    p.setPen(QColor{30, 30, 30});
    p.drawText(QPointF{area.left(), area.top() - 8}, unitsName(metric) + (daily ? " added each day, " : " through the year, ") + (data->basin == "al" ? "Atlantic" : "East and Central Pacific") +
        (tike ? "  (TJ, estimated from the wind radii)" : QString{}));
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
    for (const auto& one : series) {
        key(one.color, Qt::SolidLine, QString::number(one.year));
    }
    key(QColor{60, 60, 70}, Qt::DashLine, (daily ? "average day " : "average ") + QString::number(firstClimatology) + "-2020");
    if (!daily) {
        p.setPen(Qt::NoPen);
        p.setBrush(QColor{200, 200, 205, 200});
        p.drawRect(QRectF{lx, ly - 5, 22, 10});
        p.setPen(QColor{50, 50, 50});
        p.drawText(QPointF{lx + 27, ly + 4}, "lowest to highest of those years");
    }
    p.setPen(QColor{90, 90, 90});
    p.drawText(QPointF{area.left(), height() - 6.0}, tike ? "HURDAT2 wind radii (2004 on) and this season's ATCF best tracks; TIKE is an estimate: the kinetic energy of the 34 / 50 / 64 kt wind areas of each 6-hourly record, added up."
                                                       : "HURDAT2 (NHC best-track database) and this season's ATCF best tracks; the ACE of each 6-hourly record at 34 kt or more, as reported.");
}

void AceChart::mouseMoveEvent(QMouseEvent * event) {
    if (!data || series.empty()) {
        return;
    }
    const auto area = daily ? QRectF{54.0, 26.0, width() - 54.0 - 14.0, height() - 26.0 - 48.0} : plot();
    const double fraction = (event->position().x() - area.left()) / area.width();
    const int day = firstDay + static_cast<int>(std::lround(fraction * (lastDayShown - firstDay)));
    if (fraction < 0.0 || fraction > 1.0 || day < firstDay || day > lastDayShown) {
        QToolTip::hideText();
        return;
    }
    const auto d = static_cast<size_t>(day);
    const bool tike = metric == Metric::Tike;
    const int digits = tike ? 0 : 1;
    QString text = dateOf(day);
    for (const auto& one : series) {
        if (one.year == data->currentYear && today > 0 && day > today) {
            continue;
        }
        text += "\n" + QString::number(one.year) + ": " + QString::number(daily ? one.perDay[d] : one.total[d], 'f', digits) + (daily ? "" : "  (+" + QString::number(one.perDay[d], 'f', digits) + " that day)");
    }
    text += daily ? "\naverage day: " + QString::number(meanPerDay[d], 'f', digits)
                  : "\naverage: " + QString::number(climatology.mean[d], 'f', digits) + "  (" + QString::number(climatology.lowest[d], 'f', digits) + " to " + QString::number(climatology.highest[d], 'f', digits) + ")";
    QToolTip::showText(event->globalPosition().toPoint(), text, this);
}

AceViewer::AceViewer(Window * parent, const std::shared_ptr<HurricaneData::SeasonData>& atlantic, const std::shared_ptr<HurricaneData::SeasonData>& pacific)
    : Window{parent}
    , comboBasin{this, {"Atlantic", "East and Central Pacific"}}
    , comboMetric{this, {"ACE (accumulated cyclone energy)", "TIKE (track integrated kinetic energy)"}}
    , comboMode{this, {"Running total through the year", "Amount of each day"}}
    , buttonClear{this, None, "Clear the years"}
    , buttonTop{this, None, "The 5 highest"}
    , textSummary{this, ""}
{
    setAttribute(Qt::WA_DeleteOnClose);
    setTitle("ACE and TIKE by day - the seasons chosen against the average");
    datas[0] = atlantic;
    datas[1] = pacific;
    textSummary.setWordWrap(true);
    textSummary.getView()->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    chart = new AceChart{this};
    chart->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    list = new QListWidget{this};
    list->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    list->setFixedWidth(180);
    list->setToolTip("Tick the seasons to draw (this season is ticked to begin with)");
    QObject::connect(list, &QListWidget::itemChanged, [this] { apply(); });
    comboBasin.connect([this] { fillYears(); apply(); });
    comboMetric.connect([this] { fillYears(); apply(); });
    comboMode.connect([this] { apply(); });
    buttonClear.connect([this] {
        filling = true;
        for (int i = 0; i < list->count(); i++) {
            list->item(i)->setCheckState(Qt::Unchecked);
        }
        filling = false;
        apply();
    });
    buttonTop.connect([this] {
        // the five seasons with the most, of those in the list
        const auto& data = datas[static_cast<size_t>(std::clamp(comboBasin.getIndex(), 0, 1))];
        if (!data) {
            return;
        }
        const bool tike = comboMetric.getIndex() == 1;
        auto seasons = UtilitySeason::seasons(data->history);
        std::sort(seasons.begin(), seasons.end(), [tike] (const auto& a, const auto& b) { return (tike ? a.tike : a.ace) > (tike ? b.tike : b.ace); });
        std::vector<int> top;
        for (const auto& s : seasons) {
            if (top.size() < 5 && (!tike || s.radiiStorms > 0)) {
                top.push_back(s.year);
            }
        }
        filling = true;
        for (int i = 0; i < list->count(); i++) {
            const int year = list->item(i)->data(Qt::UserRole).toInt();
            list->item(i)->setCheckState(std::find(top.begin(), top.end(), year) != top.end() ? Qt::Checked : Qt::Unchecked);
        }
        filling = false;
        apply();
    });
    row.addWidget(comboBasin);
    row.addWidget(comboMetric);
    row.addWidget(comboMode);
    row.addWidget(buttonClear);
    row.addWidget(buttonTop);
    row.addStretch();
    rowMain.addWidgetReal(list, 0, Qt::AlignTop | Qt::AlignLeft);
    rowMain.addWidgetReal(chart, 1, Qt::Alignment{});
    box.addLayout(row);
    box.addWidget(textSummary);
    box.addLayout(rowMain, 1);
    box.getAndShow(this);
    resize(1080, 640);
    fillYears();
    apply();
}

// the seasons to tick: this season first, then back through the years (for the TIKE, only those with wind radii)
void AceViewer::fillYears() {
    const auto& data = datas[static_cast<size_t>(std::clamp(comboBasin.getIndex(), 0, 1))];
    filling = true;
    list->clear();
    if (data && data->error.empty()) {
        const bool tike = comboMetric.getIndex() == 1;
        auto * now = new QListWidgetItem{QString::number(data->currentYear) + "  (this season)", list};
        now->setFlags(now->flags() | Qt::ItemIsUserCheckable);
        now->setCheckState(Qt::Checked);
        now->setData(Qt::UserRole, data->currentYear);
        auto seasons = UtilitySeason::seasons(data->history);
        std::sort(seasons.begin(), seasons.end(), [] (const auto& a, const auto& b) { return a.year > b.year; });
        for (const auto& s : seasons) {
            if (tike && s.radiiStorms == 0) {
                continue;
            }
            auto * item = new QListWidgetItem{QString::number(s.year) + "  " + QString::number(tike ? s.tike : s.ace, 'f', tike ? 0 : 0), list};
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
            item->setCheckState(Qt::Unchecked);
            item->setData(Qt::UserRole, s.year);
        }
    }
    filling = false;
}

QString AceViewer::standingText() const {
    const auto& data = datas[static_cast<size_t>(std::clamp(comboBasin.getIndex(), 0, 1))];
    return data ? AceChart::standing(*data, comboMetric.getIndex() == 1 ? AceChart::Metric::Tike : AceChart::Metric::Ace) : QString{};
}

void AceViewer::apply() {
    if (filling) {
        return;
    }
    const auto& data = datas[static_cast<size_t>(std::clamp(comboBasin.getIndex(), 0, 1))];
    if (!data) {
        return;
    }
    std::vector<int> years;
    for (int i = 0; i < list->count(); i++) {
        if (list->item(i)->checkState() == Qt::Checked) {
            years.push_back(list->item(i)->data(Qt::UserRole).toInt());
        }
    }
    const auto metric = comboMetric.getIndex() == 1 ? AceChart::Metric::Tike : AceChart::Metric::Ace;
    chart->setData(data, metric, comboMode.getIndex() == 1, years);
    textSummary.setText(standingText().toStdString());
}
