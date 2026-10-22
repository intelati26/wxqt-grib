// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef HAFSINTENSITY_H
#define HAFSINTENSITY_H

#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <QWidget>
#include "gfs/GfsRender.h"
#include "ui/VBox.h"
#include "ui/Window.h"

using std::string;

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
    // reads both versions' tracks of a storm in the background and opens the viewer; `say` is told how it went (empty when it opened)
    static void open(Window * parent, const string& storm, std::function<void(const string&)> say);

private:
    VBox box;
};

// Helpers for the hurricane model's storm ids ("09l": the NHC number and the basin letter)
namespace Hafs {
    // the model's own id of an NHC storm id: "al092026" is "09l", "ep182026" is "18e"
    string modelId(const string& nhcId);
    // "09L Atlantic": how a storm of the model is listed
    string label(const string& modelId);
    // true for "09l", "18e" ... (a storm of the model, not an area)
    bool isStormId(const string& id);
}

#endif  // HAFSINTENSITY_H
