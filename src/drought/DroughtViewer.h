// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef DROUGHTVIEWER_H
#define DROUGHTVIEWER_H

#include <string>
#include <vector>
#include <QDate>
#include <QTabWidget>
#include <QWidget>
#include "ui/ComboBox.h"
#include "ui/HBox.h"
#include "ui/Text.h"
#include "ui/VBox.h"
#include "ui/Window.h"
#include "ui/ZoomImage.h"

// The share of the contiguous United States in each drought category by week (the U.S. Drought Monitor's statistics): lines for D0 (abnormally dry) to D4 (exceptional).
class DroughtChart : public QWidget {
public:
    struct Week {
        QDate date;
        double d[5]{};   // percent of the area in D0 or worse ... D4
    };
    explicit DroughtChart(QWidget * parent = nullptr);
    void setWeeks(const std::vector<Week>& weeks);

private:
    void paintEvent(QPaintEvent *) override;
    std::vector<Week> weeks;
};

// The drought dashboard: the U.S. Drought Monitor map (the week's, and how it changed over 1 to 52 weeks) with the area in each category over the last year; how much rain fell and
// how that compares with normal over periods from a week to five years; the Climate Prediction Center's drought outlooks, soil moisture and the standardized precipitation index.
class DroughtViewer : public Window {
public:
    explicit DroughtViewer(Window * parent);

private:
    struct Product {
        std::string label;
        std::string url;
    };
    void loadMonitor();
    void loadPrecip();
    void loadOutlook();
    void loadWeeks();
    void showPicture(ZoomImage * target, Text * status, const std::string& url, const std::string& what, int * generation, const std::string& fallback = {});
    std::string mapDate(int weeksBack) const;
    VBox box;
    HBox rowMonitor, rowPrecip, rowOutlook;
    QTabWidget * tabs{};
    ZoomImage monitorImage;
    ZoomImage precipImage;
    ZoomImage outlookImage;
    ComboBox comboMap, comboWeek;
    ComboBox comboKind, comboPeriod;
    ComboBox comboOutlook;
    Text textMonitor, textPrecip, textOutlook;
    DroughtChart * chart{};
    std::vector<Product> outlooks;
    int monitorGeneration{0}, precipGeneration{0}, outlookGeneration{0}, weeksGeneration{0};
    bool closed{false};
    void closeEventCustom() override { closed = true; }
};

#endif  // DROUGHTVIEWER_H
