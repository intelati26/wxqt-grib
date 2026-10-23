// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef NHCBASINS_H
#define NHCBASINS_H

#include <memory>
#include <string>
#include <vector>
#include <QLabel>
#include <QVBoxLayout>
#include <QWidget>
#include "hurricane/HurricaneData.h"
#include "ui/Window.h"

using std::string;
using std::vector;

// The NHC screen's basin by basin view: for the Atlantic, the East Pacific and the Central Pacific, the Tropical Weather Outlook's areas of possible development (the 2 and 7 day chances) and
// the storms and invests of that basin, each linked to the other: an invest or storm that sits inside an outlook area is listed under it, with buttons for its track map and its phase space; the
// ones outside every area follow. An invest's position is the newest fix of its best track file (the list of storms gives none).
class NhcBasins : public QWidget {
public:
    NhcBasins(Window * owner, QWidget * parent);

private:
    struct System {
        HurricaneData::StormEntry entry;
        string basin;           // "al", "ep", "cp"
        double lat{0.0}, lon{0.0};
        bool placed{false};
    };
    void fill();
    Window * owner;
    QVBoxLayout * layout{};
    std::shared_ptr<HurricaneData::OutlookData> outlook;
    std::shared_ptr<vector<System>> systems;
    int generation{0};
    bool closed{false};
};

#endif  // NHCBASINS_H
