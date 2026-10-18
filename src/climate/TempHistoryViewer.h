// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef TEMPHISTORYVIEWER_H
#define TEMPHISTORYVIEWER_H

#include <memory>
#include <string>
#include <vector>
#include <QComboBox>
#include <QDate>
#include <QDateEdit>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QPushButton>
#include "ui/Window.h"

class TempHistoryChart;

// How warm a week, a month or a year has been against the same stretch of every year since 1940, at a place: the daily temperatures of the ERA5 reanalysis (ECMWF; here through Open-Meteo's
// archive service, one request for the whole record, about 3 seconds), the stretch's average for each year as a bar (blue under the 1991-2020 average, red over it), the year asked for
// outlined, the rank among the years and the warmest and coldest. A week is the seven days that end on the date, a month the days of its month up to the date, a year the days from January 1 up
// to it, so a year in progress is held against the others at the same point of their year.
class TempHistoryViewer : public Window {
public:
    TempHistoryViewer(Window * parent, double lat, double lon, const QDate& upTo = {}, bool station = false);

    void setMetric(int index) { metricBox->setCurrentIndex(index); }   // 0 average, 1 high, 2 low, 3 total precipitation
    struct Day {
        QDate date;
        double mean{0}, high{0}, low{0};   // degrees C; NaN where the service has none
        double precip{0};                  // mm of the day; NaN where the service has none
    };

private:
    void load();
    void loadEra();                     // the daily record of ERA5 at the place
    void loadStations();                // the stations round the place, then the best one's record
    void loadStation(const QString& id, const QString& name);
    void saveCsv();
    void savePicture();
    void takeDays(std::vector<Day> list, const QString& note);
    void recompute();
    void closeEventCustom() override { closed = true; }
    QDoubleSpinBox * latBox{};
    QDoubleSpinBox * lonBox{};
    QComboBox * sourceBox{};
    QComboBox * stationBox{};
    QComboBox * savedBox{};
    QComboBox * periodBox{};
    QComboBox * metricBox{};
    QDateEdit * dateEdit{};
    QPushButton * reloadButton{};
    QPushButton * csvButton{};
    QPushButton * pictureButton{};
    QString csvTitle;
    std::string csvUnit{"deg C"};
    double csvClimate{0};
    std::vector<std::pair<int, double>> shownYears;   // what the chart shows (year, value in the native unit), for the file
    QLabel * status{};
    QLabel * summary{};
    TempHistoryChart * chart{};
    std::vector<Day> days;
    double usedLat{0}, usedLon{0};
    int generation{0};                  // a request that is not the newest is dropped when it arrives
    bool closed{false};
    bool loading{false};
};

#endif  // TEMPHISTORYVIEWER_H
