// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef HELPVIEWER_H
#define HELPVIEWER_H

#include "ui/VBox.h"
#include "ui/Window.h"

class QTextBrowser;
class QTreeWidget;

// Help and further reading: for each model the Model Viewer draws, why it matters, what the app shows of it and where its data and its documentation are (a Wikipedia article
// only where there is one about the model itself), and how to add a model of one's own. The words come with the models (GfsAboutModels.cpp, or the `why` and `link` lines of a
// model file), so a model added by file is in here too.
class HelpViewer : public Window {
public:
    explicit HelpViewer(Window *);

private:
    void showTopic(const std::string& topic);
    VBox box;
    QTreeWidget * tree{};
    QTextBrowser * page{};
};

#endif  // HELPVIEWER_H
