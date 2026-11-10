// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef MODELBAR_H
#define MODELBAR_H

#include <functional>
#include <string>
#include <vector>
#include <QString>
#include "models/ProductPicker.h"

class QWidget;

// The pieces of the model bar that are the same wherever it is: the Model Viewer's rows and the master map's model layer (the bar across its top and the layer's panel) each put these on
// screen with their own layout, but what the model list holds, what the change box offers and what Colors... does are written once, here.
namespace ModelBar {
    // the list of the models for a picker: each with what it is, grouped (deterministic by size, ensembles, waves ...) and the main models first within a group
    std::vector<ProductPicker::Entry> modelEntries(const std::vector<std::string>& ids);

    // what the change box offers, in order: the plain chart, the change since the run 6, 12 and 24 hours older, the largest of the 24 hours ending at the hour. `short` is the Model Viewer's words
    std::vector<std::string> changeLabels(bool shortWords);
    // the item of the box for a selection's change and maximum (an unusual number of hours, as the map hands over, has no item: -1), and the other way
    int changeIndex(int changeBack, bool maxDay);
    void changeFromIndex(int index, int& changeBack, bool& maxDay);

    // Colors...: the color editor of the chart, or why there is none (the reason goes to `say`). `changed` is called when the colors have been changed
    void editColors(QWidget * parent, const std::string& model, const std::string& param, int changeBack, const std::function<void(const QString&)>& say, const std::function<void()>& changed);
}

#endif  // MODELBAR_H
