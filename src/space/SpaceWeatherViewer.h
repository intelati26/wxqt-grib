// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef SPACEWEATHERVIEWER_H
#define SPACEWEATHERVIEWER_H

#include <memory>
#include <QLabel>
#include <QVBoxLayout>
#include <QWidget>
#include "space/SpaceData.h"
#include "ui/Button.h"
#include "ui/HBox.h"
#include "ui/Text.h"
#include "ui/VBox.h"
#include "ui/Window.h"

// One of the space weather strips: the planetary K index (bars, observed and forecast), the GOES X-ray flux (log scale with the flare classes), the solar wind
// speed and density, or the interplanetary magnetic field (Bt and Bz).
class SpaceChart : public QWidget {
public:
    enum Kind { Kp, Xray, Wind, Mag, Particles, Cycle };
    SpaceChart(Kind kind, QWidget * parent);
    void setData(const std::shared_ptr<SpaceData::Bundle>& data);

private:
    void paintEvent(QPaintEvent *) override;
    Kind kind;
    std::shared_ptr<SpaceData::Bundle> data;
};

// The space weather screen: the NOAA scales now and for the next days, what the sun and the solar wind are doing, the Kp, X-ray, wind and field charts,
// pictures of the aurora forecast, the sun (SUVI) and the corona (LASCO) that open large, the newest alerts and the SWPC text products.
class SpaceWeatherViewer : public Window {
public:
    explicit SpaceWeatherViewer(Window * parent);

private:
    void load();
    void fill();
    void addPicture(const QString& title, const std::string& url);
    void closeEventCustom() override { closed = true; }
    VBox box;
    HBox row;
    Button buttonRefresh;
    Text textStatus;
    QWidget * content{};
    QVBoxLayout * layout{};
    QLabel * scalesLabel{};
    QLabel * nowLabel{};
    QLabel * alertsLabel{};
    QHBoxLayout * pictures{};
    SpaceChart * charts[6]{};
    std::shared_ptr<SpaceData::Bundle> data;
    std::vector<Button *> textButtons;
    int generation{0};
    bool closed{false};
};

#endif  // SPACEWEATHERVIEWER_H
