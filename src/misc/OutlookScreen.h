// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef OUTLOOKSCREEN_H
#define OUTLOOKSCREEN_H

#include <map>
#include <QComboBox>
#include <QStackedWidget>
#include "misc/OutlookNav.h"
#include "ui/VBox.h"
#include "ui/Window.h"

// The one screen of the forecast outlooks: the SPC convective outlooks (summary, each day, state graphics), the fire weather outlooks, the thunderstorm outlooks and the WPC excessive
// rainfall outlooks, chosen in the box at the top and shown in one window, as the model screen does for the models. Each is a page made the first time it is chosen and kept while
// the window is open. The products that have windows of their own (storm reports, mesoanalysis, ...) are in the box too, and open as before.
class OutlookScreen : public Window {
public:
    OutlookScreen(Window * parent, OutlookNav::Kind kind, int value = 0);
    // shows that page in the outlook window `from` is in, or in a new one
    static void open(Window * from, OutlookNav::Kind kind, int value = 0);
    void showPage(OutlookNav::Kind kind, int value);

private:
    Window * make(OutlookNav::Kind kind, int value);
    void closeEventCustom() override;
    VBox box;
    QComboBox * combo{};
    QStackedWidget * stack{};
    std::map<int, Window *> pages;   // by kind and day
    int shown{-1};
};

#endif  // OUTLOOKSCREEN_H
