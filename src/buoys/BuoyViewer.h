// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef BUOYVIEWER_H
#define BUOYVIEWER_H

#include <memory>
#include <string>
#include <vector>
#include <QWidget>
#include "buoys/BuoyData.h"
#include "ui/ComboBox.h"
#include "ui/Text.h"
#include "ui/VBox.h"
#include "ui/Window.h"

// One NDBC station: the latest observation in words and its recent history as strips against time (wave height and period, wind speed and gust,
// pressure, air and water temperature), for the last day, three days, five days or the whole 45 days the file holds.
class BuoyChart : public QWidget {
public:
    explicit BuoyChart(QWidget * parent = nullptr) : QWidget{parent} { setMinimumSize(680, 520); }
    void setData(const std::shared_ptr<std::vector<UtilityBuoys::Obs>>& series, double hours);

private:
    void paintEvent(QPaintEvent *) override;
    std::shared_ptr<std::vector<UtilityBuoys::Obs>> series;
    double hours{120.0};
};

class BuoyViewer : public Window {
public:
    BuoyViewer(Window * parent, const BuoyData::Marker& marker);
    static QString summary(const BuoyData::Marker& marker);           // the latest observation as sentences, US units
    static QString windText(const UtilityBuoys::Obs& obs);

private:
    void showRange();
    VBox box;
    ComboBox comboRange;
    Text textStatus;
    BuoyChart * chart{};
    std::shared_ptr<std::vector<UtilityBuoys::Obs>> series;
    bool closed{false};
    void closeEventCustom() override { closed = true; }
};

#endif  // BUOYVIEWER_H
