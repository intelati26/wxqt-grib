// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef MODELCOMPAREVIEWER_H
#define MODELCOMPAREVIEWER_H

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QPushButton>
#include "cfs/EnsemblePercentiles.h"
#include "ui/Window.h"

class CompareChart;
class ModelViewer;

// Two ensembles at one place, like for like (docs/plans.md, "Model comparison at a place"): the same field at the same hours from each model's members (the median and the 25 to 75 percent band of each),
// and below them the difference of the medians, hour by hour and averaged over each day: "the Euro is cooler than the GEFS" as a number and a shape in time. A click on the chart picks a time, and "Why?"
// opens the Model Viewer on the maps of that time, the first model's 500 mb heights and winds (or the 250 mb jet) with the other model's height lines over them, so the position of the trough or the jet
// stream that makes the difference can be seen. The members come from Open-Meteo's ensemble service (as in the percentile window); the maps are the program's own models.
class ModelCompareViewer : public Window {
public:
    ModelCompareViewer(Window * parent, double lat = 39.1, double lon = -94.6, const std::string& modelA = "ecmwf_ifs025", const std::string& modelB = "gfs025", const std::string& field = "temperature");
    void load();
    QPixmap picture() const;
    QString statusText() const;
    // the maps of the time picked (or the first hour with a difference when none is picked): the Model Viewer, as "Why?" does
    ModelViewer * why();   // (null when there is nothing to show)
    // the program's model for an Open-Meteo ensemble id ("gfs025" -> "GEFS"); "" when the program has none
    static std::string mapModel(const std::string& id);

private:
    QComboBox * place{};
    QDoubleSpinBox * latBox{};
    QDoubleSpinBox * lonBox{};
    QComboBox * modelA{};
    QComboBox * modelB{};
    QComboBox * field{};
    QComboBox * mapChoice{};
    QPushButton * go{};
    QPushButton * whyButton{};
    QLabel * status{};
    CompareChart * chart{};
    int generation{0};
};

#endif  // MODELCOMPAREVIEWER_H
