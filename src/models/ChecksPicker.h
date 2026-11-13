// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef CHECKSPICKER_H
#define CHECKSPICKER_H

#include <array>
#include <functional>
#include <string>
#include <map>
#include <vector>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include "gfs/ChartPreset.h"
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
        // a product of hours, types and limits (a chance, an ensemble statistic: ChartAlias's product, window, kind and limit): one per chart, {hour, type, limit}; empty for the other kinds
        std::vector<std::array<std::string, 3>> parts;
    };
    static std::vector<Kind> kinds(const std::vector<ProductPicker::Entry>& entries);

private:
    void fillKinds();   // the Chart list: the favorites, then the kinds that match the search (and always the one on show)
    void toggleFavorite();
    void fillValues(size_t kind, const std::string& select);
    void fillAxes(size_t kind, const std::string& chartId);   // the Hour, Type and Limit lists of a product kind: each offers what exists with the ones chosen before it
    void pickAxis(int axis);                                  // 0 hour, 1 type, 2 limit
    void fillPresets();
    void pickKind(int row);
    void pickValue(int row);
    void ticks();
    void refreshFamilies();   // the buttons' counts and the line that says what is on
    void savePreset();
    void usePreset(const std::string& name);
    void applyPreset(const ChartPreset::Preset& preset);
    std::string model;
    std::vector<Kind> kindList;
    std::vector<GfsChart::OverlayChoice> choices;
    std::string chart;
    bool quiet{false};
    QWidget * presetRow{};
    QComboBox * kindBox{};
    QLineEdit * search{};
    QPushButton * star{};
    std::vector<std::string> favorites;
    std::map<std::string, std::string> labelOf;   // chart id -> its label
    QComboBox * valueBox{};
    QLabel * valueLabel{};
    QWidget * variantBody{};
    QLabel * variantLabel{};
    std::array<QComboBox *, 3> axes{};
    size_t shownKind{0};
    QLabel * note{};
    struct Family {
        QString name;
        std::string group;
        QPushButton * button{};
        std::vector<QCheckBox *> boxes;
    };
    std::vector<Family> families;
    QLabel * summary{};
    std::vector<QCheckBox *> boxes;
};

#endif  // CHECKSPICKER_H
