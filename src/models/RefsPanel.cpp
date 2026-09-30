// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "models/RefsPanel.h"
#include <QLineEdit>
#include <QSignalBlocker>
#include "models/UtilityRefs.h"

RefsPanel::RefsPanel(Window * owner)
    : comboField{owner, UtilityRefs::fieldLabels()}
    , entryThreshold{owner}
    , image{owner}
{
    entryThreshold.getView()->setMaximumWidth(110);
    entryThreshold.getView()->setToolTip("Exceedance threshold - press Enter to apply");
    rowHeader.addWidget(comboField, 1);
    rowHeader.addWidget(entryThreshold);
    box.addLayout(rowHeader);
    onFieldChanged();
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

double RefsPanel::threshold() const {
    bool ok = false;
    const auto value = QString::fromStdString(entryThreshold.getText()).trimmed().toDouble(&ok);
    return ok ? value : std::nan("");
}

void RefsPanel::onFieldChanged() {
    const auto index = fieldIndex();
    const auto uses = UtilityRefs::usesThreshold(index);
    auto * view = entryThreshold.getView();
    view->setVisible(uses);
    if (uses) {
        const QSignalBlocker blocker{view};
        view->setText(QString::number(UtilityRefs::defaultThreshold(index), 'g', 6));
        view->setPlaceholderText(QString::fromStdString(UtilityRefs::thresholdUnits(index)));
    }
}

void RefsPanel::connectThreshold(const std::function<void()>& fn) {
    QObject::connect(entryThreshold.getView(), &QLineEdit::editingFinished, entryThreshold.getView(), fn);
}

ComboBox& RefsPanel::fieldCombo() {
    return comboField;
}

ZoomImage& RefsPanel::imageView() {
    return image;
}
