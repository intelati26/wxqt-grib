// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef ACEVIEWER_H
#define ACEVIEWER_H

#include <map>
#include <memory>
#include <vector>
#include <QListWidget>
#include <QString>
#include <QWidget>
#include "ui/ChartExport.h"
#include "hurricane/HurricaneData.h"
#include "ui/Button.h"
#include "ui/ComboBox.h"
#include "ui/HBox.h"
#include "ui/Text.h"
#include "ui/VBox.h"
#include "ui/Window.h"

// ACE (accumulated cyclone energy) or TIKE (track integrated kinetic energy, estimated from the wind radii; HURDAT2 has them from 2004) through the year, day by
// day. "Running total": the total at each day for the seasons chosen against the 1991-2020 average (ACE; 2004-2020 for TIKE) as a line, with the lowest to the
// highest of those years as a band, and the amount added each day this season as bars underneath. "Each day": the amount added on each day, a bar series for each
// season chosen against the average day. Hover for the day's numbers.
class AceChart : public QWidget {
public:
    using Metric = UtilitySeason::Metric;
    explicit AceChart(QWidget * parent = nullptr) : QWidget{parent} {
        setMinimumSize(560, 250);
        setMouseTracking(true);
        ChartExport::install(this, "ACE / TIKE");   // right-click: save as PNG or PDF, or copy (the running total and each day alike)
    }
    // `years` are the seasons drawn (empty: this season); `daily` false for the running total, true for the amount of each day
    void setData(const std::shared_ptr<HurricaneData::SeasonData>& data, Metric metric = Metric::Ace, bool daily = false, const std::vector<int>& years = {});
    // the season so far against the average up to the same day, in words ("ACE 12.7 through 8 Oct: 12 % of the 1991-2020 average for the date ...")
    static QString standing(const HurricaneData::SeasonData& data, Metric metric = Metric::Ace);
    static int lastDay(const HurricaneData::SeasonData& data);   // the day of the year of the newest best-track record of this season
    static QString unitsName(Metric metric) { return metric == Metric::Ace ? "ACE" : "TIKE"; }

private:
    struct Series {
        int year{0};
        std::vector<double> total;      // 367 elements
        std::vector<double> perDay;
        QColor color;
    };
    void paintEvent(QPaintEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    QRectF plot() const;
    QRectF barsArea() const;
    std::shared_ptr<HurricaneData::SeasonData> data;
    Metric metric{Metric::Ace};
    bool daily{false};
    std::vector<Series> series;
    std::vector<double> meanPerDay;
    UtilitySeason::Climatology climatology;
    int today{0};
};

class AceViewer : public Window {
public:
    AceViewer(Window * parent, const std::shared_ptr<HurricaneData::SeasonData>& atlantic, const std::shared_ptr<HurricaneData::SeasonData>& pacific);

private:
    void apply();
    void fillYears();
    QString standingText() const;
    VBox box;
    HBox row;
    HBox rowMain;
    ComboBox comboBasin;
    ComboBox comboMetric;
    ComboBox comboMode;
    Button buttonClear;
    Button buttonTop;
    Text textSummary;
    AceChart * chart{};
    QListWidget * list{};
    bool filling{false};
    std::shared_ptr<HurricaneData::SeasonData> datas[2];
};

#endif  // ACEVIEWER_H
