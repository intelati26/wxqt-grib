// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef MODELBIASVIEWER_H
#define MODELBIASVIEWER_H

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QPushButton>
#include "cfs/ModelBias.h"
#include "ui/Window.h"

class BiasChart;

// How wrong two models have been at a place, and in which direction (docs/plans.md, "Model comparison at a place", the second step): for each lead time (1 to 7 days ahead) the bias (forecast minus what
// happened, the bars) and the size of the miss (the ticks), over the last 14, 30 or 45 days; and below, the error of each day at the lead chosen, so that a drift ("the Euro has gone cold this week")
// shows. What happened is ERA5, the reanalysis (about five days behind, so the newest days have no truth); the past runs are Open-Meteo's previous runs service. The variables are the ones of the
// ensemble windows (ensembles.json adds to them).
class ModelBiasViewer : public Window {
public:
    ModelBiasViewer(Window * parent, double lat = 39.1, double lon = -94.6, const std::string& modelA = "ecmwf_ifs025", const std::string& modelB = "gfs_global", const std::string& variable = "temperature_2m", int days = 30, int lead = 3);
    void load();
    void setLowerView(int how) { lowerView->setCurrentIndex(how); }   // 0 the error of each day, 1 how each day's forecast changed
    void setJudge(int how) { judge->setCurrentIndex(how); }   // 0 ERA5, 1 a station's highs, 2 lows, 3 rain
    QPixmap picture() const;
    QString statusText() const;

private:
    QComboBox * place{};
    QDoubleSpinBox * latBox{};
    QDoubleSpinBox * lonBox{};
    QComboBox * modelA{};
    QComboBox * modelB{};
    QComboBox * field{};
    QComboBox * judge{};
    QComboBox * lowerView{};
    QComboBox * period{};
    QComboBox * lead{};
    QPushButton * go{};
    QLabel * status{};
    BiasChart * chart{};
    int generation{0};
};

#endif  // MODELBIASVIEWER_H
