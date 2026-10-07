// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "hurricane/SeasonViewer.h"
#include <algorithm>
#include <cmath>
#include <QMouseEvent>
#include <QPainter>
#include <QTextBrowser>
#include <QToolTip>
#include "hurricane/ChartKit.h"

namespace {
    const char * metricNames[] = {"ACE (accumulated cyclone energy)", "Named storms", "Hurricanes", "Major hurricanes (Cat 3 and up)"};

    QString q(const std::string& s) {
        return QString::fromStdString(s).toHtmlEscaped();
    }

    QString dates(const UtilitySeason::Storm& s) {
        return QString::fromStdString(UtilityAtcf::formatTime(s.first)).left(6) + " - " + QString::fromStdString(UtilityAtcf::formatTime(s.last)).left(6);
    }
}

void SeasonChart::setData(const std::vector<UtilitySeason::Season>& newSeasons, int year) {
    seasons = newSeasons;
    currentYear = year;
    update();
}

void SeasonChart::setView(int newMetric, int newFirst) {
    metric = newMetric;
    firstYear = newFirst;
    update();
}

double SeasonChart::value(const UtilitySeason::Season& s) const {
    switch (metric) {
        case 1: return s.named;
        case 2: return s.hurricanes;
        case 3: return s.major;
        default: return s.ace;
    }
}

QRectF SeasonChart::plot() const {
    return QRectF{54.0, 24.0, width() - 54.0 - 14.0, height() - 24.0 - 34.0};
}

void SeasonChart::paintEvent(QPaintEvent *) {
    QPainter p{this};
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), QColor{245, 245, 245});
    std::vector<UtilitySeason::Season> shown;
    for (const auto& s : seasons) {
        if (s.year >= firstYear) {
            shown.push_back(s);
        }
    }
    if (shown.empty()) {
        p.setPen(QColor{100, 100, 100});
        p.drawText(rect(), Qt::AlignCenter, "No season data");
        return;
    }
    double top = 1.0;
    for (const auto& s : shown) {
        top = std::max(top, value(s));
    }
    const double step = top > 200 ? 50.0 : top > 100 ? 25.0 : top > 40 ? 10.0 : top > 16 ? 5.0 : 2.0;
    top = std::ceil(top / step) * step;
    const auto area = plot();
    const double barWidth = area.width() / static_cast<double>(shown.size());
    QFont small{p.font()};
    small.setPixelSize(10);
    p.setFont(small);
    p.setPen(QColor{210, 210, 210});
    p.setBrush(QColor{252, 252, 252});
    p.drawRect(area);
    for (double y = 0; y <= top + 1e-9; y += step) {
        const double py = area.bottom() - area.height() * y / top;
        p.setPen(QColor{232, 232, 232});
        p.drawLine(QPointF{area.left(), py}, QPointF{area.right(), py});
        p.setPen(QColor{70, 70, 70});
        p.drawText(QRectF{area.left() - 46, py - 7, 42, 14}, Qt::AlignRight | Qt::AlignVCenter, QString::number(y, 'f', 0));
    }
    // the 1991-2020 average of the whole season, a line across
    double average = 0.0;
    {
        double sum = 0.0;
        int n = 0;
        for (const auto& s : seasons) {
            if (s.year >= 1991 && s.year <= 2020) {
                sum += value(s);
                n++;
            }
        }
        average = n > 0 ? sum / n : 0.0;
    }
    for (size_t i = 0; i < shown.size(); i++) {
        const auto& s = shown[i];
        const double v = value(s);
        const double h = area.height() * v / top;
        const QRectF bar{area.left() + static_cast<double>(i) * barWidth + barWidth * 0.1, area.bottom() - h, barWidth * 0.8, h};
        const bool now = s.year == currentYear;
        // above the average: warmer colour
        p.setPen(Qt::NoPen);
        p.setBrush(now ? QColor{220, 40, 40} : v > average ? QColor{235, 140, 60} : QColor{90, 140, 210});
        p.drawRect(bar);
    }
    if (average > 0) {
        const double py = area.bottom() - area.height() * average / top;
        p.setPen(QPen{QColor{30, 30, 30}, 1.4, Qt::DashLine});
        p.drawLine(QPointF{area.left(), py}, QPointF{area.right(), py});
        p.drawText(QPointF{area.left() + 6, py - 4}, "1991-2020 average " + QString::number(average, 'f', metric == 0 ? 0 : 1));
    }
    // year labels: every 10 or 5 or 1 depending on the room
    const int every = barWidth > 24 ? 1 : barWidth > 9 ? 5 : 10;
    p.setPen(QColor{70, 70, 70});
    for (size_t i = 0; i < shown.size(); i++) {
        if (shown[i].year % every == 0) {
            const double x = area.left() + (static_cast<double>(i) + 0.5) * barWidth;
            p.drawText(QRectF{x - 20, area.bottom() + 3, 40, 14}, Qt::AlignHCenter, QString::number(shown[i].year));
        }
    }
    QFont bold{small};
    bold.setBold(true);
    bold.setPixelSize(12);
    p.setFont(bold);
    p.setPen(QColor{30, 30, 30});
    p.drawText(QPointF{area.left(), area.top() - 8}, QString{metricNames[std::clamp(metric, 0, 3)]} + " by season" + (currentYear > 0 ? "  (red: " + QString::number(currentYear) + " so far)" : QString{}));
    p.setFont(small);
    p.setPen(QColor{90, 90, 90});
    p.drawText(QPointF{area.left(), height() - 6.0}, "HURDAT2 (NHC) through the last finished season; the current season from the ATCF best tracks. ACE: winds of 34 kt or more at 00, 06, 12, 18 UTC, squared, / 10,000.");
}

void SeasonChart::mouseMoveEvent(QMouseEvent * event) {
    std::vector<UtilitySeason::Season> shown;
    for (const auto& s : seasons) {
        if (s.year >= firstYear) {
            shown.push_back(s);
        }
    }
    const auto area = plot();
    if (shown.empty() || !area.contains(event->position())) {
        QToolTip::hideText();
        return;
    }
    const auto index = static_cast<size_t>((event->position().x() - area.left()) / (area.width() / static_cast<double>(shown.size())));
    if (index >= shown.size()) {
        return;
    }
    const auto& s = shown[index];
    QToolTip::showText(event->globalPosition().toPoint(), QString::number(s.year) + ": " + QString::number(s.named) + " named storms, " + QString::number(s.hurricanes) + " hurricanes, " +
        QString::number(s.major) + " major, ACE " + QString::number(s.ace, 'f', 1), this);
}

SeasonViewer::SeasonViewer(Window * parent, const std::shared_ptr<HurricaneData::SeasonData>& seasonData)
    : Window{parent}
    , comboMetric{this, {metricNames[0], metricNames[1], metricNames[2], metricNames[3]}}
    , comboYears{this, {"Since 1950", "Since 1851 (all)", "Since 1991", "Last 30 seasons"}}
    , textSummary{this, ""}
    , data{seasonData}
{
    setAttribute(Qt::WA_DeleteOnClose);
    setTitle("Atlantic hurricane seasons and ACE");
    textSummary.setWordWrap(true);
    chart = new SeasonChart{this};
    auto all = data->history;
    all.insert(all.end(), data->current.begin(), data->current.end());
    seasons = UtilitySeason::seasons(all);
    chart->setData(seasons, data->current.empty() ? 0 : data->currentYear);
    comboMetric.connect([this] { apply(); });
    comboYears.connect([this] { apply(); });
    row.addWidget(comboMetric);
    row.addWidget(comboYears);
    row.addStretch();
    box.addLayout(row);
    box.addWidget(textSummary);
    box.addWidgetReal(chart, 1, Qt::Alignment{});
    // this season's storms
    auto * table = new QTextBrowser{this};
    table->setMinimumHeight(190);
    QString html;
    if (data->current.empty()) {
        html = "<p>No storms of the " + QString::number(data->currentYear) + " season in the NHC files yet.</p>";
    } else {
        html = "<table border='1' cellspacing='0' cellpadding='3' style='font-size:12px'><tr style='background:#cfe2f3'><th>Storm</th><th>Name</th><th>Dates</th><th>Peak wind</th><th>Lowest pressure</th><th>ACE</th><th></th></tr>";
        double total = 0.0;
        for (const auto& s : data->current) {
            total += s.ace;
            const bool active = std::find(data->active.begin(), data->active.end(), s.id) != data->active.end();
            html += "<tr><td>" + q(s.id.substr(0, 4)) + "</td><td>" + q(s.name) + "</td><td>" + dates(s) + "</td><td>" + q(UtilityAtcf::windLabel(s.peakWind)) + "</td><td>" +
                (s.minPressure > 0 ? QString::number(s.minPressure) + " mb" : QString{"-"}) + "</td><td>" + QString::number(s.ace, 'f', 1) + "</td><td>" + (active ? "active" : "") + "</td></tr>";
        }
        html += "<tr style='background:#eee'><td colspan='5'><b>Season total</b></td><td><b>" + QString::number(total, 'f', 1) + "</b></td><td></td></tr></table>";
    }
    table->setHtml(html);
    box.addWidgetReal(table, 0, Qt::Alignment{});
    box.getAndShow(this);
    resize(1000, 760);
    apply();
}

void SeasonViewer::apply() {
    int first = 1950;
    switch (comboYears.getIndex()) {
        case 1: first = 1851; break;
        case 2: first = 1991; break;
        case 3: first = data->currentYear - 30; break;
        default: break;
    }
    chart->setView(comboMetric.getIndex(), first);
    // the season so far, in words
    QString text;
    if (!data->current.empty()) {
        for (const auto& s : seasons) {
            if (s.year == data->currentYear) {
                text = QString::number(s.year) + " so far: " + QString::number(s.named) + " named storms, " + QString::number(s.hurricanes) + " hurricanes, " + QString::number(s.major) +
                    " major, ACE " + QString::number(s.ace, 'f', 1) + ".  1991-2020 whole-season averages: " +
                    QString::number(UtilitySeason::mean(seasons, 1991, 2020, &UtilitySeason::Season::named), 'f', 1) + " named, " +
                    QString::number(UtilitySeason::mean(seasons, 1991, 2020, &UtilitySeason::Season::hurricanes), 'f', 1) + " hurricanes, " +
                    QString::number(UtilitySeason::mean(seasons, 1991, 2020, &UtilitySeason::Season::major), 'f', 1) + " major, ACE " +
                    QString::number(UtilitySeason::mean(seasons, 1991, 2020, &UtilitySeason::Season::ace), 'f', 0) + ".";
            }
        }
    }
    textSummary.setText(text);
}
