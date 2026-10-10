// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef DROUGHTVIEWER_H
#define DROUGHTVIEWER_H

#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include <QDate>
#include <QPointer>
#include <QPushButton>
#include <QTableWidget>
#include <QTabWidget>
#include <QWidget>
#include "drought/DroughtHistory.h"
#include "drought/DroughtMap.h"
#include "drought/UtilityDrought.h"
#include "models/ProductPicker.h"
#include "ui/ComboBox.h"
#include "ui/HBox.h"
#include "ui/Text.h"
#include "ui/VBox.h"
#include "ui/Window.h"
#include "ui/ZoomImage.h"

// The share of an area in each drought category by week: lines for D0 (abnormally dry) to D4 (exceptional).
class DroughtChart : public QWidget {
public:
    struct Week {
        QDate date;
        double d[5]{};   // percent of the area in D0 or worse ... D4
    };
    explicit DroughtChart(QWidget * parent = nullptr);
    void setWeeks(const std::vector<Week>& weeks, const QString& title);

private:
    void paintEvent(QPaintEvent *) override;
    std::vector<Week> weeks;
    QString title;
};

// Bars of how much rain fell each month in the area and how far that was from normal.
class PrecipBars : public QWidget {
public:
    struct Month {
        QString label;
        double total{0}, anomaly{0};   // millimeters; the normal is the total less the anomaly
        double rank{-1};               // the month's place among the same month of every year on record, 0 to 100 (100: the wettest or warmest); -1 when there is none
        bool partial{false}, ok{false};
    };
    explicit PrecipBars(QWidget * parent = nullptr);
    void setMonths(const std::vector<Month>& months, const QString& title, bool inches, bool temperature = false);   // temperature: the anomaly is degrees C, red for warm and blue for cold

private:
    void paintEvent(QPaintEvent *) override;
    std::vector<Month> months;
    QString title;
    bool inches{true};
    bool temperature{false};
};

// The multi-year line or bars of one column of an area's history.
class HistoryChart : public QWidget {
public:
    struct Point {
        QString month;
        double value{0};
        bool ok{false};
    };
    explicit HistoryChart(QWidget * parent = nullptr);
    // bars: departures (colored by sign, around zero); else a line. `reference` is a line drawn across (100 for percent of normal, 50 for a rank), NaN for none.
    void setSeries(const std::vector<Point>& points, const QString& title, const QString& unit, bool bars, double reference, bool warmIsRed, double low, double high);

private:
    void paintEvent(QPaintEvent *) override;
    std::vector<Point> points;
    QString title, unit;
    bool bars{false}, warmIsRed{false};
    double reference{NAN}, low{0}, high{0};
};

// The drought dashboard. The area (the country, a state or a county) and the weeks to compare are chosen above the tabs and apply to all of them. The Monitor tab draws the U.S.
// Drought Monitor's own shapes (its KMZ) over the states: the categories of the week, or the cells that moved between two weeks, with the share of the area in each category at both
// dates and the weekly share over the last months. The precipitation and outlook tabs show the Climate Prediction Center's pictures.
class DroughtViewer : public Window {
public:
    explicit DroughtViewer(Window * parent);

private:
    struct Product {
        std::string label;
        std::string url;
    };
    struct Result;   // what a background load of the Monitor brings back
    void loadAreas();
    void loadSpc();   // the SPC's areas in force now (fetched again each time the picker opens)
    void chooseArea();
    void selectArea(const std::string& id);
    void refreshMonitor();
    void loadSeries();
    void loadPrecip();
    void loadPrecipArea();
    void updateHistory();    // adds what the area's history file lacks (in the background), then shows it
    void showHistory();      // the chosen column as a chart
    void exportHistory();   // the area's rain by month (the CPC's one degree analyses)
    void loadOutlook();
    void showPicture(ZoomImage * target, Text * status, const std::string& url, const std::string& what, int * generation);
    std::string mapDate(int weeksBack) const;
    std::vector<UtilityDrought::Area> selectedAreas() const;

    VBox box;
    HBox rowTop;
    QPushButton * buttonArea{};
    ComboBox comboWeek, comboCompare;
    QTabWidget * tabs{};
    DroughtMap * map{};
    QTableWidget * table{};
    DroughtChart * chart{};
    PrecipBars * precipBars{};
    HistoryChart * historyChart{};
    ComboBox comboHistory;
    Text textHistory;
    int historyGeneration{0};
    std::vector<DroughtHistory::Row> history;
    QTableWidget * precipTable{};
    int precipAreaGeneration{0};
    Text textMonitor;
    ZoomImage precipImage, outlookImage;
    ComboBox comboKind, comboPeriod, comboOutlook, comboMetric;
    Text textPrecip, textOutlook;
    HBox rowPrecip, rowOutlook;
    std::vector<Product> outlooks;
    std::shared_ptr<std::vector<UtilityDrought::Area>> states, counties, offices, spc, meso;   // spc: the mesoscale discussions and watches in force now
    std::string areaId{"US"};
    QPointer<ProductPicker> picker;
    std::mutex seriesMutex;
    std::vector<DroughtChart::Week> series;
    int monitorGeneration{0}, precipGeneration{0}, outlookGeneration{0}, seriesGeneration{0};
    bool closed{false};
    void closeEventCustom() override { closed = true; }
};

#endif  // DROUGHTVIEWER_H
