// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef CIRALOOPVIEWER_H
#define CIRALOOPVIEWER_H

#include <string>
#include <vector>
#include <QByteArray>
#include "objects/UrlAnimation.h"
#include "ui/ComboBox.h"
#include "ui/HBox.h"
#include "ui/VBox.h"
#include "ui/Window.h"
#include "ui/ZoomImage.h"

using std::string;
using std::vector;

// A loop of one CIRA / RAMMB product for one storm (4 km infrared, 89 GHz microwave): the usual play / scrub / range / save bar.
class CiraLoopViewer : public Window {
public:
    CiraLoopViewer(Window * parent, const string& stormId, const string& stormTitle, const string& product);

private:
    void closeEventCustom() override { closed = true; }
    void reload();
    void showLatest(const QByteArray&);
    string stormId;
    string stormTitle;
    vector<string> productKeys;   // the products worth looping, parallel to comboProduct
    VBox box;
    HBox row;
    ZoomImage image;
    ComboBox comboProduct;
    ComboBox comboCount;
    UrlAnimation objectAnimate;
    bool closed{false};
};

#endif  // CIRALOOPVIEWER_H
