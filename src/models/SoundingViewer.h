// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef SOUNDINGVIEWER_H
#define SOUNDINGVIEWER_H

#include <memory>
#include <string>
#include <vector>
#include <QDateTime>
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
// with (model mode), or an SPC observed balloon sounding picked by site and time
// (observed mode): Skew-T log-p with the lifted parcel and wind barbs, a hodograph, and the
// SHARPpy-style parameter table.
class SoundingViewer : public Window {
public:
    SoundingViewer(Window * parent, double lon, double lat, const string& runId, const string& forecastHour);
    // model sounding for a valid time: the latest synoptic RRFS run and the lead hour nearest `validUtc`
    // (for products on another model's clock, e.g. SPC Post); says so on screen if RRFS does not reach that time
    SoundingViewer(Window * parent, double lon, double lat, const QDateTime& validUtc);
    // SPC observed sounding; `site` is a sounding-site code ("OUN"), empty = the site nearest the current location
    SoundingViewer(Window * parent, const string& site);

private:
    void build();
    void start();
    void startObserved();
    void onSave();
    void closeEventCustom() override;

    double lon;
    double lat;
    string runId;
    string forecastHour;
    QDateTime wantedValid;   // set: choose the lead hour from this valid time instead of `forecastHour`
    string status;   // same "RRFS <date> <cycle>z    F<hh> valid ..." form the map screens use, for names and headers

    VBox box;
    HBox rowTop;
    Text textInfo;
    ComboBox comboSite;
    ComboBox comboTime;
    ComboBox comboArea;   // model mode: the point itself or the mean over a radius around it
    ComboBox comboParcel;
    ComboBox comboLayout;   // SPC's fixed layout, or the layout that fills the window
    QPushButton * buttonSave;
    SoundingCanvas * canvas;

    bool observed{false};
    std::vector<string> timeCodes;   // yyMMddHH per comboTime entry, "" = latest
    QDateTime observedTime;          // valid time of the loaded observed sounding
    int generation{0};
    SoundingProfile profile;
    SoundingAnalysis analysis;
    bool loaded{false};
    bool closed{false};
};

#endif  // SOUNDINGVIEWER_H
