// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef SOUNDINGPICK_H
#define SOUNDINGPICK_H

#include <functional>
#include <string>
#include <utility>
#include <QDateTime>
#include "ui/Button.h"
#include "ui/Window.h"

// The "click the map, then press Sounding" control shared by the RRFS map viewers: a Sounding button,
// the picked point, and opening a SoundingViewer for the viewer's current run and forecast hour.
class SoundingPick {
public:
    // runAndHour: the viewer's RRFS run id ("" = latest) and forecast hour, read when the button is pressed;
    // titleBase: the viewer's own title, prefixed to the "point picked" / "pick a point" notes
    SoundingPick(Window * owner, std::function<std::pair<std::string, std::string>()> runAndHour, std::string titleBase);
    // for a viewer on another model's clock: the product's valid time (UTC); the latest synoptic RRFS run and
    // the lead hour nearest to it are chosen when the sounding opens
    SoundingPick(Window * owner, std::function<QDateTime()> validTime, std::string titleBase);
    Button& button() { return buttonSounding; }
    void pick(double lon, double lat);

private:
    void open();
    Window * owner;
    std::function<std::pair<std::string, std::string>()> runAndHour;
    std::function<QDateTime()> validTime;
    std::string titleBase;
    Button buttonSounding;
    bool havePoint{false};
    double lon{0.0};
    double lat{0.0};
};

#endif  // SOUNDINGPICK_H
