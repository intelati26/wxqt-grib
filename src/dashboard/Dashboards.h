// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef DASHBOARDS_H
#define DASHBOARDS_H

class Window;

// Placeholder dashboards (see PlaceholderDashboard): planned screens with a toolbar entry so the layout can be judged before they
// are built. The plans behind them are the notes in the wxqt-grib-docs repository.
namespace Dashboards {
    void openTropicalHub(Window * parent);
    void openSpaceWeather(Window * parent);
    void openTornadoHistory(Window * parent);
    void openForecastDiscussions(Window * parent);
}

#endif  // DASHBOARDS_H
