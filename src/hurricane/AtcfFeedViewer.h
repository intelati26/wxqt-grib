// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef ATCFFEEDVIEWER_H
#define ATCFFEEDVIEWER_H

#include <set>
#include <string>
#include <vector>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QTableWidget>
#include <QTabWidget>
#include <QTimer>
#include "ui/Window.h"
#include "ui/VBox.h"

using std::string;
using std::vector;

// The raw ATCF feed of a storm: the last lines of its deck, as the file has them (the newest at the bottom) and decoded in columns. The best track (b-deck: the analyses, every 6 hours, with the wind
// radii), the guidance (a-deck: every model's forecast of every cycle) and the fixes (f-deck: the satellite and aircraft fixes) of the NHC's basins come from the NHC's ATCF directory; the
// best track of the other basins (JTWC's western Pacific, Indian Ocean and Southern Hemisphere) from UCAR's research data repository. The file is read again every few minutes; the lines that
// came since the last look are marked. id is the ATCF one in lower case ("wp292026", "al092026").
class AtcfFeedViewer : public Window {
public:
    AtcfFeedViewer(Window * parent, const string& id, const string& title);

private:
    void load();
    void showLines(const string& text);
    void makeTimeline(const string& text);
    void findInvest(const string& text);
    void closeEventCustom() override { closed = true; }
    string stormId;
    QLabel * status{};
    QComboBox * deck{};
    QComboBox * view{};      // all lines / only where the status changes / only where the category changes (best track)
    QLabel * timeline{};     // AL93 -> AL09 NINE (TD) -> ISAIAS (TS) ...
    QComboBox * count{};
    QLineEdit * filter{};
    QCheckBox * refresh{};
    QTabWidget * tabs{};
    QTableWidget * table{};
    QPlainTextEdit * raw{};
    QTimer timer;
    std::set<string> seen;     // the lines already looked at
    std::set<string> fresh;    // the ones that came with the last reading (marked)
    bool firstLook{true};
    string lastText;
    string investNote;       // the invest number the storm came from, when the decks say
    string timelineBase;     // the events of the storm without the invest
    int generation{0};
    bool closed{false};
    VBox box;
};

#endif  // ATCFFEEDVIEWER_H
