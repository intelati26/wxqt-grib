// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef CHARTBUILDER_H
#define CHARTBUILDER_H

#include <functional>
#include <string>
#include <vector>
#include <QComboBox>
#include <QDialog>
#include <QLabel>
#include "gfs/GfsChart.h"

// "Build a chart": a map made to order from a template of the model (the chance of rain over a limit in a period, of a temperature under a limit, of gusts over one). The choices are boxes of the
// values the template offers; Show draws the chart, which then is in the chart list like the rest.
class ChartBuilder : public QDialog {
public:
    ChartBuilder(QWidget * parent, const std::string& model, const std::vector<GfsChart::Template>& templates);
    std::function<void(const std::string& chartId)> onBuilt;

private:
    void rebuildSettings();
    std::string chosenId() const;
    void updateText();
    std::string model;
    std::vector<GfsChart::Template> templates;
    QComboBox * which{};
    QWidget * settingsArea{};
    std::vector<QComboBox *> boxes;
    QLabel * summary{};
};

#endif  // CHARTBUILDER_H
