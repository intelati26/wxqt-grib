// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef CLIMATECHART_H
#define CLIMATECHART_H

#include <string>
#include <QWidget>
#include "climate/UtilityClimate.h"

using std::string;

// A bar chart of one climate index over the last years: warm (positive) bars red, cool (negative) blue, with the El Niño / La Niña
// line at +/- the threshold when the index has one. The title carries the newest value.
class ClimateChart : public QWidget {
public:
    ClimateChart(const UtilityClimate::IndexInfo&, const UtilityClimate::Series&, int years, QWidget * parent = nullptr);
    QSize sizeHint() const override { return {480, 180}; }

protected:
    void paintEvent(QPaintEvent *) override;

private:
    UtilityClimate::IndexInfo info;
    UtilityClimate::Series series;   // only the years shown
    string newest;
};

#endif  // CLIMATECHART_H
