// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "models/ChartResolution.h"
#include <algorithm>
#include <vector>
#include "util/Utility.h"

namespace {
    struct Choice {
        const char * label;
        double scale;
    };
    const std::vector<Choice>& choices() {
        static const std::vector<Choice> list{
            {"Chart size: 1100 px", 1.0},
            {"Chart size: 1650 px", 1.5},
            {"Chart size: 2200 px", 2.0},
            {"Chart size: 3300 px (slow)", 3.0},
        };
        return list;
    }
    int indexNow() {
        return std::clamp(Utility::readPrefInt("MODEL_RESOLUTION", 0), 0, static_cast<int>(choices().size()) - 1);
    }
}

double ChartResolution::scale() {
    return choices()[static_cast<size_t>(indexNow())].scale;
}

std::string ChartResolution::keyPart() {
    return indexNow() == 0 ? std::string{} : "|res" + std::to_string(indexNow());
}

QComboBox * ChartResolution::makeBox(QWidget * parent, const std::function<void()>& changed) {
    auto * box = new QComboBox{parent};
    for (const auto& choice : choices()) {
        box->addItem(choice.label);
    }
    box->setCurrentIndex(indexNow());
    box->setToolTip("How large the charts are drawn: the width of the picture. Larger is drawn slower and takes more memory (a loop of them most), zooms deeper, and is what is saved, as a picture or a loop. The Model Viewer and its NWS analysis and 15 minute windows share this choice.");
    QObject::connect(box, &QComboBox::currentIndexChanged, parent, [box, changed] (int index) {
        if (index >= 0) {
            Utility::writePref("MODEL_RESOLUTION", std::to_string(index));
            changed();
        }
    });
    return box;
}
