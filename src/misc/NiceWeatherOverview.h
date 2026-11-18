// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef NICEWEATHEROVERVIEW_H
#define NICEWEATHEROVERVIEW_H

#include <memory>
#include <vector>
#include "misc/NiceWeatherCache.h"
#include <QLabel>
#include "ui/Image.h"
#include "ui/ScrolledWindow.h"
#include "ui/VBox.h"
#include "ui/Window.h"

// The nice weather outlook, days 1 to 8 on one page (as the SPC summary has its days): a small map of each day with how much of the country is Nice or better; a click opens the day.
class NiceWeatherOverview : public Window {
public:
    explicit NiceWeatherOverview(Window *);

private:
    void resizeEventCustom() override;
    void colourDays();   // each day's box as the SPC summary has it: the highest level of the day on its colour; a day with dangerous gross weather says so instead
    void load(int day);   // 0..7: one day after another, so the data are fetched once and the machine is not asked for eight maps at a time
    std::vector<QLabel *> labels;
    std::vector<std::shared_ptr<const class NiceWeatherCache::Day>> made;   // each day as it was shown, for the best day
    std::vector<Image> images;
    VBox box;
    ScrolledWindow sw;
    int generation{0};
};

#endif  // NICEWEATHEROVERVIEW_H
