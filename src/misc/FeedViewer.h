// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef FEEDVIEWER_H
#define FEEDVIEWER_H

#include <string>
#include <vector>
#include <QByteArray>
#include <QDateTime>
#include <QComboBox>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include "ui/VBox.h"
#include "ui/Window.h"
#include "ui/ZoomImage.h"

using std::string;
using std::vector;

// A viewer of an RSS feed whose items are a picture and a note (a "chart of the day"), and the thumbnail of its newest item for the home screen: an advertisement of sorts for someone
// else's interesting data, always credited and always linked to the place it comes from (the picture is theirs, shown as it is). The first feed is the Iowa Environmental Mesonet's daily
// feature (Iowa State University; Daryl Herzmann): a chart of the day with a paragraph about it and a link that makes the same chart on their site.
class FeedViewer : public Window {
public:
    struct Source {
        string id;       // "iem-feature"
        string title;    // the window's name
        string url;      // the feed
        string credit;   // under the picture
        string site;     // the place the items come from
    };
    struct Item {
        QString title;
        QString date;           // "Wed 18 Nov 2026" (the feed's own date, or the one in the item's link)
        QString link;           // the item on the site
        QString imageUrl;
        QString text;           // the note, as plain text
        QString extraLink;      // another link in the note ("Generate This Chart on IEM Website")
        QString extraLabel;
    };
    static const Source& iem();
    // When the feed is asked for again (the user, 2026-11-18: "check for changes only if a workday 9am hasn't updated in 24 hours"): never read before, or a workday (Monday to Friday, local
    // time), 9 in the morning or later, with the last check at least 24 hours ago. Otherwise the copy kept from the last check is used with no network at all: a feature that is posted once
    // a weekday is not worth more. A check that failed is tried again after half an hour, not at every look. `lastCheck`, `lastAttempt`: seconds since 1970 (0: never).
    static bool dueToCheck(long long lastCheck, long long lastAttempt, const QDateTime& now);
    // blocking, off the UI thread: the feed's items, newest first, from the copy kept on disk unless the feed is due to be asked for (empty when it cannot be read at all)
    static vector<Item> read(const Source& source);
    // the items of a feed's text (for the tests)
    static vector<Item> parse(const QByteArray& xml);
    // blocking: the newest item and its picture, for the home screen (the feed is kept 30 minutes); false when either could not be had. The title of the last one had is `lastTitle()`.
    static bool newest(const Source& source, Item& item, QByteArray& image);
    static string lastTitle();

    FeedViewer(Window * parent, const Source& source);

private:
    void show(int index);
    void closeEventCustom() override { closed = true; }
    Source source;
    VBox box;
    QComboBox * items{};
    QLabel * note{};
    QPushButton * openItem{}, * openChart{};
    QPlainTextEdit * text{};
    ZoomImage image;
    vector<Item> list;
    int generation{0};
    bool closed{false};
};

#endif  // FEEDVIEWER_H
