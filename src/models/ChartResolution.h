// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef CHARTRESOLUTION_H
#define CHARTRESOLUTION_H

#include <functional>
#include <string>
#include <QComboBox>

// How large the model charts are drawn, one choice for every screen that draws them (the Model Viewer, its NWS analysis and 15 minute windows): the width of the picture, 1100 pixels as it
// always was or larger. A larger chart is drawn slower and takes more memory (a loop of them most), zooms deeper, and is what the screen saves, picture or loop. Kept as MODEL_RESOLUTION.
namespace ChartResolution {
    double scale();                       // the picture's size as a multiple of the 1100 pixel one
    std::string keyPart();                // "" at 1100 pixels, else what tells a picture drawn at another size apart (for a cache)
    // the choice as a box in the screen's row: kept when changed, then `changed` is called. Every box made follows the others (they read the same preference when made).
    QComboBox * makeBox(QWidget * parent, const std::function<void()>& changed);
}

#endif  // CHARTRESOLUTION_H
