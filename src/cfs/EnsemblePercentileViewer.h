// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef ENSEMBLEPERCENTILEVIEWER_H
#define ENSEMBLEPERCENTILEVIEWER_H

#include <memory>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QPushButton>
#include "cfs/EnsemblePercentiles.h"
#include "ui/Window.h"

class PercentileChart;

// The ensemble at a place as percentiles (docs/plans.md, "Ensemble percentiles at a place"): 2 m temperature as a band from the lowest to the highest member with the 10 to 90 and 25 to 75 percent bands and the
// median, the precipitation added up since the first hour (the same bands), and below it the chance that a six hour period has measurable rain or snow (the share of members with at least 0.01 in). The members
// come from Open-Meteo's ensemble service, one request for both fields; a place is a saved location or any latitude and longitude. Move over the chart to read the numbers at that time.
class EnsemblePercentileViewer : public Window {
public:
    explicit EnsemblePercentileViewer(Window * parent, double lat = 39.1, double lon = -94.6, const std::string& model = "gfs025");
    // load now (a route of the development aids and a way to open it ready); `done` runs when the chart is drawn or the load failed
    void load();
    QPixmap picture() const;
    QString statusText() const;

private:
    QComboBox * place{};
    QDoubleSpinBox * latBox{};
    QDoubleSpinBox * lonBox{};
    QComboBox * model{};
    QPushButton * go{};
    QLabel * status{};
    PercentileChart * chart{};
    bool loading{false};
    int generation{0};
};

#endif  // ENSEMBLEPERCENTILEVIEWER_H
