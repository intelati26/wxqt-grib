// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef MODELSCREEN_H
#define MODELSCREEN_H

#include <map>
#include <string>
#include <QComboBox>
#include <QDateTime>
#include <QStackedWidget>
#include "ui/VBox.h"
#include "ui/Window.h"

using std::string;

// The one model screen: every source of model charts in one window, chosen in the box at the top. The charts drawn here from the models' data (the Model Viewer),
// SPC's REFS store with all its products, SPC's post-processed probabilities, NSSL's CAMs pictures and the picture models that are kept until they are retired
// (SPC HRRR, NSSL WRF, WPC GEFS, SPC HREF). Each source is its own screen as before, made the first time it is chosen and kept while the window is open.
class ModelScreen : public Window {
public:
    // source: one of the ids of sources() ("" the one used last)
    explicit ModelScreen(Window * parent, const string& source = {});
    void showSource(const string& id);
    // the Model Viewer on a model, at the run and hour whose valid time is `validUtc` (the history page)
    void openAt(const string& model, const QDateTime& validUtc);
    struct Source {
        const char * id;
        const char * label;
    };
    static const std::vector<Source>& sources();

private:
    Window * make(const string& id);
    void closeEventCustom() override;
    VBox box;
    QComboBox * combo{};
    QStackedWidget * stack{};
    std::map<string, Window *> pages;
    string shown;
};

#endif  // MODELSCREEN_H
