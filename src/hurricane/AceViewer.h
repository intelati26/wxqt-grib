// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef ACEVIEWER_H
#define ACEVIEWER_H

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

// ACE (accumulated cyclone energy) through the year, day by day: this season's running total against the 1991-2020 average (a line) and the lowest and
// highest of those years (a band), another year to compare if wanted, and the ACE added each day this season as bars underneath. Hover for the day's numbers.
class AceChart : public QWidget {
public:
    explicit AceChart(QWidget * parent = nullptr) : QWidget{parent} { setMinimumSize(560, 250); setMouseTracking(true); ChartExport::install(this, "ACE by day"); }
    // `compareYear` 0 for none. The history comes from HURDAT2, the season under way from the ATCF best tracks.
    void setData(const std::shared_ptr<HurricaneData::SeasonData>& data, int compareYear);
    // the season so far against the average up to the same day, in words ("ACE 12.7 by 8 Oct: 38 % of the 1991-2020 average for the date (the 41st highest of 36 years...")
    static QString standing(const HurricaneData::SeasonData& data);
    static int lastDay(const HurricaneData::SeasonData& data);   // the day of the year of the newest best-track record of this season

private:
    void paintEvent(QPaintEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    QRectF plot() const;
    QRectF barsArea() const;
    std::shared_ptr<HurricaneData::SeasonData> data;
    int compareYear{0};
    vector<double> season, compare, perDay;
    UtilitySeason::Climatology climatology;
    int today{0};
};

class AceViewer : public Window {
public:
    AceViewer(Window * parent, const std::shared_ptr<HurricaneData::SeasonData>& atlantic, const std::shared_ptr<HurricaneData::SeasonData>& pacific);

private:
    void apply();
    void fillYears();
    bool filling{false};
    VBox box;
    HBox row;
    ComboBox comboBasin;
    ComboBox comboCompare;
    Text textSummary;
    AceChart * chart{};
    std::shared_ptr<HurricaneData::SeasonData> datas[2];
};

#endif  // ACEVIEWER_H
