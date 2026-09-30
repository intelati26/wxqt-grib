// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef SOUNDINGVIEWER_H
#define SOUNDINGVIEWER_H

#include <string>
#include <QPushButton>
#include "sounding/SoundingAnalysis.h"
#include "sounding/SoundingProfile.h"
#include "ui/ComboBox.h"
#include "ui/HBox.h"
#include "ui/Text.h"
#include "ui/VBox.h"
#include "ui/Window.h"

using std::string;

class SoundingCanvas;

// A model sounding for one point, locked to the run and forecast hour it was opened
// with: Skew-T log-p with the lifted parcel and wind barbs, a hodograph, and the
// SHARPpy-style parameter table.
class SoundingViewer : public Window {
public:
    SoundingViewer(Window * parent, double lon, double lat, const string& runId, const string& forecastHour);

private:
    void start();
    void onSave();
    void closeEventCustom() override;

    double lon;
    double lat;
    string runId;
    string forecastHour;
    string status;   // same "RRFS <date> <cycle>z    F<hh> valid ..." form the map screens use, for names and headers

    VBox box;
    HBox rowTop;
    Text textInfo;
    ComboBox comboParcel;
    QPushButton * buttonSave;
    SoundingCanvas * canvas;

    SoundingProfile profile;
    SoundingAnalysis analysis;
    bool loaded{false};
    bool closed{false};
};

#endif  // SOUNDINGVIEWER_H
