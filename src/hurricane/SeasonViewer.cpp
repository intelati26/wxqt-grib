// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "ui/ChartPainter.h"
#include "hurricane/SeasonViewer.h"
#include <algorithm>
#include <cmath>
#include <QMouseEvent>
#include <QTableWidget>
#include <QHeaderView>
#include <QPainter>
#include <QTextBrowser>
#include <QToolTip>
#include "hurricane/ChartKit.h"
#include "ui/NumberItem.h"

namespace {
    const char * metricNames[] = {"ACE (accumulated cyclone energy)", "Named storms", "Hurricanes", "Major hurricanes (Cat 3 and up)", "TIKE (track integrated kinetic energy, TJ)"};

    QString q(const std::string& s) {
        return QString::fromStdString(s).toHtmlEscaped();
    }

    QString dates(const UtilitySeason::Storm& s) {
        return QString::fromStdString(UtilityAtcf::formatTime(s.first)).left(10) + " - " + QString::fromStdString(UtilityAtcf::formatTime(s.last)).left(10);
    }
}

void SeasonChart::setData(const std::vector<UtilitySeason::Season>& newSeasons, int year) {
    seasons = newSeasons;
    currentYear = year;
    update();
}

void SeasonChart::setView(int newMetric, int newFirst, int newGroup) {
    metric = newMetric;
    firstYear = newFirst;
    group = newGroup;
    update();
}

double SeasonChart::value(const UtilitySeason::Season& s) const {
    switch (metric) {
        case 1: return s.named;
        case 2: return s.hurricanes;
        case 3: return s.major;
        case 4: return s.tike;
        default: return s.ace;
    }
}

// the seasons shown (those since the first year, those with radii for the TIKE), or the decades made of them; `divisors` is what each is divided by
std::vector<UtilitySeason::Season> SeasonChart::grouped(std::vector<double>& divisors) const {
    std::vector<UtilitySeason::Season> shown;
    divisors.clear();
    for (const auto& s : seasons) {
        if (s.year >= firstYear && (metric != 4 || s.radiiStorms > 0)) {   // a TIKE needs wind radii (HURDAT2 has them from 2004)
            if (group == 0) {
                shown.push_back(s);
                divisors.push_back(1.0);
            } else {
                const int decade = s.year / 10 * 10;
                if (shown.empty() || shown.back().year != decade) {
                    UtilitySeason::Season d;
                    d.year = decade;
                    shown.push_back(d);
                    divisors.push_back(0.0);
                }
                auto& d = shown.back();
                d.cyclones += s.cyclones;
                d.named += s.named;
                d.hurricanes += s.hurricanes;
                d.major += s.major;
                d.ace += s.ace;
                d.tike += s.tike;
                d.radiiStorms += s.radiiStorms;
                divisors.back() += 1.0;
            }
        }
    }
    if (group == 2) {
        std::fill(divisors.begin(), divisors.end(), 1.0);   // the total of the decade
    }
    return shown;
}

QRectF SeasonChart::plot() const {
    return QRectF{54.0, 24.0, width() - 54.0 - 14.0, height() - 24.0 - 34.0};
}

void SeasonChart::paintEvent(QPaintEvent *) {
    ChartPainter p{this};
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), QColor{245, 245, 245});
    std::vector<double> divisors;
    const auto shown = grouped(divisors);
    if (shown.empty()) {
        p.setPen(QColor{100, 100, 100});
        p.drawText(rect(), Qt::AlignCenter, "No season data");
        return;
    }
    double top = 1.0;
    for (size_t i = 0; i < shown.size(); i++) {
        top = std::max(top, value(shown[i]) / divisors[i]);
    }
    const double step = metric == 4 ? (top > 40000 ? 10000.0 : top > 20000 ? 5000.0 : top > 8000 ? 2000.0 : top > 3000 ? 1000.0 : top > 1000 ? 500.0 : 100.0)
                                    : (top > 1000 ? 200.0 : top > 400 ? 100.0 : top > 200 ? 50.0 : top > 100 ? 25.0 : top > 40 ? 10.0 : top > 16 ? 5.0 : 2.0);
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
            if (s.year >= 1991 && s.year <= 2020 && (metric != 4 || s.radiiStorms > 0)) {
                sum += value(s);
                n++;
            }
        }
        average = n > 0 ? sum / n : 0.0;
        if (group == 2) {
            average *= 10.0;   // a decade's total against ten average seasons
        }
    }
    for (size_t i = 0; i < shown.size(); i++) {
        const auto& s = shown[i];
        const double v = value(s) / divisors[i];
        const double h = area.height() * v / top;
        const QRectF bar{area.left() + static_cast<double>(i) * barWidth + barWidth * 0.1, area.bottom() - h, barWidth * 0.8, h};
        const bool now = group == 0 ? s.year == currentYear : currentYear / 10 * 10 == s.year;
        // above the average: warmer colour
        p.setPen(Qt::NoPen);
        p.setBrush(now ? QColor{220, 40, 40} : v > average ? QColor{235, 140, 60} : QColor{90, 140, 210});
        p.drawRect(bar);
    }
    if (average > 0) {
        const double py = area.bottom() - area.height() * average / top;
        p.setPen(QPen{QColor{30, 30, 30}, 1.4, Qt::DashLine});
        p.drawLine(QPointF{area.left(), py}, QPointF{area.right(), py});
        p.drawText(QPointF{area.left() + 6, py - 4}, QString{metric == 4 ? "2004-2020 average " : "1991-2020 average "} + QString{group == 2 ? "decade (10 seasons) " : "season "} + QString::number(average, 'f', metric == 0 || metric == 4 ? 0 : 1));
    }
    // year labels: every 10 or 5 or 1 depending on the room
    const int every = group != 0 ? 10 : barWidth > 24 ? 1 : barWidth > 9 ? 5 : 10;
    p.setPen(QColor{70, 70, 70});
    for (size_t i = 0; i < shown.size(); i++) {
        if (shown[i].year % every == 0) {
            const double x = area.left() + (static_cast<double>(i) + 0.5) * barWidth;
            p.drawText(QRectF{x - 20, area.bottom() + 3, 40, 14}, Qt::AlignHCenter, QString::number(shown[i].year) + (group != 0 ? "s" : ""));
        }
    }
    QFont bold{small};
    bold.setBold(true);
    bold.setPixelSize(12);
    p.setFont(bold);
    p.setPen(QColor{30, 30, 30});
    p.drawText(QPointF{area.left(), area.top() - 8}, QString{metricNames[std::clamp(metric, 0, 4)]} + (group == 0 ? " by season" : group == 1 ? " by decade (the average season of each)" : " by decade (the total)") + (currentYear > 0 ? "  (red: " + QString::number(currentYear) + " so far)" : QString{}));
    p.setFont(small);
    p.setPen(QColor{90, 90, 90});
    p.drawText(QPointF{area.left(), height() - 6.0}, "HURDAT2 (NHC) through the last finished season; the current season from the ATCF best tracks. ACE: winds of 34 kt or more at 00, 06, 12, 18 UTC, squared, / 10,000. TIKE: the kinetic energy in the 34, 50 and 64 kt wind radii of those records, estimated, added up.");
}

void SeasonChart::mouseMoveEvent(QMouseEvent * event) {
    std::vector<double> divisors;
    const auto shown = grouped(divisors);
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
    if (group != 0) {
        QToolTip::showText(event->globalPosition().toPoint(), QString::number(s.year) + "s: " + QString::number(value(s) / divisors[index], 'f', metric == 0 || metric == 4 ? 0 : 1) + (group == 1 ? " a season on average" : " in the decade") +
            "\n(" + QString::number(s.named) + " named storms, " + QString::number(s.hurricanes) + " hurricanes, " + QString::number(s.major) + " major, ACE " + QString::number(s.ace, 'f', 0) + ")", this);
        return;
    }
    QToolTip::showText(event->globalPosition().toPoint(), QString::number(s.year) + ": " + QString::number(s.named) + " named storms, " + QString::number(s.hurricanes) + " hurricanes, " +
        QString::number(s.major) + " major, ACE " + QString::number(s.ace, 'f', 1) + (s.radiiStorms > 0 ? ", TIKE " + QString::number(std::lround(s.tike)) + " TJ" : QString{}), this);
}

SeasonViewer::SeasonViewer(Window * parent, const std::shared_ptr<HurricaneData::SeasonData>& seasonData)
    : Window{parent}
    , comboMetric{this, {metricNames[0], metricNames[1], metricNames[2], metricNames[3], metricNames[4]}}
    , comboYears{this, {"Since 1950", "Since 1851 (all)", "Since 1991", "Last 30 seasons"}}
    , comboGroup{this, {"Each season", "By decade (average season)", "By decade (total)"}}
    , textSummary{this, ""}
    , data{seasonData}
{
    setAttribute(Qt::WA_DeleteOnClose);
    setTitle(string{data->basin == "al" ? "Atlantic" : "Northeast Pacific (Eastern and Central)"} + " seasons and ACE");
    textSummary.setWordWrap(true);
    chart = new SeasonChart{this};
    auto all = data->history;
    all.insert(all.end(), data->current.begin(), data->current.end());
    seasons = UtilitySeason::seasons(all);
    chart->setData(seasons, data->current.empty() ? 0 : data->currentYear);
    comboMetric.connect([this] { apply(); });
    comboYears.connect([this] { apply(); });
    comboGroup.connect([this] { apply(); });
    row.addWidget(comboMetric);
    row.addWidget(comboYears);
    row.addWidget(comboGroup);
    row.addStretch();
    box.addLayout(row);
    box.addWidget(textSummary);
    box.addWidgetReal(chart, 1, Qt::Alignment{});
    // every season in a table: a click on a heading ranks them by it (the ACE, the TIKE, the named storms, the hurricanes, the major hurricanes)
    ranked = new QTableWidget{0, 7, this};
    ranked->setHorizontalHeaderLabels({"Season", "Systems", "Named storms", "Hurricanes", "Major hurricanes", "ACE", "TIKE (TJ)"});
    ranked->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ranked->setSelectionBehavior(QAbstractItemView::SelectRows);
    ranked->verticalHeader()->hide();
    ranked->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ranked->setToolTip("Click a heading to rank the seasons by it; click again to turn the order round");
    ranked->setMinimumHeight(170);
    ranked->setMaximumHeight(240);
    box.addWidgetReal(ranked, 0, Qt::Alignment{});
    // this season's storms
    auto * table = new QTextBrowser{this};
    table->setMinimumHeight(190);
    QString html;
    if (data->current.empty()) {
        html = "<p>No storms of the " + QString::number(data->currentYear) + " season in the NHC files yet.</p>";
    } else {
        html = "<table border='1' cellspacing='0' cellpadding='3' style='font-size:12px'><tr style='background:#cfe2f3'><th style='color:#10243a'>Storm</th><th style='color:#10243a'>Name</th><th style='color:#10243a'>Dates</th><th style='color:#10243a'>Peak wind</th><th style='color:#10243a'>Lowest pressure</th><th style='color:#10243a'>ACE</th><th style='color:#10243a'>TIKE (TJ)</th><th style='color:#10243a'></th></tr>";
        double total = 0.0;
        double totalTike = 0.0;
        bool anyTike = false;
        for (const auto& s : data->current) {
            total += s.ace;
            totalTike += s.tike;
            anyTike = anyTike || s.hasRadii;
            const bool active = std::find(data->active.begin(), data->active.end(), s.id) != data->active.end();
            html += "<tr><td>" + q(s.id.substr(0, 4)) + "</td><td>" + q(s.name) + "</td><td>" + dates(s) + "</td><td>" + q(UtilityAtcf::windLabel(s.peakWind)) + "</td><td>" +
                (s.minPressure > 0 ? QString::number(s.minPressure) + " mb" : QString{"-"}) + "</td><td>" + QString::number(s.ace, 'f', 1) + "</td><td>" + (s.hasRadii ? QString::number(std::lround(s.tike)) : QString{"-"}) + "</td><td>" + (active ? "active" : "") + "</td></tr>";
        }
        html += "<tr style='background:#eee; color:#10243a'><td colspan='5' style='color:#10243a'><b>Season total</b></td><td style='color:#10243a'><b>" + QString::number(total, 'f', 1) + "</b></td><td style='color:#10243a'><b>" + (anyTike ? QString::number(std::lround(totalTike)) : QString{"-"}) + "</b></td><td></td></tr></table>";
    }
    table->setHtml(html);
    box.addWidgetReal(table, 0, Qt::Alignment{});
    box.getAndShow(this);
    resize(1000, 900);
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
    chart->setView(comboMetric.getIndex(), first, comboGroup.getIndex());
    {   // the table of seasons, for the years chosen; the column it was sorted by stays (the ACE, biggest first, to begin with)
        const bool begun = ranked->rowCount() > 0;
        const int column = begun ? ranked->horizontalHeader()->sortIndicatorSection() : 5;
        const auto order = begun ? ranked->horizontalHeader()->sortIndicatorOrder() : Qt::DescendingOrder;
        ranked->setSortingEnabled(false);
        std::vector<const UtilitySeason::Season *> chosen;
        for (const auto& season : seasons) {
            if (season.year >= first) {
                chosen.push_back(&season);
            }
        }
        ranked->setRowCount(static_cast<int>(chosen.size()));
        for (int row = 0; row < static_cast<int>(chosen.size()); row++) {
            const auto& season = *chosen[static_cast<size_t>(row)];
            const auto cell = [&] (int c, const QString& text, double value, bool has = true) { ranked->setItem(row, c, new NumberItem{text, value, has}); };
            cell(0, QString::number(season.year) + (season.year == data->currentYear && !data->current.empty() ? " (so far)" : ""), season.year);
            cell(1, QString::number(season.cyclones), season.cyclones);
            cell(2, QString::number(season.named), season.named);
            cell(3, QString::number(season.hurricanes), season.hurricanes);
            cell(4, QString::number(season.major), season.major);
            cell(5, QString::number(season.ace, 'f', 1), season.ace);
            cell(6, season.radiiStorms > 0 ? QString::number(std::lround(season.tike)) : QString{"-"}, season.tike, season.radiiStorms > 0);   // no wind radii before 2004
        }
        ranked->setSortingEnabled(true);
        ranked->sortByColumn(column, order);
    }
    // the season so far, in words
    QString text;
    double tikeAverage = 0.0;
    {
        double sum = 0.0;
        int n = 0;
        for (const auto& x : seasons) {
            if (x.year >= 2004 && x.year <= 2020 && x.radiiStorms > 0) {
                sum += x.tike;
                n++;
            }
        }
        tikeAverage = n > 0 ? sum / n : 0.0;
    }
    if (!data->current.empty()) {
        for (const auto& s : seasons) {
            if (s.year == data->currentYear) {
                text = QString::number(s.year) + " so far: " + QString::number(s.named) + " named storms, " + QString::number(s.hurricanes) + " hurricanes, " + QString::number(s.major) +
                    " major, ACE " + QString::number(s.ace, 'f', 1) + ".  1991-2020 whole-season averages: " +
                    QString::number(UtilitySeason::mean(seasons, 1991, 2020, &UtilitySeason::Season::named), 'f', 1) + " named, " +
                    QString::number(UtilitySeason::mean(seasons, 1991, 2020, &UtilitySeason::Season::hurricanes), 'f', 1) + " hurricanes, " +
                    QString::number(UtilitySeason::mean(seasons, 1991, 2020, &UtilitySeason::Season::major), 'f', 1) + " major, ACE " +
                    QString::number(UtilitySeason::mean(seasons, 1991, 2020, &UtilitySeason::Season::ace), 'f', 0) + "." +
                    (s.radiiStorms > 0 ? "  TIKE " + QString::number(std::lround(s.tike)) + " TJ so far (an estimate from the wind radii; the 2004-2020 whole-season average is " + QString::number(std::lround(tikeAverage)) + " TJ)." : QString{});
            }
        }
    }
    if (data->basin != "al" && !text.isEmpty()) {
        text += "  Pacific records before the satellite era (about 1971) are incomplete.";
    }
    textSummary.setText(text);
}
