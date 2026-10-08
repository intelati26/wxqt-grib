// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef SEASONVIEWER_H
#define SEASONVIEWER_H

#include "ui/ChartExport.h"
#include <memory>
#include <vector>
#include <QString>
#include <QWidget>
#include "hurricane/HurricaneData.h"
#include "ui/ComboBox.h"
#include "ui/HBox.h"
#include "ui/Text.h"
#include "ui/VBox.h"
#include "ui/Window.h"

// The Atlantic hurricane seasons: a bar for each year of ACE (accumulated cyclone energy), named storms, hurricanes or major hurricanes from the
// HURDAT2 database (1851 on) and this season so far from the ATCF best tracks, against the 1991-2020 average, and this season's storms as a table.
class SeasonChart : public QWidget {
public:
    explicit SeasonChart(QWidget * parent = nullptr) : QWidget{parent} { setMinimumSize(640, 280); setMouseTracking(true); ChartExport::install(this, "Hurricane seasons"); }
    void setData(const std::vector<UtilitySeason::Season>& seasons, int currentYear);
    void setView(int metric, int firstYear, int group = 0);   // metric: 0 ACE, 1 named storms, 2 hurricanes, 3 major hurricanes, 4 TIKE; group: 0 each season, 1 decades (the average season of each), 2 decades (the total)

private:
    double value(const UtilitySeason::Season&) const;
    QRectF plot() const;
    void paintEvent(QPaintEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    std::vector<UtilitySeason::Season> seasons;
    int currentYear{0};
    int metric{0};
    int firstYear{1950};
    int group{0};
    std::vector<UtilitySeason::Season> grouped(std::vector<double>& divisors) const;   // the seasons shown, as they are or as decades
};

class SeasonViewer : public Window {
public:
    SeasonViewer(Window * parent, const std::shared_ptr<HurricaneData::SeasonData>& data);

private:
    void apply();
    VBox box;
    HBox row;
    ComboBox comboMetric;
    ComboBox comboYears;
    ComboBox comboGroup;
    Text textSummary;
    SeasonChart * chart{};
    std::shared_ptr<HurricaneData::SeasonData> data;
    std::vector<UtilitySeason::Season> seasons;
};

#endif  // SEASONVIEWER_H
