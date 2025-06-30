// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "Entry.h"

Entry::Entry(Window * parent)
    : parent{parent}
    , entry{new QLineEdit{parent}}
{}

void Entry::connect(const function<void()>& fn) {
    QObject::connect(entry, &QLineEdit::textChanged, parent, fn);
}

void Entry::setText(const string& s) {
    entry->setText(QString::fromStdString(s));
}

string Entry::getText() const {
    return entry->text().toStdString();
}

QLineEdit * Entry::getView() {
    return entry;
}
