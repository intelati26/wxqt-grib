// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "ui/ButtonFlat.h"
#include "common/GlobalVariables.h"
#include "settings/UIPreferences.h"
#include "ui/ButtonIcon.h"

int ButtonFlat::iconSize{};

ButtonFlat::ButtonFlat(Window * parent, const string& imageName, const string& label)
    : parent{parent}
    , button{new QPushButton{parent}}
{
    iconSize = UIPreferences::toolbarIconSize;
    button->setFlat(true);
    button->setToolTip(QString::fromStdString(label));
    button->setContentsMargins(0, 0, 0, 0);
    if (!imageName.empty()) {
        ButtonIcon(button, GlobalVariables::imageDir + imageName, iconSize);
    }
}

QPushButton * ButtonFlat::getView() {
    return button;
}

void ButtonFlat::connect(const function<void()>& fn) {
    QObject::connect(button, &QPushButton::released, parent, fn);
}

void ButtonFlat::setText(const string& s) {
    button->setText(QString::fromStdString(s));
}

void ButtonFlat::setVisible(bool b) {
    button->setVisible(b);
}

void ButtonFlat::refresh() {
    iconSize = UIPreferences::toolbarIconSize;
    button->setIconSize(QSize{iconSize, iconSize});
}

int ButtonFlat::getIconSize() {
    return iconSize;
}
