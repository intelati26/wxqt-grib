// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef MODELCOMPAREVIEWER_H
#define MODELCOMPAREVIEWER_H

#include <cmath>
#include <functional>
#include <QComboBox>
#include <QDateTime>
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
    ModelCompareViewer(Window * parent, double lat = 39.1, double lon = -94.6, const std::string& modelA = "ecmwf_ifs025", const std::string& modelB = "gfs025", const std::string& field = "temperature_2m");
    void load();
    void setSpread(int how) { spread->setCurrentIndex(how); }   // 0 bands, 1 every member, 2 both, 3 box and whisker
    QPixmap picture() const;
    QString statusText() const;
    // the maps of the time picked (or the first hour with a difference when none is picked): the Model Viewer, as "Why?" does
    // Every ensemble of the list at the time picked (or the day with the largest difference): a box for each model's members at that hour, with the members as dots, in a window of its own. Blocking reads go
    // on a thread; the window opens when they are in.
    void allModels();
    std::function<void(QWidget *)> allShown;   // called with the window of allModels() once it is up (a development aid)
    ModelViewer * why();
    // The Model Viewer on the maps of a time (also used by the bias window): `main` and `other` are the program's models ("GFS", "IFS-ENS"), `chart` the chart asked for; `message` says what was opened or why not
    static ModelViewer * showMaps(Window * parent, const std::string& main, const std::string& other, const std::string& chart, const QDateTime& when, QString& message, double lat = std::nan(""), double lon = std::nan(""));   // (null when there is nothing to show)
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
    QComboBox * spread{};
    QPushButton * go{};
    QPushButton * whyButton{};
    QPushButton * allButton{};
    QLabel * status{};
    CompareChart * chart{};
    int generation{0};
    int placeOffset{0};   // seconds the place's clock is ahead of UTC, from the last load
};

#endif  // MODELCOMPAREVIEWER_H
