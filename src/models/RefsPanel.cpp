// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "models/RefsPanel.h"
#include "models/UtilityRefs.h"

RefsPanel::RefsPanel(Window * owner)
    : comboField{owner, UtilityRefs::fieldLabels()}
    , image{owner}
{
    box.addWidget(comboField);
    box.addWidgetReal(&image, 1);
}

void RefsPanel::addTo(HBox& row) {
    row.addLayout(box, 1);
}

void RefsPanel::setImage(const QByteArray& bytes) {
    image.setBytes(bytes);
}

void RefsPanel::setImageKeepView(const QByteArray& bytes) {
    image.setBytesKeepView(bytes);
}

int RefsPanel::fieldIndex() const {
    return comboField.getIndex();
}

void RefsPanel::setFieldIndex(int index) {
    comboField.setIndex(static_cast<size_t>(index));
}

ComboBox& RefsPanel::fieldCombo() {
    return comboField;
}

ZoomImage& RefsPanel::imageView() {
    return image;
}
