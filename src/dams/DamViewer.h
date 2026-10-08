// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef DAMVIEWER_H
#define DAMVIEWER_H

#include <memory>
#include <QString>
#include <QWidget>
#include "dams/DamData.h"
#include "ui/ComboBox.h"
#include "ui/Text.h"
#include "ui/VBox.h"
#include "ui/Window.h"

// One Corps of Engineers dam: its latest hourly numbers in words and its history as strips against time (pool elevation, release with the share through the
// turbines, power generated, inflow, tailwater elevation).
class DamChart : public QWidget {
public:
    explicit DamChart(QWidget * parent = nullptr) : QWidget{parent} { setMinimumSize(700, 520); }
    void setData(const std::shared_ptr<DamData::Data>& data, int hours);

private:
    void paintEvent(QPaintEvent *) override;
    std::shared_ptr<DamData::Data> data;
    int hours{168};
};

class DamViewer : public Window {
public:
    DamViewer(Window * parent, const UtilityDams::Project& project);
    static QString summary(const DamData::Latest&);   // "Release 5,205 cfs (4,706 through the turbines), generating 68 MWh, pool 911.4 ft ..."

private:
    void load();
    const UtilityDams::Project * project;
    VBox box;
    ComboBox comboRange;
    Text textStatus;
    DamChart * chart{};
    std::shared_ptr<DamData::Data> data;
    int generation{0};
    bool closed{false};
    void closeEventCustom() override { closed = true; }
};

#endif  // DAMVIEWER_H
