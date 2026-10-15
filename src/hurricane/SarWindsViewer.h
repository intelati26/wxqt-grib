// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef SARWINDSVIEWER_H
#define SARWINDSVIEWER_H

#include <memory>
#include <string>
#include <vector>
#include <QComboBox>
#include <QLabel>
#include <QListWidget>
#include "hurricane/UtilitySarWinds.h"
#include "ui/HBox.h"
#include "ui/VBox.h"
#include "ui/Window.h"
#include "ui/ZoomImage.h"

using std::string;
using std::vector;

// NOAA STAR's SAR winds of one storm: the passes still scheduled for it, the scenes made so far (newest first) with the winds measured in each, and each scene's pictures.
// id is the NHC / ATCF id ("ep202026"), name the storm's name, year the season.
class SarWindsViewer : public Window {
public:
    SarWindsViewer(Window * parent, const string& id, const string& name, int year);

private:
    void showScene(int row);
    void showPicture();
    void closeEventCustom() override { closed = true; }
    VBox box;
    HBox rowMain;
    QLabel * schedule{};
    QLabel * measurements{};
    QListWidget * list{};
    QComboBox * picture{};
    ZoomImage image;
    string stormId, stormName;
    int stormYear;
    vector<UtilitySarWinds::Scene> scenes;
    bool closed{false};
};

#endif  // SARWINDSVIEWER_H
