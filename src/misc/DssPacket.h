// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef DSSPACKET_H
#define DSSPACKET_H

#include <string>
#include <QDateTime>
#include <QLabel>
#include <QPushButton>
#include "ui/VBox.h"
#include "ui/Window.h"

using std::string;

// The local forecast office's decision support (briefing) packet for the current location: the PDF the office keeps at weather.gov/media/<office>/ under a name of its own choosing. The office
// publishes no schedule, so the screen says what the office's status file says (in effect or expired, until when, a line about it) and what the web server says: when the packet was last updated
// (uploaded), and whether there is a new file since the last time it was looked at.
// The address that works for an office, the upload time seen and the time of the look are kept in the preferences (DSS_URL_, DSS_LM_, DSS_CHK_ + the office).
class DssPacket : public Window {
public:
    explicit DssPacket(Window * parent);

    struct Result {
        bool found{false};
        string url;
        QDateTime uploaded;    // the file's Last-Modified (invalid when the server sent none)
        QDateTime previousUploaded, previousChecked;   // what the last look saw (invalid on a first look)
        bool changed{false};   // a newer file than at the last look
        string problem;
        // the office's status file (source/<office>/dsspacket.xml): the window the packet is in effect, its one line summary and when the office last changed it
        enum class Feed { None, Active, Expired } feed{Feed::None};
        QDateTime start, end, feedUpdated;
        string description;
    };
    // looks at the office's packet, updates the kept values; blocking
    static Result check(const string& wfo);
    static void readFeed(const string& wfo, Result& r);   // fills the feed fields (Feed::None when the office has no status file)
    // the packet names: the ones known for an office, then the common ones
    static std::vector<string> candidates(const string& wfo);

private:
    void look();
    void closeEventCustom() override { closed = true; }
    VBox box;
    bool closed{false};
    string wfo;
    string url;
    QLabel * status{};
    QLabel * note{};
    QPushButton * open{};
    QPushButton * again{};
};

#endif  // DSSPACKET_H
