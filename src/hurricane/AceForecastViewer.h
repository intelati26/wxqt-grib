// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef ACEFORECASTVIEWER_H
#define ACEFORECASTVIEWER_H

#include <memory>
#include <string>
#include <vector>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QWidget>
#include "hurricane/UtilityAceForecast.h"
#include "ui/Window.h"

class AceBoard;

// The accumulated cyclone energy a storm is forecast to make, from the Weather Lab large ensemble (FNV3, 1000 members): the running total of the members by hour (the mean, the median, the
// percentiles and the extremes of those that form the storm, and the mean of all of them, a member that does not form having none), the ACE made in each hour, the numbers at the hour chosen
// (click a bar or the lines) and how the members are spread at that hour and over the whole life of the storm. A storm that exists or a possible one is chosen in the list. The data is Google's
// (experimental, with its credit on the picture).
class AceForecastViewer : public Window {
public:
    explicit AceForecastViewer(Window * parent);

private:
    void load();
    void fillList();
    void show(int index);
    void closeEventCustom() override { closed = true; }
    struct Loaded {
        std::vector<UtilityAceForecast::Storm> storms;
        int ensembleSize{0};
        std::string cycle;
        std::string error;
    };
    std::shared_ptr<Loaded> loaded;
    std::vector<size_t> order;       // the list's rows: indexes into loaded->storms
    QComboBox * stormBox{};
    QLabel * status{};
    QLabel * credit{};
    AceBoard * board{};
    bool closed{false};
    int generation{0};
};

#endif  // ACEFORECASTVIEWER_H
