// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef MRMSVERIFYVIEWER_H
#define MRMSVERIFYVIEWER_H

#include <memory>
#include <string>
#include <vector>
#include <QCheckBox>
#include <QComboBox>
#include <QDateTimeEdit>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include "ui/Window.h"

// A model's reflectivity chart against MRMS at a valid time on the hour: every run of the model that has a forecast for that time (each lead time) is scored against the MRMS
// scan nearest the time, or against the highest MRMS value of the hour that ends at it (for the model fields that are the maximum of an hour). The table has
// POD, FAR, CSI and bias at a threshold and the Fractions Skill Score; the row picked is drawn as the model, MRMS and where they agree (gfs/MrmsVerify).
class MrmsVerifyViewer : public Window {
public:
    // `validUtc`: the time to start with (the screen's own valid time)
    MrmsVerifyViewer(Window * parent, const std::string& model, const std::string& param, const std::string& sector, const QDateTime& validUtc);

private:
    struct Row;
    void compare();
    void showRow();
    void fillTable();
    void closeEventCustom() override { closed = true; }

    std::string model, param, sector;
    QComboBox * modelPick{};
    QComboBox * runsPick{};
    QDateTimeEdit * timePick{};
    QCheckBox * hourMax{};
    QComboBox * thresholdPick{};
    QPushButton * go{};
    QLabel * status{};
    QTableWidget * table{};
    QLabel * picture{};
    std::vector<std::shared_ptr<Row>> rows;
    std::shared_ptr<std::vector<std::shared_ptr<Row>>> pending;
    QString observedNote;
    int generation{0};
    bool closed{false};
};

#endif  // MRMSVERIFYVIEWER_H
