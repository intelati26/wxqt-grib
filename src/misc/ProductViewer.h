// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef PRODUCTVIEWER_H
#define PRODUCTVIEWER_H

#include <string>
#include <QLabel>
#include <QPlainTextEdit>
#include "ui/VBox.h"
#include "ui/Window.h"
#include "ui/ZoomImage.h"

using std::string;

// A text product with its picture: the picture the office or centre publishes with it (a mesoscale discussion's map, a watch's radar and outline) beside the text, which is reflowed and wrapped to the
// window like the other text screens. `text` is the product's own text, or, when it is empty, `textUrl` is read (a page of HTML has its preformatted text taken out of it).
class ProductViewer : public Window {
public:
    ProductViewer(Window * parent, const string& title, const string& imageUrl, const string& text, const string& textUrl = {});

private:
    void closeEventCustom() override { closed = true; }
    VBox box;
    QPlainTextEdit * body{};
    QLabel * note{};
    ZoomImage image;
    bool closed{false};
};

#endif  // PRODUCTVIEWER_H
