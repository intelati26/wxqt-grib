// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef CHECKSPICKER_H
#define CHECKSPICKER_H

#include <functional>
#include <string>
#include <vector>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QLabel>
#include <QPushButton>
#include "gfs/GfsChart.h"
#include "models/ProductPicker.h"

// The chart picker as checks (docs/plans.md section 9): your presets on top, then the chart (one list of its kinds, grouped by category), the value that kind varies by (a level, an hour,
// a limit: the numbers of the chart's id, read through ChartAlias), and the lines and wind barbs ticked onto it. Every choice takes effect at once (onPick, onOverlays), so the picture
// shows what the rows say. Beside the Charts... picker, which stays.
class ChecksPicker : public QDialog {
public:
    ChecksPicker(QWidget * parent, const std::string& model, const std::vector<ProductPicker::Entry>& entries, const std::string& current, const std::vector<GfsChart::OverlayChoice>& choices,
                 const std::vector<std::string>& ticked);
    std::function<void(const std::string&)> onPick;
    std::function<void(const std::vector<std::string>&)> onOverlays;
    const std::string& modelId() const { return model; }
    // the chart and lines now on show (when something else changed them): the rows follow, nothing is called
    void setState(const std::string& chart, const std::vector<std::string>& ticked);
    // the kinds of chart the entries make: the entries of each, in the order of the list; the kind's name (its label, its numbers written '#'), its category, and the text of each value
    struct Kind {
        std::string name;
        std::string group;
        std::vector<ProductPicker::Entry> charts;
        std::vector<std::string> values;   // one per chart: "500mb", "6 hours" ("" when the kind has one chart)
    };
    static std::vector<Kind> kinds(const std::vector<ProductPicker::Entry>& entries);

private:
    void fillKinds();
    void fillValues(size_t kind, const std::string& select);
    void fillPresets();
    void pickKind(int row);
    void pickValue(int row);
    void ticks();
    void savePreset();
    void usePreset(const std::string& name);
    std::string model;
    std::vector<Kind> kindList;
    std::vector<GfsChart::OverlayChoice> choices;
    std::string chart;
    bool quiet{false};
    QWidget * presetRow{};
    QComboBox * kindBox{};
    QComboBox * valueBox{};
    QLabel * valueLabel{};
    QLabel * note{};
    std::vector<QCheckBox *> boxes;
};

#endif  // CHECKSPICKER_H
