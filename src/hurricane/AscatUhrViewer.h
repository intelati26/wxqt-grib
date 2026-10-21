// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef ASCATUHRVIEWER_H
#define ASCATUHRVIEWER_H

#include <string>
#include <vector>
#include <QComboBox>
#include <QLabel>
#include <QListWidget>
#include "ui/Window.h"
#include "ui/VBox.h"
#include "ui/ZoomImage.h"

using std::string;
using std::vector;

// NOAA OSPO's ASCAT ultra high resolution (UHR) winds of the storms (MetOp-B and MetOp-C, a 4.45 km wind vector cell, the AVE retrieval of BYU / KNMI): the passes of one storm newest first with
// the pictures of each. The "M" files are the colour wind maps (AVE2M, AVE3M: speed and barbs); AVE the backscatter map; AVE2 and AVE3 the strips of the single beams. atcfId ("wp292026") opens on that storm.
class AscatUhrViewer : public Window {
public:
    AscatUhrViewer(Window * parent, const string& atcfId = {});

private:
    struct Pass {
        string file;       // SEAP-UHR-ave3-29W41271_v1r0_m03_s202610210600000_e202610210900000_c202610211043369
        string satellite;  // MetOp-B or MetOp-C
        string type;       // ave, ave2, ave2m, ave3, ave3m
        string storm;      // 29W
        string start, end; // yyyyMMddHHmm
    };
    void fill(const string& atcfId);
    void fillPasses();
    void showPass(int row);
    void closeEventCustom() override { closed = true; }
    VBox box;
    QComboBox * stormBox{};
    QComboBox * typeBox{};
    QListWidget * list{};
    QLabel * note{};
    ZoomImage image;
    vector<Pass> passes;                           // every pass of the lists
    vector<Pass> shown;                            // those of the storm and picture type chosen, newest first
    vector<std::pair<string, string>> storms;      // value, name
    bool closed{false};
};

#endif  // ASCATUHRVIEWER_H
