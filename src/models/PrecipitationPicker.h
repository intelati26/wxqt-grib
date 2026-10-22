// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef PRECIPITATIONPICKER_H
#define PRECIPITATIONPICKER_H

#include <functional>
#include <string>
#include <vector>
#include <QComboBox>
#include <QDialog>
#include <QLabel>
#include <QPushButton>

// The precipitation charts of a model as parts instead of a list of sixteen: what (the amount over a period, the amount since the start, the type, the rate, snowfall, precipitable water),
// the period (1 hour to 60 hours) and the lines that go with it (the sea level pressure with a thickness, the 850 mb temperature, the 10 m wind). The three choices make one chart that the model
// has: a part that no chart of the model goes with is not offered. "Standard" is the usual chart (the amount over 6 hours). `ids` are the charts the model has.
class PrecipitationPicker : public QDialog {
public:
    PrecipitationPicker(QWidget * parent, const std::vector<std::string>& ids, const std::vector<std::string>& labels, const std::string& current);
    std::function<void(const std::string&)> onPick;
    // the chart ids this picker can make, so that the chart picker knows whether to offer it
    static bool offeredFor(const std::vector<std::string>& ids);

private:
    struct Row {
        int what;            // index into whats()
        int period;          // hours, 0: not a choice
        int lines;           // index into lineChoices(), 0: none
        std::string id;
    };
    void rebuild();         // the choices of the parts from what is chosen so far
    void refreshResult();   // the chart that the choices make, and its name
    std::string chosenId() const;
    std::vector<std::string> ids, labels;
    std::vector<Row> rows;
    QComboBox * whatBox{};
    QComboBox * periodBox{};
    QComboBox * linesBox{};
    QLabel * result{};
    QPushButton * standard{};
    QPushButton * use{};
    bool building{false};
};

#endif  // PRECIPITATIONPICKER_H
