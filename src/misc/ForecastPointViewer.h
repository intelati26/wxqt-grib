// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef FORECASTPOINTVIEWER_H
#define FORECASTPOINTVIEWER_H

#include <functional>
#include <memory>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>
#include "misc/UtilityForecastPoint.h"
#include "ui/Button.h"
#include "ui/ComboBox.h"
#include "ui/HBox.h"
#include "ui/VBox.h"
#include "ui/Window.h"

// The weekly summary of a forecast point as a table (a day to a column), tinted by value. `compact` keeps the rows of most use (for the home screen).
class ForecastPointTable : public QTableWidget {
public:
    explicit ForecastPointTable(QWidget * parent = nullptr);
    void setData(const UtilityForecastPoint::Data& data, bool compact);
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;
};

// The hourly graph of one series of a forecast point: a line (or bars for amounts and chances) over the days, the days marked, the hour under the pointer read out.
class ForecastPointChart : public QWidget {
public:
    explicit ForecastPointChart(QWidget * parent = nullptr);
    void setSeries(const std::shared_ptr<UtilityForecastPoint::Data>& data, const std::string& key);
    QSize sizeHint() const override { return {700, 260}; }

private:
    void paintEvent(QPaintEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    std::shared_ptr<UtilityForecastPoint::Data> data;
    std::string key;
};

// What the outlooks say of the point for the next three days: the severe thunderstorm and the excessive rainfall category in the colors of the outlooks.
class ForecastPointOutlooks : public QTableWidget {
public:
    explicit ForecastPointOutlooks(QWidget * parent = nullptr);
    void setData(const UtilityForecastPoint::Data& data);
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;
};

// The home screen's card: where the point is, the outlooks, the compact weekly table, and a button for the full page.
class CardForecastPoint : public QWidget {
public:
    explicit CardForecastPoint(Window * parent);
    void setData(const std::shared_ptr<UtilityForecastPoint::Data>& data);
    std::function<void()> onOpen;

private:
    QLabel * title;
    QPushButton * open;
    ForecastPointTable * table;
    ForecastPointOutlooks * outlooks;
};

// The full page, as the NWS "IDSS Forecast Points" page has it: the weekly summary with every row, the outlooks, the hourly graph of any of its series (or all of them one under
// another), the hourly table, the forecast discussion of the office, and any of the saved locations or a point of one's own.
class ForecastPointViewer : public Window {
public:
    ForecastPointViewer(Window * parent, const std::shared_ptr<UtilityForecastPoint::Data>& data);

private:
    void apply();                 // fill every part from `data`
    void loadPoint(double lat, double lon);
    void showSeries();
    void fillHourly();
    void fillAllGraphs();
    void exportCsv();
    void openDiscussion();
    void choosePoint();
    void closeEventCustom() override { closed = true; }
    VBox box;
    HBox rowTop;
    ComboBox comboPoint;
    Button buttonOther;
    Button buttonRefresh;
    Button buttonDiscussion;
    Button buttonCsv;
    QLabel * title;
    QTabWidget * tabs;
    ForecastPointTable * table;
    ForecastPointOutlooks * outlooks;
    ComboBox comboSeries;
    ForecastPointChart * chart;
    QTableWidget * hourlyTable;
    QWidget * allGraphs;
    QVBoxLayout * allLayout;
    std::shared_ptr<UtilityForecastPoint::Data> data;
    std::vector<std::pair<double, double>> savedPoints;   // the saved locations, as the combo lists them (the last entry is the point of one's own, when there is one)
    int generation{0};
    bool closed{false};
};

#endif  // FORECASTPOINTVIEWER_H
