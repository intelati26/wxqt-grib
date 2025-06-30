// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "ui/Button.h"
#include "common/GlobalVariables.h"
#include "ui/ButtonIcon.h"
#include "ui/IconMapping.h"

Button::Button(Window * parent, Icon icon, const string& label)
    : parent{parent}
    , button{new QPushButton{parent}}
{
    button->setToolTip(QString::fromStdString(label));
    if (icon != None) {
        ButtonIcon(button, GlobalVariables::imageDir + IconMapping::toString(icon));
    }
    if (!label.empty() && icon == None) {
        button->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
        button->setText(QString::fromStdString(label));
    }
}

QPushButton * Button::getView() {
    return button;
}

void Button::connect(const function<void()>& fn) {
    QObject::connect(button, &QPushButton::released, parent, fn);
}

void Button::setText(const string& s) {
    button->setText(QString::fromStdString(s));
}

string Button::getText() const {
    return button->text().toStdString();
}

void Button::setVisible(bool b) {
    button->setVisible(b);
}
