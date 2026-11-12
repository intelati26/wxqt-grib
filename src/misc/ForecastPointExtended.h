// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef FORECASTPOINTEXTENDED_H
#define FORECASTPOINTEXTENDED_H

#include <functional>
#include <memory>
#include <QDateTime>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QWidget>
#include "misc/NbmText.h"
#include "misc/UtilityForecastPoint.h"

class ExtendedCanvas;
class ExtendedLabels;
class Window;

// The extended forecast of a place, 11 days: graphs and a table from the National Blend of Models text bulletins of its nearest station (the NWS grid for the first hours). Equal width columns,
// although the spacing of the times changes (3 hours, then 12). A click on a column picks a time, and the button then opens the heights and winds of it.
class ForecastPointExtended : public QWidget {
public:
    ForecastPointExtended(Window * owner, QWidget * parent);
    void setPoint(const std::shared_ptr<UtilityForecastPoint::Data>& data);   // a new place: the page is reloaded the next time it is shown
    void showing();                                                            // the page came to the front: loaded now when it is out of date (more than 30 minutes, or another place)
    std::function<void(const QDateTime&)> openSynoptic;                        // the button: the valid time (UTC)

private:
    void load();
    void fill();
    void picked(int column);
    Window * owner;
    QLabel * status;
    ExtendedLabels * labels;
    QScrollArea * scroll;
    ExtendedCanvas * canvas;
    QLabel * pickText;
    QPushButton * pickButton;
    std::shared_ptr<UtilityForecastPoint::Data> data;
    std::shared_ptr<NbmText::Table> table;
    qint64 loadedAt{0};
    int generation{0};
    bool stale{true};
    friend class ExtendedCanvas;
};

#endif  // FORECASTPOINTEXTENDED_H
