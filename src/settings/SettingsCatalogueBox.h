// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef SETTINGSCATALOGUEBOX_H
#define SETTINGSCATALOGUEBOX_H

#include <memory>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QWidget>
#include "gfs/CatalogCheck.h"
#include "gfs/GfsRender.h"

// Settings > Catalogue (section 21 of docs/plans.md): what a model's newest file lists, against what the program's charts ask for. "Check" reads the model's index (no field is downloaded) and fills the
// table: Found (a chart uses it), Gone (was there at the last check and is not now: a rename or a removal), Never found (a chart asks for it and it has never been there), Not cataloged (the model has it
// and no chart draws it). A field of the table becomes a chart of your own with "Make a chart from this field": a starter file in your models folder (the ramp and units chosen from the variable), read at
// once, shown in the Model Viewer under "Your charts". The rules and the format of the file: the Help page "Chart files".
class SettingsCatalogueBox : public QWidget {
public:
    explicit SettingsCatalogueBox(QWidget * parent = nullptr);
    // a model chosen and checked (a development aid), the filter chosen (0 every field, 1 not cataloged, 2 gone, 3 never found, 4 found), the first row selected and a chart made from it when `make`
    void run(const QString& modelId, int filter = 0, bool make = false);
    QString statusText() const { return status->text(); }

private:
    void check();
    void fill();
    void makeChart();
    QComboBox * model{};
    QComboBox * filterBox{};
    QPushButton * checkButton{};
    QPushButton * makeButton{};
    QLabel * status{};
    QTableWidget * table{};
    std::shared_ptr<GfsRender::Session> session;
    CatalogCheck::Report report;
    bool haveReport{false};
    int generation{0};
};

#endif  // SETTINGSCATALOGUEBOX_H
