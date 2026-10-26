// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef ABPGEXTENDED_H
#define ABPGEXTENDED_H

#include <string>
#include "ui/ComboBox.h"
#include "ui/ControlRow.h"
#include "ui/Text.h"
#include "ui/VBox.h"
#include "ui/Window.h"
#include "ui/ZoomImage.h"

using std::string;

// The University of Missouri's extended analog guidance (days 6-14, from the GEFS members): one picture for the whole country per
// product (severe reports, 72 h precipitation, temperature, snow) for the periods days 6-8, 9-11 and 12-14. The newest run is named
// by the site's own page; the picture states its valid period.
class AbpgExtended : public Window {
public:
    explicit AbpgExtended(Window * parent);

private:
    void reload();
    void closeEventCustom() override { closed = true; }

    VBox box;
    ControlRow row;
    ComboBox comboPeriod;
    ComboBox comboProduct;
    Text textValid;
    ZoomImage image;
    int generation{0};
    bool closed{false};
};

#endif  // ABPGEXTENDED_H
