// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef HELPVIEWER_H
#define HELPVIEWER_H

#include "ui/VBox.h"
#include "ui/Window.h"

class QTextBrowser;
class QPushButton;
class QTreeWidget;

// Help and further reading: for each model the Model Viewer draws, why it matters, what the app shows of it and where its data and its documentation are (a Wikipedia article
// only where there is one about the model itself), and how to add a model of one's own. The words of the built-in models are Markdown pages (help/models/<id>.md, bundled as resources); a
// model added by file carries them in its `why` and `link` lines, so it is in here too.
class HelpViewer : public Window {
public:
    // `topic` opens a page at once: "guide:editing-rules", "guide:server-file", "guide:chart-files", "guide:model-files", "add", "sources" or "model:<id>"
    explicit HelpViewer(QWidget *, const std::string& topic = {});
    // A "Help..." button for an editor: it opens this page of the Help window (the rule of docs/help-coverage.md: where a thing is edited, the page about it is one click away).
    static QPushButton * button(QWidget * parent, const std::string& topic, const QString& tip = {});

private:
    void showTopic(const std::string& topic);
    VBox box;
    QTreeWidget * tree{};
    QTextBrowser * page{};
};

#endif  // HELPVIEWER_H
