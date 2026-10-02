// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef CLIMATEHISTORYVIEWER_H
#define CLIMATEHISTORYVIEWER_H

#include <string>
#include <QByteArray>
#include <QDateEdit>
#include "climate/UtilityClimate.h"
#include "objects/UrlAnimation.h"
#include "ui/Button.h"
#include "ui/ComboBox.h"
#include "ui/HBox.h"
#include "ui/VBox.h"
#include "ui/Window.h"
#include "ui/ZoomImage.h"

using std::string;

// A picture with a history, as a loop and a date picker: the daily global sea surface temperature, anomaly, HotSpots, Degree Heating
// Weeks and bleaching-alert maps (NOAA Coral Reef Watch, back to 2020), and NHC's 14-day Atlantic / Pacific SST loops. Pick the
// last day, the number of frames and the spacing (daily, weekly, every 30 days), then play, scrub or save the loop.
class ClimateHistoryViewer : public Window {
public:
    ClimateHistoryViewer(Window * parent, const string& productKey);

private:
    void closeEventCustom() override { closed = true; }
    void changed();
    void reload();
    void showLatest(const QByteArray&);
    string endDate() const;
    VBox box;
    HBox row;
    ZoomImage image;
    ComboBox comboProduct;
    ComboBox comboCount;
    ComboBox comboStep;
    QDateEdit * dateEdit;
    Button buttonLatest;
    UrlAnimation objectAnimate;
    bool dated{true};
    bool closed{false};
};

#endif  // CLIMATEHISTORYVIEWER_H
