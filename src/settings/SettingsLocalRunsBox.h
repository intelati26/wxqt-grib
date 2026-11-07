// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef SETTINGSLOCALRUNSBOX_H
#define SETTINGSLOCALRUNSBOX_H

#include <string>
#include <vector>
#include <QWidget>

class QLabel;
class QLineEdit;

// The "Local runs" tab of the settings: the folders where the Model Viewer looks for the runs of the models that are made on this computer (ArWen, WOOF-NAM: WRF-format history files). Each model has the
// folders it uses (`;` between them, globs allowed), what was found in them, a Browse button to add a folder and a button for the places the program looks in by itself.
class SettingsLocalRunsBox : public QWidget {
public:
    explicit SettingsLocalRunsBox(QWidget * parent = nullptr);

private:
    struct Row {
        std::string model;        // "ARWEN"
        std::string setting;      // the preference that holds the folders
        QLineEdit * folders{};
        QLabel * found{};
    };
    void addRow(class QVBoxLayout * layout, const std::string& model, const std::string& setting);
    void look(Row&);
    std::vector<Row> rows;
};

#endif  // SETTINGSLOCALRUNSBOX_H
