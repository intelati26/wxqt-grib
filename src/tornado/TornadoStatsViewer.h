// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef TORNADOSTATSVIEWER_H
#define TORNADOSTATSVIEWER_H

#include <map>
#include <memory>
#include <vector>
#include <QListWidget>
#include <QWidget>
#include "tornado/TornadoData.h"
#include "ui/Button.h"
#include "ui/ComboBox.h"
#include "ui/HBox.h"
#include "ui/Text.h"
#include "ui/VBox.h"
#include "ui/Window.h"

// Tornadoes counted over time: by day of the year, week, month, year or decade; the count, the fatalities or the injuries. Over the year (day, week, month) the seasons
// ticked in the list are drawn against the 1991-2020 average, as a running total through the year (with the lowest to the highest of those years as a band) or the
// amount in each day, week or month. By year and decade, bars for all the years, the average of 1991-2020 as a line. Hover for the numbers.
class TornadoChart : public QWidget {
public:
    using Group = UtilityTornado::Group;
    using Metric = UtilityTornado::Metric;
    explicit TornadoChart(QWidget * parent = nullptr);
    // `tornadoes` are those that pass the filters; `years` the seasons drawn for day / week / month; `cumulative` a running total
    void setData(const std::shared_ptr<const TornadoData::Database>& db, const std::vector<const UtilityTornado::Tornado *>& tornadoes, Group group, Metric metric,
                 bool cumulative, const std::vector<int>& years);
    static QString metricName(Metric m) { return m == Metric::Count ? "Tornadoes" : m == Metric::Deaths ? "Fatalities" : "Injuries"; }

private:
    void paintEvent(QPaintEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    int buckets() const;                 // 366, 53, 12 for day / week / month
    void paintHeatmap(class ChartPainter&);
    QRectF plot() const;
    struct Series {
        int year{0};
        std::vector<double> each;        // by bucket (index 0 unused), the amount in it
        std::vector<double> total;       // running total
        QColor color;
    };
    std::shared_ptr<const TornadoData::Database> db;
    Group group{Group::DayOfYear};
    Metric metric{Metric::Count};
    bool cumulative{true};
    std::vector<Series> series;
    std::vector<double> averageEach;     // the mean of 1991-2020 by bucket
    std::vector<double> averageTotal;
    std::vector<double> lowest, highest; // the range of the running totals over 1991-2020
    std::vector<std::pair<int, double>> bars;   // year or decade, value
    double barAverage{0.0};
    std::vector<int> highlight;
    std::vector<std::pair<int, std::vector<double>>> heat;   // the heatmap: each year's weeks (index 1 .. 53), oldest first, then the average of 1991-2020 under year 0
    double heatMax{1.0};
};

class TornadoStatsViewer : public Window {
public:
    TornadoStatsViewer(Window * parent, const std::shared_ptr<const TornadoData::Database>& db);

private:
    void apply();
    void fillYears();
    QString standing(const std::vector<const UtilityTornado::Tornado *>&) const;
    VBox box;
    HBox row;
    HBox rowMain;
    ComboBox comboMetric;
    ComboBox comboGroup;
    ComboBox comboMode;
    ComboBox comboRating;
    ComboBox comboState;
    Button buttonClear;
    Button buttonTop;
    Button buttonRecent;
    Button buttonYears;
    Text textSummary;
    TornadoChart * chart{};
    QListWidget * list{};
    bool filling{false};
    std::shared_ptr<const TornadoData::Database> db;
};

#endif  // TORNADOSTATSVIEWER_H
