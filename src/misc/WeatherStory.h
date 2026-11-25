// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef WEATHERSTORY_H
#define WEATHERSTORY_H

#include <string>
#include <vector>
#include <QByteArray>
#include <QDateTime>
#include <QLabel>
#include <QPushButton>
#include "ui/VBox.h"
#include "ui/Window.h"
#include "ui/ZoomImage.h"

using std::string;
using std::vector;

// A forecast office's weather story: the graphics (up to five) the office keeps on weather.gov/<office>/weatherstory. Offices publish them in one of two ways: a list at source/<office>/WxStory/
// WeatherStory.xml (a title, a text, a window of time in effect and the picture of each), or fixed picture files, images/<office>/wxstory/Tab<n>FileL.png, replaced in place (no text; the
// upload time of the file says how new it is). The first is read when the office has it, else the second.
namespace WeatherStory {
    struct Story {
        string title;
        string description;
        string imageUrl;
        QDateTime start, end;    // the window in effect (invalid for the picture files)
        QDateTime updated;       // the picture's upload time (Last-Modified), invalid when the server sent none
    };
    // the stories in effect now, the newest first (by the time each picture was uploaded, else by the start of its window); empty when the office has none; blocking
    vector<Story> active(const string& wfo);
}

// The stories of the current location's office: a row of previews of every one in effect (the newest first, shown large), the text and times of the chosen one, and the buttons for the office's own
// page and for its decision support packet.
class WeatherStoryViewer : public Window {
public:
    explicit WeatherStoryViewer(Window * parent);

private:
    void choose(size_t index);
    void closeEventCustom() override { closed = true; }
    VBox box;
    bool closed{false};
    string wfo;
    vector<WeatherStory::Story> stories;
    vector<QByteArray> pictures;          // each story's picture once it has arrived
    vector<QPushButton *> previews;
    QWidget * strip{};
    QLabel * text{};
    QLabel * note{};
    ZoomImage image;
};

#endif  // WEATHERSTORY_H
