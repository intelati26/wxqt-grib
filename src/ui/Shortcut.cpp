// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "ui/Shortcut.h"

Shortcut::Shortcut(const QKeySequence& key, QWidget * parent)
    : parent{parent}
    , qshortcut{new QShortcut{key, parent}}
{}

QShortcut * Shortcut::get() {
    return qshortcut;
}

void Shortcut::connect(const function<void()>& fn) {
    QObject::connect(get(), &QShortcut::activated, parent, fn);
}
