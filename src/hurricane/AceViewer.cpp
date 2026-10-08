// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "hurricane/AceViewer.h"
#include <algorithm>
#include <cmath>
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
    // the newest of the best-track records of this season, or the date today when the season has a storm and it is still the same year
    return daysOfNewest(data.current);
}

void AceChart::setData(const std::shared_ptr<HurricaneData::SeasonData>& newData, int newCompare) {
    data = newData;
    compareYear = newCompare;
    season.clear();
    compare.clear();
    perDay.assign(367, 0.0);
    climatology = UtilitySeason::Climatology{};
    if (data && data->error.empty()) {
        season = UtilitySeason::cumulativeByDay(data->current, data->currentYear);
        for (size_t d = 1; d < season.size(); d++) {
            perDay[d] = season[d] - season[d - 1];
        }
        climatology = UtilitySeason::climatology(data->history, 1991, 2020);
        if (compareYear > 0) {
            compare = UtilitySeason::cumulativeByDay(data->history, compareYear);
        }
        // the season is drawn up to the day of its newest record, and "today" is the same unless the season has not started
        today = lastDay(*data);
    }
    update();
}

QString AceChart::standing(const HurricaneData::SeasonData& data) {
    if (!data.error.empty() || data.current.empty()) {
        return {};
    }
    const int day = lastDay(data);
    if (day <= 0) {
        return {};
    }
    const auto now = UtilitySeason::cumulativeByDay(data.current, data.currentYear);
    const auto clim = UtilitySeason::climatology(data.history, 1991, 2020);
    const double ace = now[static_cast<size_t>(day)];
    const double normal = clim.mean[static_cast<size_t>(day)];
    // how many of the years since 1950 (the satellite era, the fair record) had more by this day
    int more = 0;
    int years = 0;
    for (int year = 1950; year < data.currentYear; year++) {
        const auto c = UtilitySeason::cumulativeByDay(data.history, year);
        years++;
        more += c[static_cast<size_t>(day)] > ace ? 1 : 0;
    }
    QString text = "ACE " + QString::number(ace, 'f', 1) + " through " + dateOf(day) + ": ";
    if (normal > 0.5) {
        text += QString::number(std::lround(100.0 * ace / normal)) + " % of the 1991-2020 average for the date (" + QString::number(normal, 'f', 1) + ")";
    } else {
        text += "the 1991-2020 average for the date is " + QString::number(normal, 'f', 1);
    }
    if (years > 0) {
        text += ", " + QString::number(more + 1) + QString{more + 1 == 1 ? "st" : more + 1 == 2 ? "nd" : more + 1 == 3 ? "rd" : "th"} + " highest of the " + QString::number(years + 1) + " seasons since 1950 at this date";
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
    if (!data || !data->error.empty() || season.empty()) {
        p.setPen(QColor{100, 100, 100});
        p.drawText(rect(), Qt::AlignCenter, data && !data->error.empty() ? QString::fromStdString(data->error) : "Loading...");
        return;
    }
    const auto area = plot();
    const auto bars = barsArea();
    double top = 10.0;
    for (size_t d = firstDay; d <= lastDayShown; d++) {
        top = std::max({top, climatology.highest[d], season[d], compare.empty() ? 0.0 : compare[d]});
    }
    const double step = top > 400 ? 100.0 : top > 200 ? 50.0 : top > 100 ? 25.0 : top > 40 ? 10.0 : top > 16 ? 5.0 : 2.0;
    top = std::ceil(top * 1.05 / step) * step;
    const auto xOf = [&] (double day) { return area.left() + area.width() * (day - firstDay) / (lastDayShown - firstDay); };
    const auto yOf = [&] (double ace) { return area.bottom() - area.height() * ace / top; };
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
    // month lines
    static const int months[] = {121, 152, 182, 213, 244, 274, 305, 335};
    static const char * monthNames[] = {"May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    for (int i = 0; i < 8; i++) {
        p.setPen(QColor{232, 232, 232});
        p.drawLine(QPointF{xOf(months[i]), area.top()}, QPointF{xOf(months[i]), bars.bottom()});
        p.setPen(QColor{70, 70, 70});
        p.drawText(QRectF{xOf(months[i]) + 2, area.bottom() + 3, 40, 14}, Qt::AlignLeft, monthNames[i]);
    }
    // the band of the lowest and highest of 1991-2020 and its mean
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
    const auto line = [&] (const vector<double>& values, int upTo, const QColor& color, double width, Qt::PenStyle style) {
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
    if (!compare.empty()) {
        line(compare, lastDayShown, QColor{40, 110, 210}, 2.0, Qt::SolidLine);
    }
    line(season, today > 0 ? today : lastDayShown, QColor{215, 40, 40}, 2.8, Qt::SolidLine);
    if (today >= firstDay && today <= lastDayShown) {
        p.setPen(QPen{QColor{215, 40, 40}, 1.0, Qt::DotLine});
        p.drawLine(QPointF{xOf(today), area.top()}, QPointF{xOf(today), area.bottom()});
        p.setBrush(QColor{215, 40, 40});
        p.setPen(QColor{255, 255, 255});
        p.drawEllipse(QPointF{xOf(today), yOf(season[static_cast<size_t>(today)])}, 4.0, 4.0);
    }
    // the legend and title
    QFont bold{small};
    bold.setBold(true);
    bold.setPixelSize(12);
    p.setFont(bold);
    p.setPen(QColor{30, 30, 30});
    p.drawText(QPointF{area.left(), area.top() - 8}, QString{"ACE through the year, "} + (data->basin == "al" ? "Atlantic" : "East and Central Pacific"));
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
    key(QColor{215, 40, 40}, Qt::SolidLine, QString::number(data->currentYear));
    key(QColor{60, 60, 70}, Qt::DashLine, "1991-2020 average");
    if (!compare.empty()) {
        key(QColor{40, 110, 210}, Qt::SolidLine, QString::number(compareYear));
    }
    p.setPen(Qt::NoPen);
    p.setBrush(QColor{200, 200, 205, 200});
    p.drawRect(QRectF{lx, ly - 5, 22, 10});
    p.setPen(QColor{50, 50, 50});
    p.drawText(QPointF{lx + 27, ly + 4}, "lowest to highest of those years");
    // the ACE of each day of this season
    double barTop = 1.0;
    for (size_t d = firstDay; d <= lastDayShown; d++) {
        barTop = std::max(barTop, perDay[d]);
    }
    p.setPen(QColor{210, 210, 210});
    p.setBrush(QColor{252, 252, 252});
    p.drawRect(bars);
    for (int d = firstDay; d <= lastDayShown; d++) {
        const double v = perDay[static_cast<size_t>(d)];
        if (v <= 0.0) {
            continue;
        }
        const double h = bars.height() * v / barTop;
        p.setPen(Qt::NoPen);
        p.setBrush(QColor{225, 90, 70});
        p.drawRect(QRectF{xOf(d) - 1.0, bars.bottom() - h, std::max(2.0, area.width() / (lastDayShown - firstDay)), h});
    }
    p.setPen(QColor{70, 70, 70});
    p.setFont(small);
    p.drawText(QRectF{bars.left() + 4, bars.top() + 1, 200, 12}, Qt::AlignLeft, "ACE added each day (peak " + QString::number(barTop, 'f', 1) + ")");
    p.drawText(QPointF{area.left(), height() - 6.0}, "HURDAT2 (NHC best-track database) and this season's ATCF best tracks; the ACE of each 6-hourly record at 34 kt or more, as reported.");
}

void AceChart::mouseMoveEvent(QMouseEvent * event) {
    if (!data || season.empty()) {
        return;
    }
    const auto area = plot();
    const double fraction = (event->position().x() - area.left()) / area.width();
    const int day = firstDay + static_cast<int>(std::lround(fraction * (lastDayShown - firstDay)));
    if (fraction < 0.0 || fraction > 1.0 || day < firstDay || day > lastDayShown) {
        QToolTip::hideText();
        return;
    }
    const auto d = static_cast<size_t>(day);
    QString text = dateOf(day);
    if (today == 0 || day <= today) {
        text += "\n" + QString::number(data->currentYear) + ": " + QString::number(season[d], 'f', 1) + "  (+" + QString::number(perDay[d], 'f', 1) + " that day)";
    }
    text += "\n1991-2020 average: " + QString::number(climatology.mean[d], 'f', 1) + "  (" + QString::number(climatology.lowest[d], 'f', 1) + " to " + QString::number(climatology.highest[d], 'f', 1) + ")";
    if (!compare.empty()) {
        text += "\n" + QString::number(compareYear) + ": " + QString::number(compare[d], 'f', 1);
    }
    QToolTip::showText(event->globalPosition().toPoint(), text, this);
}

AceViewer::AceViewer(Window * parent, const std::shared_ptr<HurricaneData::SeasonData>& atlantic, const std::shared_ptr<HurricaneData::SeasonData>& pacific)
    : Window{parent}
    , comboBasin{this, {"Atlantic", "East and Central Pacific"}}
    , comboCompare{this, {"No comparison year"}}
    , textSummary{this, ""}
{
    setAttribute(Qt::WA_DeleteOnClose);
    setTitle("ACE by day - accumulated cyclone energy through the season");
    datas[0] = atlantic;
    datas[1] = pacific;
    textSummary.setWordWrap(true);
    chart = new AceChart{this};
    row.addWidget(comboBasin);
    row.addWidget(comboCompare);
    row.addStretch();
    box.addLayout(row);
    box.addWidget(textSummary);
    box.addWidgetReal(chart, 1, Qt::Alignment{});
    box.getAndShow(this);
    resize(940, 620);
    comboBasin.connect([this] { fillYears(); apply(); });
    comboCompare.connect([this] { apply(); });
    fillYears();
    apply();
}

void AceViewer::fillYears() {
    const auto& data = datas[static_cast<size_t>(std::clamp(comboBasin.getIndex(), 0, 1))];
    std::vector<std::string> list{"No comparison year"};
    if (data && data->error.empty()) {
        std::vector<int> years;
        for (const auto& season : UtilitySeason::seasons(data->history)) {
            years.push_back(season.year);
        }
        std::sort(years.rbegin(), years.rend());
        for (const int year : years) {
            list.push_back(std::to_string(year));
        }
    }
    filling = true;
    comboCompare.setList(list);
    comboCompare.setIndex(0);
    filling = false;
}

void AceViewer::apply() {
    if (filling) {
        return;
    }
    const auto& data = datas[static_cast<size_t>(std::clamp(comboBasin.getIndex(), 0, 1))];
    if (!data) {
        return;
    }
    const int year = comboCompare.getIndex() > 0 ? std::atoi(comboCompare.getValue().c_str()) : 0;
    chart->setData(data, year);
    textSummary.setText(AceChart::standing(*data).toStdString());
}
