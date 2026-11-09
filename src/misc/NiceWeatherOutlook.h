// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef NICEWEATHEROUTLOOK_H
#define NICEWEATHEROUTLOOK_H

#include <algorithm>
#include <string>
#include "gfs/GfsGrid.h"
#include "ui/ComboBox.h"
#include "ui/Image.h"
#include "ui/Shortcut.h"
#include "ui/Text.h"
#include "ui/TwoWidgetScroll.h"
#include "ui/VBox.h"
#include "ui/Window.h"

// The nice weather outlook (a work in progress) as the SPC outlook pages are laid out: the day to show, the map of that day (the National Blend of Models' daytime weather in five levels, Decent
// to Perfect) and beside it the summary of the grid's coloring: how much of the contiguous United States is at each level, and how the regions compare.
class NiceWeatherOutlook : public Window {
public:
    NiceWeatherOutlook(Window *, int);
    static std::string summaryOf(const GfsGrid::Grid& grid);   // the summary text of a grid of levels (what the page shows beside the map)
    void selectDay(int day) { if (comboDay.getIndex() != std::clamp(day, 0, 6)) { comboDay.setIndex(static_cast<size_t>(std::clamp(day, 0, 6))); showDay(); } }   // 0 day 1 .. 6 day 7

private:
    void showDay();
    VBox leftBox;
    ComboBox comboDay;
    Text header;
    Image photo;
    Text text;
    TwoWidgetScroll sw;
    Shortcut shortcutImage;
    int generation{0};
};

#endif  // NICEWEATHEROUTLOOK_H
