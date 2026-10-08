// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef DROPSONDEVIEWER_H
#define DROPSONDEVIEWER_H

#include <QString>
#include <QWidget>
#include "hurricane/UtilityDropsonde.h"
#include "ui/Button.h"
#include "ui/HBox.h"
#include "ui/VBox.h"
#include "ui/Window.h"

// A reconnaissance dropsonde in the way a forecaster reads one: a simple skew-T of the temperature and dew point from the surface to the top of the report,
// the wind barbs beside it, a hodograph of the winds (the tip of the wind vector at each level, the surface first), the table of the mandatory levels,
// the table of the winds below 700 mb, and the mean winds of the lowest 500 m and 150 m.
class DropsondeChart : public QWidget {
public:
    explicit DropsondeChart(const UtilityDropsonde::Drop& drop, QWidget * parent = nullptr);
    static QString compass(double degrees);                              // 135 -> "SE"
    static QString windText(double direction, double speed);             // "SE at 27 kt"
    // the mean wind of the lowest `meters` above the surface, from the heights the sounding screen builds (filled in between the reported ones)
    static bool meanWind(const UtilityDropsonde::Drop&, double meters, double& direction, double& speed);
    static double relativeHumidity(double temperature, double dewPoint);

private:
    void paintEvent(QPaintEvent *) override;
    UtilityDropsonde::Drop drop;
};

class DropsondeViewer : public Window {
public:
    DropsondeViewer(Window * parent, const UtilityDropsonde::Drop& drop);

private:
    VBox box;
    HBox row;
    Button buttonFull;
    DropsondeChart * chart{};
    UtilityDropsonde::Drop drop;
};

#endif  // DROPSONDEVIEWER_H
