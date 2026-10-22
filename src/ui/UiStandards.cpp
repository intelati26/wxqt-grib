// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "ui/UiStandards.h"
#include <QComboBox>
#include <QLabel>
#include <QPalette>
#include <QPushButton>
#include <QSizePolicy>
#include "settings/UIPreferences.h"

QPushButton * UiStandards::button(QWidget * parent, const QString& label, const QString& tip) {
    auto * button = new QPushButton{label, parent};
    button->setToolTip(tip.isEmpty() ? label : tip);
    button->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
    return button;
}

QComboBox * UiStandards::combo(QWidget * parent) {
    auto * box = new QComboBox{parent};
    box->setMaxVisibleItems(UIPreferences::comboBoxSize);
    box->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    return box;
}

void UiStandards::muted(QLabel * label) {
    label->setForegroundRole(QPalette::PlaceholderText);
}
