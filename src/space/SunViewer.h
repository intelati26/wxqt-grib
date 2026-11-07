// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef SUNVIEWER_H
#define SUNVIEWER_H

#include <memory>
#include <vector>
#include <QComboBox>
#include <QImage>
#include <QLabel>
#include "space/UtilitySun.h"
#include "ui/VBox.h"
#include "ui/Window.h"

class SunDisc;

// The Sun with its numbered active regions (UtilitySun) in a window of its own: point at a region for its sunspot class, size, magnetic class and flare chances. The picture on the home screen
// has no hover; this is where the detail is.
class SunViewer : public Window {
public:
    explicit SunViewer(Window * parent);
    void choose(int index) { product->setCurrentIndex(index); }   // that picture of the list (the picture is read when the choice changes)

private:
    void load();
    void closeEventCustom() override { closed = true; }
    VBox box;
    SunDisc * disc{};
    QLabel * status{};
    QComboBox * product{};
    QComboBox * resolution{};
    int generation{0};
    bool closed{false};
};

#endif  // SUNVIEWER_H
