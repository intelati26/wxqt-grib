// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef RECONRENDERER_H
#define RECONRENDERER_H

#include <functional>
#include <string>
#include <vector>
#include <QColor>
#include <QComboBox>
#include <QPainter>
#include <QPointF>
#include "hurricane/UtilityDropsonde.h"
#include "hurricane/UtilityHdob.h"
#include "hurricane/UtilityVdm.h"

// How reconnaissance is drawn on any map that has a storm's track and guidance: the flight tracks coloured by the wind (the flight level or the SFMR surface wind) with wind barbs, the centre
// fixes of the vortex messages and the dropsondes. One place, so that the track map, the one-flight page and the master map draw it alike, and one choice, "show the last X hours", to keep
// it from burying the track and the guidance: only what was observed in the hours before the newest observation is drawn. (The newest, not the clock: a storm that is over keeps its last day.)
namespace ReconRenderer {
    struct Data {
        const std::vector<UtilityHdob::Message> * flights{nullptr};
        const std::vector<UtilityVdm::Vdm> * fixes{nullptr};
        const std::vector<UtilityDropsonde::Drop> * drops{nullptr};
    };
    struct Settings {
        int hours{6};               // the last X hours before the newest observation; 0 all
        bool sfmr{false};           // the tracks coloured by the SFMR surface wind, not the flight-level wind
        bool barbs{true};
        bool labels{true};          // the pressure and wind beside a dropsonde, where there is room
    };
    using Project = std::function<QPointF(double lat, double lon)>;
    using Accept = std::function<bool(double lat, double lon)>;   // false: not this storm's (far from it)

    // the time the newest observation of any of the three was made, 0 when there is none
    long newest(const Data& data);
    // the oldest second to draw for these settings (0: everything)
    long cutoff(const Data& data, int hours);
    bool recent(long seconds, long cutoff);

    QColor windColor(double knots);   // the colour of a hurricane category, of the wind in knots

    // `px` is the size of a screen pixel in the units the painter works in; `accept` may be empty
    void paintFlights(QPainter&, const Project&, double px, const Data&, const Settings&, const Accept& accept = {});
    void paintFixes(QPainter&, const Project&, double px, const Data&, const Settings&, const Accept& accept = {});
    void paintDrops(QPainter&, const Project&, double px, const Data&, const Settings&, const Accept& accept = {});

    // the choices of "show the last X hours"; saved under the preference name
    const std::vector<std::pair<const char *, int>>& hourChoices();
    int savedHours(const std::string& pref, int standard = 6);
    QComboBox * hoursCombo(QWidget * parent, const std::string& pref, const std::function<void(int hours)>& changed, int standard = 6);
    // how many HDOB bulletins to read to have the hours (the archive has a bulletin of about half an hour for each aircraft)
    int bulletinsFor(int hours);
}

#endif  // RECONRENDERER_H
