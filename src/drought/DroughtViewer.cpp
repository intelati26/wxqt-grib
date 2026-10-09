// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "drought/DroughtViewer.h"
#include <algorithm>
#include <QPainter>
#include <QPainterPath>
#include <QVBoxLayout>
#include "objects/FutureBytes.h"
#include "objects/FutureText.h"
#include "ui/ActivityLabel.h"

namespace {
    const QColor categoryColors[5] = {QColor{"#e8d84c"}, QColor{"#fcd37f"}, QColor{"#ffaa00"}, QColor{"#e60000"}, QColor{"#730000"}};
    const char * categoryNames[5] = {"D0 abnormally dry", "D1 moderate", "D2 severe", "D3 extreme", "D4 exceptional"};
}

DroughtChart::DroughtChart(QWidget * parent) : QWidget{parent} {
    setMinimumHeight(210);
    setMaximumHeight(230);
}

void DroughtChart::setWeeks(const std::vector<Week>& w) {
    weeks = w;
    update();
}

void DroughtChart::paintEvent(QPaintEvent *) {
    QPainter p{this};
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), palette().window());
    const QRectF plot{46.0, 22.0, width() - 60.0, height() - 72.0};
    p.setPen(palette().color(QPalette::WindowText));
    auto font = p.font();
    font.setPointSizeF(font.pointSizeF() * 0.9);
    p.setFont(font);
    p.drawText(QPointF{6, 14}, "Share of the contiguous U.S. in drought (U.S. Drought Monitor, percent of area)");
    if (weeks.size() < 2) {
        p.drawText(plot, Qt::AlignCenter, "Loading the statistics...");
        return;
    }
    double top = 20.0;
    for (const auto& w : weeks) {
        top = std::max(top, w.d[0]);
    }
    top = std::min(100.0, std::ceil(top / 10.0) * 10.0);
    p.setPen(QColor{128, 128, 128, 90});
    for (double v = 0; v <= top + 0.1; v += top > 60 ? 20 : 10) {
        const double y = plot.bottom() - v / top * plot.height();
        p.drawLine(QPointF{plot.left(), y}, QPointF{plot.right(), y});
        p.setPen(palette().color(QPalette::WindowText));
        p.drawText(QRectF{0, y - 8, plot.left() - 4, 16}, Qt::AlignRight | Qt::AlignVCenter, QString::number(v, 'f', 0) + "%");
        p.setPen(QColor{128, 128, 128, 90});
    }
    const auto x = [&] (size_t i) { return plot.left() + plot.width() * static_cast<double>(i) / static_cast<double>(weeks.size() - 1); };
    for (int c = 4; c >= 0; c--) {   // the milder categories behind
        QPainterPath path;
        for (size_t i = 0; i < weeks.size(); i++) {
            const QPointF at{x(i), plot.bottom() - weeks[i].d[c] / top * plot.height()};
            i == 0 ? path.moveTo(at) : path.lineTo(at);
        }
        p.setPen(QPen{categoryColors[c].darker(c == 0 ? 130 : 100), 2.0});
        p.setBrush(Qt::NoBrush);
        p.drawPath(path);
    }
    p.setPen(palette().color(QPalette::WindowText));
    for (size_t i = 0; i < weeks.size(); i += std::max<size_t>(1, weeks.size() / 6)) {
        p.drawText(QRectF{x(i) - 30, plot.bottom() + 3, 60, 14}, Qt::AlignHCenter, weeks[i].date.toString("MMM d"));
    }
    double legendX = plot.left();
    for (int c = 0; c < 5; c++) {   // the key, with this week's number
        p.setPen(QPen{categoryColors[c].darker(c == 0 ? 130 : 100), 3.0});
        p.drawLine(QPointF{legendX, height() - 10.0}, QPointF{legendX + 14, height() - 10.0});
        p.setPen(palette().color(QPalette::WindowText));
        const QString text = QString{"%1 %2%"}.arg(categoryNames[c]).arg(weeks.back().d[c], 0, 'f', 1);
        p.drawText(QPointF{legendX + 18, height() - 6.0}, text);
        legendX += 18 + p.fontMetrics().horizontalAdvance(text) + 14;
    }
}

DroughtViewer::DroughtViewer(Window * parent)
    : Window{parent}
    , monitorImage{this}
    , precipImage{this}
    , outlookImage{this}
    , comboMap{this, {"Current drought", "Change since 1 week ago", "Change since 2 weeks ago", "Change since 4 weeks ago", "Change since 8 weeks ago", "Change since 12 weeks ago",
                      "Change since 26 weeks ago", "Change since 52 weeks ago"}}
    , comboWeek{this, {"Latest week"}}
    , comboKind{this, {"Total precipitation", "Departure from normal", "Percent of normal"}}
    , comboPeriod{this, {"1 and 2 weeks, 1 and 2 months", "3, 4, 5 and 6 months", "9, 12, 18 and 24 months", "2, 3, 4 and 5 years (percent of normal)"}}
    , comboOutlook{this}
    , textMonitor{this, ""}
    , textPrecip{this, ""}
    , textOutlook{this, ""}
{
    setTitle("Drought");
    tabs = new QTabWidget{this};
    // the three tabs
    const auto page = [this] (const QString& name, HBox& row, ZoomImage& image, Text& status, QWidget * extra) {   // a tab: its pickers, a line saying what is shown, the picture, and a chart under it
        auto * widget = new QWidget{tabs};
        auto * column = new QVBoxLayout{widget};
        column->setContentsMargins(4, 4, 4, 4);
        row.addStretch();
        column->addLayout(row.getView());
        column->addWidget(status.getView());
        image.setMinimumHeight(380);
        column->addWidget(&image, 1);
        if (extra) {
            column->addWidget(extra);
        }
        tabs->addTab(widget, name);
    };
    // the pickers
    rowMonitor.addWidget(comboMap);
    rowMonitor.addWidget(comboWeek);
    rowPrecip.addWidget(comboKind);
    rowPrecip.addWidget(comboPeriod);
    rowOutlook.addWidget(comboOutlook);
    chart = new DroughtChart{tabs};
    page("Drought Monitor", rowMonitor, monitorImage, textMonitor, chart);
    page("Precipitation", rowPrecip, precipImage, textPrecip, nullptr);
    page("Outlooks and soil moisture", rowOutlook, outlookImage, textOutlook, nullptr);
    box.addWidgetReal(tabs, 1, Qt::Alignment{});
    box.addWidgetReal(new ActivityLabel{this});
    box.getAndShow(this);

    const std::string cpc = "https://www.cpc.ncep.noaa.gov/products/";
    outlooks = {{"Drought outlook, this month (CPC)", cpc + "expert_assessment/mdohomeweb.png"},
                {"Drought outlook, the season (CPC)", cpc + "expert_assessment/sdohomeweb.png"},
                {"Soil moisture percentile, ensemble (CPC)", cpc + "Drought/Figures/smp/ens.png"},
                {"Soil moisture percentile, change over 7 days", cpc + "Drought/Figures/smp/ens_7.png"},
                {"Soil moisture percentile, change over 30 days", cpc + "Drought/Figures/smp/ens_30.png"},
                {"Soil moisture rank, daily (CPC leaky bucket)", cpc + "Soilmst_Monitoring/Figures/daily/curr.w.rank.daily.gif"},
                {"Standardized precipitation index, 3 months", cpc + "Drought/Figures/index/spi3.web.gif"},
                {"Standardized precipitation index, 6 months", cpc + "Drought/Figures/index/spi6.web.gif"},
                {"Standardized precipitation index, 12 months", cpc + "Drought/Figures/index/spi12.web.gif"},
                {"Standardized precipitation index, 24 months", cpc + "Drought/Figures/index/spi24.web.gif"}};
    std::vector<std::string> names;
    for (const auto& o : outlooks) {
        names.push_back(o.label);
    }
    comboOutlook.setList(names);
    // the map dates: the last twelve Tuesdays (the Monitor is released on Thursdays, for the week that ended on Tuesday)
    std::vector<std::string> weeks{"Latest week"};
    for (int back = 0; back < 12; back++) {
        weeks.push_back(QDate::fromString(QString::fromStdString(mapDate(back)), "yyyyMMdd").toString("MMMM d, yyyy").toStdString());
    }
    comboWeek.setList(weeks);
    comboMap.connect([this] { loadMonitor(); });
    comboWeek.connect([this] { loadMonitor(); });
    comboKind.connect([this] { loadPrecip(); });
    comboPeriod.connect([this] { loadPrecip(); });
    comboOutlook.connect([this] { loadOutlook(); });
    loadMonitor();
    loadPrecip();
    loadOutlook();
    loadWeeks();
}

// the date (yyyymmdd) of the Tuesday `weeksBack` weeks before the newest map that is out
std::string DroughtViewer::mapDate(int weeksBack) const {
    auto day = QDate::currentDate();
    while (day.dayOfWeek() != 2) {   // back to a Tuesday
        day = day.addDays(-1);
    }
    if (QDate::currentDate().daysTo(day) > -2) {   // the map for a Tuesday comes out on the Thursday after
        day = day.addDays(-7);
    }
    return day.addDays(-7 * weeksBack).toString("yyyyMMdd").toStdString();
}

// the picture arrives; if it does not (a map not out yet) and there is a fallback address, that one
void DroughtViewer::showPicture(ZoomImage * target, Text * status, const std::string& url, const std::string& what, int * generation, const std::string& fallback) {
    const int mine = ++(*generation);
    status->setText("Loading " + what + "...");
    new FutureBytes{this, url, [this, target, status, url, what, generation, mine, fallback] (const QByteArray& bytes) {
        if (closed || mine != *generation) {
            return;
        }
        if (bytes.size() < 2000 || bytes.startsWith("<")) {
            if (!fallback.empty()) {
                showPicture(target, status, fallback, what, generation);
                return;
            }
            status->setText(what + " is not available right now.");
            return;
        }
        const bool keepView = target->hasImage();
        keepView ? target->setBytesKeepView(bytes) : target->setBytes(bytes);
        status->setText(what);
    }};
}

void DroughtViewer::loadMonitor() {
    static const char * periods[] = {"", "1W", "2W", "4W", "8W", "12W", "26W", "52W"};
    const int map = std::max(0, comboMap.getIndex());
    const int week = std::max(0, comboWeek.getIndex());   // 0: the latest
    const auto date = mapDate(week == 0 ? 0 : week - 1);
    const auto earlier = mapDate((week == 0 ? 0 : week - 1) + 1);   // when the newest is not out yet
    const std::string base = "https://droughtmonitor.unl.edu/data/";
    const auto url = [&] (const std::string& d) {
        return map == 0 ? base + "png/" + d + "/" + d + "_usdm.png" : base + "chng/png/" + d + "/" + d + "_conus_chng_" + periods[map] + ".png";
    };
    const auto when = QDate::fromString(QString::fromStdString(date), "yyyyMMdd").toString("MMMM d, yyyy").toStdString();
    showPicture(&monitorImage, &textMonitor, url(date), std::string{map == 0 ? "U.S. Drought Monitor, " : "U.S. Drought Monitor change map, "} + when +
                (map == 0 ? "" : " (" + std::string{comboMap.getValue()} + ")") + ".  Produced by the National Drought Mitigation Center (University of Nebraska-Lincoln), the USDA and NOAA.",
                &monitorGeneration, week == 0 ? url(earlier) : std::string{});
}

void DroughtViewer::loadPrecip() {
    static const char * sets[] = {"1wk,2wk,1,2months", "3,4,5,6months", "9,12,18,24months", "2,3,4,5yrs"};
    static const char * kinds[] = {"total", "anom", "percent"};
    int kind = std::max(0, comboKind.getIndex());
    const int period = std::max(0, comboPeriod.getIndex());
    if (period == 3 && kind != 2) {   // the years are only drawn as a percent of normal
        kind = 2;
        comboKind.block();
        comboKind.setIndex(2);
        comboKind.unblock();
    }
    const std::string url = std::string{"https://www.cpc.ncep.noaa.gov/products/Drought/Figures/precip/us.4panel.fordrought.briefing.recent."} + sets[period] + ".rain." + kinds[kind] + ".png";
    showPicture(&precipImage, &textPrecip, url, std::string{comboKind.getValue()} + ", " + comboPeriod.getValue() + ".  NOAA Climate Prediction Center, from gauge and radar analyses.", &precipGeneration);
}

void DroughtViewer::loadOutlook() {
    const int i = std::clamp(comboOutlook.getIndex(), 0, static_cast<int>(outlooks.size()) - 1);
    showPicture(&outlookImage, &textOutlook, outlooks[static_cast<size_t>(i)].url, outlooks[static_cast<size_t>(i)].label + ".  NOAA Climate Prediction Center.", &outlookGeneration);
}

// the weekly area in drought over the last year (CSV: MapDate,AreaOfInterest,None,D0,D1,D2,D3,D4,...)
void DroughtViewer::loadWeeks() {
    const int mine = ++weeksGeneration;
    const auto end = QDate::currentDate(), start = end.addDays(-370);
    const std::string url = "https://usdmdataservices.unl.edu/api/USStatistics/GetDroughtSeverityStatisticsByAreaPercent?aoi=conus&startdate=" + start.toString("M/d/yyyy").toStdString() +
        "&enddate=" + end.toString("M/d/yyyy").toStdString() + "&statisticsType=1";
    new FutureText{this, url, [this, mine] (std::string text) {
        if (closed || mine != weeksGeneration) {
            return;
        }
        std::vector<DroughtChart::Week> weeks;
        const auto lines = QString::fromStdString(text).split('\n', Qt::SkipEmptyParts);
        for (const auto& line : lines) {
            const auto cells = line.trimmed().split(',');
            if (cells.size() < 8) {
                continue;
            }
            DroughtChart::Week w;
            w.date = QDate::fromString(cells[0], "yyyyMMdd");
            if (!w.date.isValid()) {
                continue;
            }
            for (int c = 0; c < 5; c++) {
                w.d[c] = cells[3 + c].toDouble();
            }
            weeks.push_back(w);
        }
        std::sort(weeks.begin(), weeks.end(), [] (const auto& a, const auto& b) { return a.date < b.date; });
        chart->setWeeks(weeks);
    }};
}
