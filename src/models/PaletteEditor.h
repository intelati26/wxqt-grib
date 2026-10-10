// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef PALETTEEDITOR_H
#define PALETTEEDITOR_H

#include <functional>
#include <string>
#include <QDialog>
#include "gfs/GfsChart.h"

class QCheckBox;
class QLabel;
class QTableWidget;
class PaletteBar;

// Edits the color scale a kind of model chart is drawn with: the stops (value and color, with transparency), banded or smooth, add / remove, back to the default, and a text file
// to import or export. Applying keeps the scale (PaletteStore) for every chart that uses the same default and calls `changed`.
class PaletteEditor : public QDialog {
public:
    // `original`: the scale the chart is drawn with when the user has none; values are shown in the user's units for `quantity`
    PaletteEditor(QWidget * parent, const std::string& title, const GfsChart::Ramp& original, GfsChart::Quantity quantity, bool us, std::function<void()> changed);

private:
    void fill(const GfsChart::Ramp&);
    GfsChart::Ramp current() const;   // the table as a scale in the grids' units
    void addStop();
    void removeStop();
    void apply();
    void reset();
    void import();
    void exportFile();
    void pickColor(int row);
    void refreshBar();
    void say(const QString&);
    std::string key;
    GfsChart::Ramp original;
    GfsChart::Quantity quantity;
    bool us;
    std::function<void()> changed;
    QTableWidget * table{};
    QCheckBox * banded{};
    QLabel * note{};
    PaletteBar * bar{};
};

#endif  // PALETTEEDITOR_H
