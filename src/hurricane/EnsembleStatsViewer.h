// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef ENSEMBLESTATSVIEWER_H
#define ENSEMBLESTATSVIEWER_H

#include "ui/ChartExport.h"
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <QWidget>
#include "hurricane/HurricaneData.h"
#include "hurricane/UtilityEnsembleStats.h"
#include "ui/HBox.h"
#include "ui/Text.h"
#include "ui/VBox.h"
#include "ui/Window.h"

using std::string;
using std::vector;

// How the members of every ensemble are spread over the forecast, all on one screen: for each of AIFS ENS, IFS ENS and GEFS the 25-75 % band
// and median of the maximum wind and of the minimum pressure, the radius around the mean position that holds half and nine tenths of the
// members, and the share of members that are still a cyclone or reach tropical-storm and hurricane wind. NHC's official forecast and the
// unperturbed IFS / AIFS runs are drawn over them.
class EnsembleChart : public QWidget {
public:
    explicit EnsembleChart(QWidget * parent = nullptr) : QWidget{parent} { setMinimumSize(640, 430); ChartExport::install(this, "Ensemble statistics"); }
    void setData(const std::shared_ptr<HurricaneData::StormData>& storm, const std::shared_ptr<HurricaneData::EnsembleData>& ensembles);
    // which families are drawn: the ensembles by label ("AIFS ENS", "IFS ENS", "GEFS"), "runs" (the unperturbed AIFS / IFS), "nhc" (NHC's forecast dots)
    void setShown(const string& family, bool shown);
    static bool isEnsemble(const string& label) { return label == "AIFS ENS" || label == "IFS ENS" || label == "GEFS"; }

private:
    struct Series {
        string label;
        string cycle;
        QColor color;
        vector<UtilityEnsembleStats::Hour> hours;   // up to the last hour with at least five members
        int members{0};
    };
    void paintEvent(QPaintEvent *) override;
    bool shown(const string& family) const;
    vector<Series> series;
    std::map<string, bool> hidden;
    std::shared_ptr<HurricaneData::StormData> storm;
    std::shared_ptr<HurricaneData::EnsembleData> ensembles;
};

class EnsembleStatsViewer : public Window {
public:
    EnsembleStatsViewer(Window * parent, const std::shared_ptr<HurricaneData::StormData>& storm,
                        const std::shared_ptr<HurricaneData::EnsembleData>& ensembles);

private:
    VBox box;
    HBox rowFamilies;
    Text textStatus;
    EnsembleChart * chart{};
};

#endif  // ENSEMBLESTATSVIEWER_H
