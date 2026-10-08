// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef SURFACEHISTORYVIEWER_H
#define SURFACEHISTORYVIEWER_H

#include <memory>
#include <QWidget>
#include "obs/SurfaceStation.h"
#include "obs/UtilityMetarHistory.h"
#include "ui/Text.h"
#include "ui/VBox.h"
#include "ui/Window.h"

// Strips against time for the last 24 hours of one airport: temperature and dew point, wind speed and gust, altimeter setting.
class SurfaceHistoryChart : public QWidget {
public:
    explicit SurfaceHistoryChart(QWidget * parent = nullptr) : QWidget{parent} { setMinimumSize(640, 480); }
    void setData(const std::vector<UtilityMetarHistory::Ob>& obs, bool fahrenheit, const QString& message);

private:
    void paintEvent(QPaintEvent *) override;
    std::vector<UtilityMetarHistory::Ob> obs;
    bool fahrenheit{true};
    QString message{"Loading..."};
};

// What an airport reported now (the card) with the chart of its day below.
class SurfaceHistoryViewer : public Window {
public:
    SurfaceHistoryViewer(Window * parent, const SurfaceStation&);

private:
    void closeEventCustom() override { closed = true; }
    VBox box;
    Text textCard;
    SurfaceHistoryChart * chart{};
    bool closed{false};
};

#endif  // SURFACEHISTORYVIEWER_H
