// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "ui/UiStandards.h"
#include <QColor>
#include <QComboBox>
#include <QFrame>
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

void UiStandards::hoverPill(QLabel * label, int alpha) {
    label->setStyleSheet(QString{"QLabel { background-color: rgba(15, 15, 15, %1); color: #f2f2f2; padding: 4px 8px; border-radius: 3px; }"}.arg(alpha));
}

void UiStandards::notice(QLabel * label) {
    label->setStyleSheet("QLabel { background: #fff3cd; color: #664d03; padding: 4px 8px; border-radius: 3px; }");
}

void UiStandards::working(QLabel * label) {
    label->setStyleSheet("QLabel { background-color: rgba(20, 60, 120, 225); color: #ffffff; padding: 5px 10px; border-radius: 4px; }");
}

void UiStandards::error(QLabel * label) {
    label->setStyleSheet("QLabel { color: #b00020; }");
}

void UiStandards::card(QFrame * frame, const QColor& edge) {
    frame->setFrameShape(QFrame::StyledPanel);
    frame->setObjectName("stormCard");
    frame->setStyleSheet("QFrame#stormCard { border: 1px solid #999; border-left: 6px solid " + edge.name() + "; }");
}
