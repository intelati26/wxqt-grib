// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef RADARHISTORY_H
#define RADARHISTORY_H

#include <functional>
#include <vector>
#include <QDateTime>
#include <QDateTimeEdit>
#include "ui/Button.h"
#include "ui/HBox.h"
#include "ui/Text.h"
#include "ui/VBox.h"
#include "ui/Window.h"

using std::function;
using std::vector;

// Radar history: pick a past time (UTC) and the radar window shows the scan at or before it, from Unidata's S3 archive of NEXRAD
// Level III files (back to early 2022 for the super-resolution products). Previous / next scan step through the radar's scans;
// "Back to live" returns to the newest picture. A loop started while a time is shown ends at that time.
class RadarHistory : public Window {
public:
    RadarHistory(Window * parent,
                 const function<void(const QDateTime&)>& apply,
                 const function<vector<QDateTime>(const QDateTime& from, const QDateTime& to)>& scans,
                 const function<QDateTime()>& shown);
    void refreshStatus();   // the radar window changed what it shows (a radar or product switch)

private:
    void closeEventCustom() override { closed = true; }
    void showTime();
    void step(int direction);
    void live();
    void setEdit(const QDateTime&);
    QDateTime editTime() const;
    VBox box;
    HBox rowTime;
    HBox rowButtons;
    Text textNote;
    Text textStatus;
    QDateTimeEdit * edit;
    Button buttonShow;
    Button buttonDayBack;
    Button buttonDayForward;
    Button buttonPrevious;
    Button buttonNext;
    Button buttonLive;
    function<void(const QDateTime&)> apply;
    function<vector<QDateTime>(const QDateTime&, const QDateTime&)> scans;
    function<QDateTime()> shown;
    bool closed{false};
};

#endif  // RADARHISTORY_H
