// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef GLOBEAREAPICKER_H
#define GLOBEAREAPICKER_H

#include <functional>
#include <string>
#include <QDialog>

// MAG's area picker for the globe: the names of the model areas in a grid; the pointer over a name shows the area on the globe (a rectangle with its name, the globe turning to it), a click
// chooses it. The pointer over the globe itself does the same with the area under it.
class GlobeAreaPicker : public QDialog {
public:
    explicit GlobeAreaPicker(QWidget * parent);
    std::function<void(const std::string&)> onHover;   // the id of the area under the pointer; empty when it left the names
    std::function<void(const std::string&)> onPick;
};

#endif  // GLOBEAREAPICKER_H
