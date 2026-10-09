// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef HAFSVIEWER_H
#define HAFSVIEWER_H

#include <memory>
#include <string>
#include <vector>
#include <QCheckBox>
#include <QWidget>
#include "gfs/GfsRender.h"
#include "ui/Button.h"
#include "ui/BackForward.h"
#include "ui/ComboBox.h"
#include "ui/HBox.h"
#include "models/ChartHover.h"
#include "ui/ZoomImage.h"
#include "ui/Text.h"
#include "ui/VBox.h"
#include "ui/Window.h"

using std::string;

// NCEP's hurricane model (HAFS-A and HAFS-B) for one storm: its 2 km storm-following grid drawn as wind and pressure, simulated radar and satellite, rain, sea surface
// temperature, shear and vorticity, and waves. The model is run only for the active storms and invests, so the storm list is whatever has files in the newest cycle.
// `storm` is the NHC id ("al092026") of the one to show first, "" for the first in the list.
// The forecast intensity of one storm from each version of the hurricane model against forecast hour: maximum wind and minimum pressure, HAFS-A and HAFS-B together.
class HafsIntensityChart : public QWidget {
public:
    explicit HafsIntensityChart(QWidget * parent = nullptr);
    void setData(const std::vector<GfsChart::TrackPoint>& a, const std::vector<GfsChart::TrackPoint>& b, const string& title);

private:
    void paintEvent(QPaintEvent *) override;
    std::vector<GfsChart::TrackPoint> trackA, trackB;
    string title;
};

class HafsIntensityViewer : public Window {
public:
    HafsIntensityViewer(Window * parent, const string& storm, const std::vector<GfsChart::TrackPoint>& a, const std::vector<GfsChart::TrackPoint>& b, const string& cycle);

private:
    VBox box;
};

class HafsViewer : public Window {
public:
    HafsViewer(Window * parent, const string& storm = "", const string& name = "");
    // the model's own id of an NHC storm id: "al092026" is "09l", "ep182026" is "18e"
    static string modelId(const string& nhcId);

private:
    void loadStorms();
    void fillStorms(const std::vector<string>& found, const string& cycle);
    void draw();
    void step(int by);
    void showIntensity();
    string model() const { return comboModel.getIndex() == 1 ? "HAFSB" : "HAFSA"; }
    std::vector<string> storms;
    std::vector<string> productIds;   // before the product list that fills it
    HBox row;
    VBox box;
    ZoomImage image;                     // the chart: wheel / Ctrl + - zoom, drag to pan; the zoom stays across the hours of one chart
    string shownChart;                   // the model, storm and product on view: another hour of it keeps the zoom
    std::unique_ptr<ChartHover> hover;   // the value under the pointer
    ComboBox comboModel;
    ComboBox comboStorm;
    ComboBox comboProduct;
    ComboBox comboTime;
    BackForward backForward;
    Button buttonIntensity;
    QCheckBox * radiiCheck{};                  // the quadrant wind field (34 / 50 / 64 kt) on the chart
    Text textStatus;
    string first;            // the storm to show when the list arrives
    string firstName;
    int drawing{0};
    int loading{0};
    std::shared_ptr<GfsRender::Session> session;
};

#endif  // HAFSVIEWER_H
